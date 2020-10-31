#include "mainframe.h"
#include "ui_mainframe.h"
#include <QShortcut>
#include <QDebug>

#include <chrono>

MainFrame::MainFrame( QWidget *parent ) :
    QMainWindow( parent ),
    ui( new Ui::MainFrame )
{
    ui->setupUi( this ) ;

    ui->closeButton->hide();

    ui->imageView->setBackgroundRole( QPalette::Base );
    ui->imageView->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );
    ui->imageView->setScaledContents( false );
    ui->imageView->setAlignment( Qt::AlignCenter );

    ui->histogramView->setBackgroundRole( QPalette::Base );
    ui->histogramView->setSizePolicy( QSizePolicy::Ignored, QSizePolicy::Ignored );
    ui->histogramView->setScaledContents( false );
    ui->histogramView->setAlignment( Qt::AlignCenter );

    new QShortcut( QKeySequence( Qt::CTRL + Qt::Key_F ), this, SLOT( on_toggleFullScreenButton_clicked() ) );
    new QShortcut( QKeySequence( Qt::CTRL + Qt::Key_Q ), this, SLOT( on_closeButton_clicked() ) );

    int count = ASICamera::GetCount();
    for( int i = 0; i < count; i++ ) {
        camerasInfo.emplace_back( ASICamera::GetInfo( i ) );
    }

    for( int i = 0; i < count; i++ ) {
        ui->cameraSelectionCombo->addItem( camerasInfo[i]->Name, QVariant( i ) );
    }
}

MainFrame::~MainFrame()
{
    delete ui;
}

void MainFrame::on_closeButton_clicked()
{
    close();
}

void MainFrame::on_toggleFullScreenButton_clicked()
{
    bool isFullScreen = windowState().testFlag( Qt::WindowFullScreen );
    if( isFullScreen ) {
        ui->closeButton->hide();
        showNormal();  
    } else {
        showFullScreen();
        ui->closeButton->show();
    }
}

void MainFrame::on_exposureSlider_valueChanged( int value )
{
    // Slider position to exposure translation table (quasi logorithmic)
    static int exposures[] =
        { 1, 1, 1, 2, 2, 3, 4, 5, 6, 8,
          10, 12, 15, 20, 25, 30, 40, 50, 60, 80,
          100, 125, 150, 200, 250, 300, 400, 500, 600, 800,
          1000, 1250, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 8000,
          10000, 12500, 15000, 20000, 25000, 30000, 40000, 50000, 60000, 80000,
          100000, 125000, 150000, 200000, 250000, 300000, 400000, 500000, 600000, 800000,
          1000000, 1250000, 1500000, 2000000, 2500000, 3000000, 4000000, 5000000, 6000000, 8000000,
          10000000, 12500000, 15000000, 20000000, 25000000, 30000000, 40000000, 50000000, 60000000, 80000000,
          100000000 };

    // Set exposure in the exposure spin box with convinient time units
    int exp = exposures[value];
    if( exp >= 10000000 ) {
        // Seconds for exposures equal or above 10 s
        ui->exposureSpinBox->setSuffix( " s" );
        ui->exposureSpinBox->setValue( exp / 1000000 );
    } else if( exp >= 10000 ) {
        // Milliseconds for exposures equal or above 10 ms
        ui->exposureSpinBox->setSuffix( " ms" );
        ui->exposureSpinBox->setValue( exp / 1000 );
    } else {
        // Microseconds for exposures below 10 ms
        ui->exposureSpinBox->setSuffix( " us" );
        ui->exposureSpinBox->setValue( exp );
    }
}

void MainFrame::on_gainSlider_valueChanged( int value )
{
    ui->gainSpinBox->setValue( value );
}

void MainFrame::on_captureButton_clicked()
{
    int selectedIndex = ui->cameraSelectionCombo->currentData().toInt();

    auto exposure = ui->exposureSpinBox->value();
    auto exposureUnits = ui->exposureSpinBox->suffix();
    if( exposureUnits == " s" ) {
        exposure *= 1000000;
    } else if( exposureUnits == " ms" ) {
        exposure *= 1000;
    } else {
        assert( exposureUnits == " us" );
    }

    auto gain = ui->gainSpinBox->value();


    if( camerasInfo.size() > 0 ) {
        auto start = std::chrono::steady_clock::now();

        if( camera == nullptr ) {
            std::shared_ptr<ASI_CAMERA_INFO> cameraInfo = camerasInfo[selectedIndex];
            qDebug() << cameraInfo->Name;

            camera = ASICamera::Open( cameraInfo->CameraID );

            setWindowTitle( cameraInfo->Name );

            bool isAuto = false;
            long min, max, defaultVal;

            camera->SetExposure( exposure );
            long exposure = camera->GetExposure( isAuto );
            camera->GetExposureCaps( min, max, defaultVal );
            qDebug() << "Exposure "<< exposure << ( isAuto ? " (auto)" : "" ) <<
                "[" << min << max << defaultVal << "]";

            camera->SetGain( gain );
            long gain = camera->GetGain( isAuto );
            camera->GetGainCaps( min, max, defaultVal );
            qDebug() << "Gain " << gain << ( isAuto ? "(auto)" : "" ) <<
                "[" << min << max << defaultVal << "]";

            camera->SetWhiteBalanceR( 50 );
            camera->GetWhiteBalanceRCaps( min, max, defaultVal );
            qDebug() << "WB_R" << camera->GetWhiteBalanceR() <<
                "[" << min << max << defaultVal << "]";

            camera->SetWhiteBalanceB( 50 );
            camera->GetWhiteBalanceBCaps( min, max, defaultVal );
            qDebug() << "WB_B" << camera->GetWhiteBalanceB() <<
                "[" << min << max << defaultVal << "]";

            camera->SetOffset( 64 );
            camera->GetOffsetCaps( min, max, defaultVal );
            qDebug() << "Offset" << camera->GetOffset() <<
                "[" << min << max << defaultVal << "]";

            int width = 0;
            int height = 0;
            int bin = 0;
            ASI_IMG_TYPE imgType = ASI_IMG_END;
            camera->GetROIFormat( width, height, bin, imgType );
            imgType = ASI_IMG_RAW16;
            camera->SetROIFormat( width, height, bin, imgType );
            camera->GetROIFormat( width, height, bin, imgType );
            qDebug() << width << "x" << height << " bin" << bin;
            switch( imgType ) {
                case ASI_IMG_RAW8: qDebug() << "RAW8"; break;
                case ASI_IMG_RGB24: qDebug() << "RGB24"; break;
                case ASI_IMG_RAW16: qDebug() << "RAW16"; break;
                case ASI_IMG_Y8: qDebug() << "Y8"; break;
                default:
                    assert( false );
            }

            auto end = std::chrono::steady_clock::now();
            auto msec = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
            qDebug() << "Camera initialized in" << msec << "msec";
        }

        const ushort* raw = camera->DoExposure();

        /*FILE* out = fopen( "image.cfa", "wb" );
        fwrite( buf.data(), 1, size, out );
        fclose( out );*/

        auto end = std::chrono::steady_clock::now();
        auto msec = std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
        qDebug() << "Data ready in" << msec << "msec";

        int width = camera->GetWidth();
        int height = camera->GetHeight();

        msec = render( raw, width, height );

        qDebug() << "Rendered in " << msec << "msec";

        qDebug() << "Camera temperature is" << camera->GetTemperature();
    } else {
        qDebug() << "No camera";

        FILE* file = fopen( "image.cfa", "rb" );
        int width = 1936;
        int height = 1096;
        std::vector<ushort> raw( width * height );
        fread( raw.data(), sizeof( ushort ), width * height, file );
        fclose( file );

        auto msec = render( raw.data(), width, height );

        qDebug() << msec << "msec";
    }
}

ulong MainFrame::render( const ushort* raw, int width, int height )
{
    auto start = std::chrono::steady_clock::now();

    std::vector<uint> histR( 256 );
    std::fill( histR.begin(), histR.end(), 0 );
    std::vector<uint> histG( 256 );
    std::fill( histG.begin(), histG.end(), 0 );
    std::vector<uint> histB( 256 );
    std::fill( histG.begin(), histG.end(), 0 );

    size_t w = width / 2;
    size_t h = height / 2;
    size_t byteWidth = 3 * w;
    std::vector<uchar> pixels( byteWidth * h );
    uchar* rgb = pixels.data();
    for( size_t y = 0; y < h; y++ ) {
        const ushort* srcLine = raw + 2 * width * y;
        uchar* dstLine = rgb + byteWidth * y;
        for( size_t x = 0; x < w; x++ ) {
            const ushort* src = srcLine + 2 * x;
            uchar r = src[0] / 256;
            uchar g = src[1] / 256;
            uchar b = src[width + 1] / 256;

            uchar* dst = dstLine + 3 * x;
            dst[0] = r;
            dst[1] = g;
            dst[2] = b;

            if( r < histR.size() ) {
                histR[r]++;
            }
            if( g < histG.size() ) {
                histG[g]++;
            }
            if( b < histB.size() ) {
                histB[b]++;
            }
        }
    }

    QImage image( rgb, w, h, QImage::Format_RGB888 );
    ui->imageView->setPixmap( QPixmap::fromImage( image ) );

    renderHistogram( histR.data(), histG.data(), histB.data(), histR.size() );

    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>( end - start ).count();
}

void MainFrame::renderHistogram( const uint* r, const uint* g, const uint* b, int size )
{
    uint max = 0;
    for( int i = 0; i < size - 1; i++ ) {
        if( r[i] > max ) {
            max = r[i];
        }
        if( g[i] > max ) {
            max = g[i];
        }
        if( b[i] > max ) {
            max = b[i];
        }
    }

    int h = 128;
    int biteWidth = 3 * size;
    std::vector<uchar> hist( biteWidth * h );
    std::fill( hist.begin(), hist.end(), 0 );
    uchar* p = hist.data();
    for( int i = 0; i < size; i++ ) {
        int R = 127 - ( 127 * r[i] ) / max;
        int G = 127 - ( 127 * g[i] ) / max;
        int B = 127 - ( 127 * b[i] ) / max;
        for( int j = 0; j < h; j++ ) {
            uchar* p0 = p + j * biteWidth + 3 * i;
            if( R < 127 ) {
                if( j > R ) {
                    p0[0] = 0x50;
                } else if( j == R ){
                    p0[0] = 0xFF;
                }
            }
            if( G < 127 ) {
                if( j > G ) {
                    p0[1] = 0x50;
                } else if( j == G ){
                    p0[1] = 0xFF;
                }
            }
            if( B < 127 ) {
                if( j > B ) {
                    // Make blue on black brighter (otherwise it is too dim)
                    if( p0[0] == 0 && p0[1] == 0 ) {
                        p0[2] = 0x80;
                    } else {
                        p0[2] = 0x50;
                    }
                } else if( j == B ){
                    // Make blue outline briter
                    p0[0] = 0x80;
                    p0[1] = 0x80;
                    p0[2] = j < 127 ? 0xFF : 0;
                }
            }
        }
    }

    QImage image( p, size, h, QImage::Format_RGB888 );
    ui->histogramView->setPixmap( QPixmap::fromImage( image ) );
}
