#include "qdispgraphic.h"
#include "paramlevelconfig.h"
#include "ui_qdispgraphic.h"
#include "dbopt.h"
#include <QPointF>
#include <QVector>
#include <QSqlRecord>
#include <QSqlQuery>
#include  <QPrinter>
#include <QPrintDialog>

QDispgraphic::QDispgraphic(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::QDispgraphic),
    mMarkerMax(NULL),
    mMarkerMin(NULL),
    mPicker(NULL),
    mGrid(NULL),
    mScaleDraw(NULL),
    mScaleEngine(NULL),
    mDisPlot(NULL),mPanner(NULL)
{
    ui->setupUi(this);
    mModel = new QSqlTableModel();
    mModel->setTable("AssayData");
    mModel->setEditStrategy(QSqlTableModel::OnManualSubmit);
    mModel->select();
    InitPlot ();
    InitCurve("Ch4", Qt::black);
    queryData("Ch4");

    QSqlQuery query;
    query.exec("SELECT * from AssayData");
    while(query.next ()) {
        QString value = query.value("DeviceId").toString ();
        if (ui->comboBox_DevIndex->findText (value) < 0 ) {
            ui->comboBox_DevIndex->addItem (value);
        }
        value = query.value("PipeId").toString ();
        if (ui->comboBox_PipeIndexEnd->findText (value) < 0 ) {
            ui->comboBox_PipeIndexEnd->addItem (value);
        }
        if (ui->comboBox_PipeIndexStart->findText(value) < 0 ) {
            ui->comboBox_PipeIndexStart->addItem (value);
        }
    }
    query.exec("SELECT MAX(AssayTime), MIN(AssayTime) FROM AssayData");
    if(query.next ()) {
        ui->dateTimeEdit_Start->setDateTime(
            QDateTime::fromString (
                query.value ("MIN(AssayTime)").toString (),
                QString("yyyy/MM/dd/hh:mm:ss"))
        );
        ui->dateTimeEdit_End->setDateTime(
            QDateTime::fromString (
                query.value ("MAX(AssayTime)").toString (),
                QString("yyyy/MM/dd/hh:mm:ss"))
        );
    }
    query.exec("SELECT * from FriendlyName");
    while(query.next ()) {
        if (query.value ("RangeMax").toString ().size ()) {
            ui->comboBox->addItem (query.value ("FriendlyName").toString ());
        }
    }
    FilterQuery();
    connect(ui->checkBox_DevIndex,
            SIGNAL(clicked()),
            this,
            SLOT(FilterQuery()));
    connect(ui->comboBox_DevIndex,
            SIGNAL(currentIndexChanged(QString)),
            this,
            SLOT(FilterQuery()));
    connect(ui->checkBox_StartEndDate,
            SIGNAL(clicked()),
            this,
            SLOT(FilterQuery()));
    connect(ui->dateTimeEdit_End,
            SIGNAL(dateTimeChanged(QDateTime)),
            this,
            SLOT(FilterQuery()));
    connect(ui->dateTimeEdit_Start,
            SIGNAL(dateTimeChanged(QDateTime)),
            this,
            SLOT(FilterQuery()));
    connect(ui->checkBox_PipeIndex,
            SIGNAL(clicked()),
            this,
            SLOT(FilterQuery()));
    connect(ui->comboBox_PipeIndexStart,
            SIGNAL(currentTextChanged(QString)),
            this,
            SLOT(FilterQuery()));
    connect(ui->comboBox_PipeIndexEnd,
            SIGNAL(currentTextChanged(QString)),
            this,
            SLOT(FilterQuery()));
    connect(ui->comboBox,
            SIGNAL(currentIndexChanged(QString)),
            this,
            SLOT(DisplayChange(QString)),
            Qt::QueuedConnection);
    connect(ui->pushButton_print,
            SIGNAL(clicked()),
            this,
            SLOT(printFunc()));
    connect(ui->spinBox_Max,
            SIGNAL(valueChanged(double)),
            this,
            SLOT(MaxMarkervalueChanged(double)));
    connect(ui->spinBox_Min,
            SIGNAL(valueChanged(double)),
            this,
            SLOT(MinMarkervalueChanged(double)));
    connect(ui->pushButton_Config,
            SIGNAL(clicked()),
            this,
            SLOT(ConfigParamLevel()));
}
QDispgraphic::~QDispgraphic()
{
    delete ui;
    QList<DispCurve*>::iterator it;
    for (it=mCurveList.begin (); it!=mCurveList.end (); it++) {
        delete *it;
    }
    if(mMarkerMax) {
        delete mMarkerMax;
    }
    if(mMarkerMin) {
        delete mMarkerMin;
    }
    if(mPicker) {
        delete mPicker;
    }
    if(mGrid) {
        delete mGrid;
    }
    if(mPanner) {
        delete mPanner;
    }
}

void QDispgraphic::InitPlot()
{
    ui->qwtPlot->setAutoFillBackground( true );
    ui->qwtPlot->setPalette( palette ());
    ui->qwtPlot->setAutoReplot( true );

    QwtPlotCanvas *canvas = new QwtPlotCanvas();
    canvas->setLineWidth( 0 );
    canvas->setFrameStyle( QFrame::Box | QFrame::Plain );
    canvas->setBorderRadius( 10 );
//    canvas->setLegendAttribute( QwtPlotCurve::LegendShowLine );

//    QPalette canvasPalette( Qt::gray);
//    canvasPalette.setColor( QPalette::Foreground, QColor( 133, 190, 232 ) );
//    canvasPalette.setColor( palette ());
//    canvas->setPalette(canvasPalette);
    canvas->setPalette( palette ());
    ui->qwtPlot->setCanvas( canvas );

    // panning with the left mouse button
    mPanner =  new QwtPlotPanner( canvas );

    // zoom in/out with the wheel
    mDisPlot = new DisPlotMagnifier( canvas );

    ui->qwtPlot->setAxisTitle( QwtPlot::xBottom, "X-->" );
    ui->qwtPlot->setAxisScale( QwtPlot::xBottom, 0,20);
    ui->qwtPlot->setAxisScale( QwtPlot::xBottom, 0, mModel->rowCount ());
    ui->qwtPlot->setAxisLabelRotation( QwtPlot::xBottom, -20.0 );
    ui->qwtPlot->setAxisLabelAlignment( QwtPlot::xBottom, Qt::AlignLeft | Qt::AlignBottom );

    ui->qwtPlot->setAxisTitle( QwtPlot::yLeft, "Y-->" );
    ui->qwtPlot->setAxisScale( QwtPlot::yLeft, 0, 1000);

    // grid
    mGrid = new QwtPlotGrid();
    mGrid->enableXMin( true );
    mGrid->setMajorPen( Qt::darkGray, 2, Qt::DotLine );
    mGrid->setMinorPen( Qt::lightGray, 2 , Qt::DotLine );
    mGrid->attach(ui->qwtPlot);
    // marker
    mMarkerMax = new QwtPlotMarker();
    mMarkerMax->setValue( 0.0, 0.0 );
    QwtText Lable("Max 0.0");
    mMarkerMax->setLabel (Lable);
    mMarkerMax->setLineStyle( QwtPlotMarker::HLine );
    mMarkerMax->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
    mMarkerMax->setLinePen( Qt::red, 1, Qt::SolidLine );
    mMarkerMax->attach(ui->qwtPlot);
    // marker
    mMarkerMin = new QwtPlotMarker();
    QwtText minLable("Min 0.0");
    mMarkerMin->setLabel (minLable);
    mMarkerMin->setValue( 0.0, 0.0 );
    mMarkerMin->setLineStyle( QwtPlotMarker::HLine );
    mMarkerMin->setLabelAlignment( Qt::AlignRight | Qt::AlignBottom );
    mMarkerMin->setLinePen( Qt::darkYellow, 1, Qt::SolidLine );
    mMarkerMin->attach(ui->qwtPlot);

    //Picker
    mPicker = new Picker(ui->qwtPlot->canvas ());

    mScaleDraw = new DateScaleDraw( Qt::UTC);
    mScaleEngine = new DateScaleEngine( Qt::UTC);

    ui->qwtPlot->axisScaleDraw (QwtPlot::xBottom);
    ui->qwtPlot->setAxisScaleDraw( QwtPlot::xBottom, mScaleDraw );
    ui->qwtPlot->setAxisScaleEngine(QwtPlot::xBottom, mScaleEngine );
    ui->qwtPlot->setAxisLabelAlignment( QwtPlot::xBottom, Qt::AlignLeft | Qt::AlignBottom );

}

void QDispgraphic::MaxMarkervalueChanged(double value)
{
    if(!mMarkerMax) {
        return;
    }
    mMarkerMax->setValue( 0.0, value);
}
void QDispgraphic::MinMarkervalueChanged(double value)
{
    if(!mMarkerMin) {
        return;
    }
    mMarkerMin->setValue( 0.0, value);
}
void QDispgraphic::ConfigParamLevel()
{
    ParamLevelConfig  config;
    config.exec ();
}
void QDispgraphic::printFunc()
{
    QPrinter printer( QPrinter::HighResolution );

    QString docName = ui->qwtPlot->title().text();
    if ( !docName.isEmpty() ) {
        docName.replace ( QRegExp ( QString::fromLatin1 ( "\n" ) ), tr ( " -- " ) );
        printer.setDocName ( docName );
    }

    printer.setCreator( "Platform");
    printer.setOrientation( QPrinter::Landscape );

    QPrintDialog dialog( &printer );
    if ( dialog.exec() ) {
        QwtPlotRenderer renderer;

        if ( printer.colorMode() == QPrinter::GrayScale ) {
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasBackground );
            renderer.setDiscardFlag( QwtPlotRenderer::DiscardCanvasFrame );
            renderer.setLayoutFlag( QwtPlotRenderer::FrameWithScales );
        }

        renderer.renderTo(ui->qwtPlot, printer );
    }
}

void QDispgraphic::queryData(QString name)
{
    QVector<QPointF> temp;
    int maxY = 0;
    int minY = 0;
    int count = 0;
    QSqlQuery query;
    query.exec("SELECT MAX(AssayTime), MIN(AssayTime) FROM AssayData");
    if(query.next ()) {
        double minAssayTime = 0;
        double maxAssayTime = 0;
        minAssayTime = QwtDate::toDouble(QDateTime::fromString (
                                             query.value ("MIN(AssayTime)").toString (),"yyyy/MM/dd/hh:mm:ss")
                                        );
        maxAssayTime = QwtDate::toDouble (QDateTime::fromString (
                                              query.value ("MAX(AssayTime)").toString (),"yyyy/MM/dd/hh:mm:ss")
                                         );
        ui->qwtPlot->setAxisScale( QwtPlot::xBottom,
                                   minAssayTime, maxAssayTime);
    }

    while((count) < mModel->rowCount ()) {
        double Y = mModel->record (count).value (name).toDouble ();
        double IntX =QwtDate::toDouble (QDateTime::fromString (
                                            mModel->record (count).value ("AssayTime").toString (),"yyyy/M/d/h:m:s"));
        temp.push_back (QPointF(IntX, Y));
        Y >= maxY ? maxY = Y : maxY = maxY;
        Y <= minY ? minY = Y : minY = minY;
        if (mModel->canFetchMore ()) {
            mModel->fetchMore ();
        }
        count++;
    }
    ui->qwtPlot->setAxisScale( QwtPlot::yLeft,(double)minY*12.0/10.0, (double)maxY*12.0/10.0);
    if (mDisPlot) {
        mDisPlot->setAxisScale((double)minY*12.0/10.0, (double)maxY*12.0/10.0);
    }
    QList<DispCurve*>::iterator it;
    for (it=mCurveList.begin (); it!=mCurveList.end (); it++) {
        if (((DispCurve*)*it)->title().text() == name) {
            ((DispCurve*)*it)->setSamples (temp);
        }

    }
    ui->qwtPlot->replot();
}
void QDispgraphic::DisplayChange(QString index)
{
    QString sql = QString("SELECT * FROM FriendlyName Where Name='")+
                  GetTableColumByFriendName(index) + QString("'");
    QSqlQuery query;
    query.exec(sql);
    if (query.next ()) {
        ui->spinBox_Max->setValue (query.value ("RangeMax").toDouble ());
        MaxMarkervalueChanged (query.value ("RangeMax").toDouble ());
        ui->spinBox_Min->setValue (query.value ("RangeMin").toDouble ());
        MinMarkervalueChanged (query.value ("RangeMin").toDouble ());
    }
    FilterQuery ();
}

void QDispgraphic::legendChecked( const QVariant &itemInfo, bool on )
{
    QwtPlotItem *plotItem = ui->qwtPlot->infoToItem( itemInfo );
    if ( plotItem )
        showCurve( plotItem, on );
}

void QDispgraphic::showCurve( QwtPlotItem *item, bool on )
{
    item->setVisible( on );

    QwtLegend *lgd = qobject_cast<QwtLegend *>( ui->qwtPlot->legend() );

    QList<QWidget *> legendWidgets =
        lgd->legendWidgets( ui->qwtPlot->itemToInfo( item ) );

    if ( legendWidgets.size() == 1 ) {
        QwtLegendLabel *legendLabel =
            qobject_cast<QwtLegendLabel *>( legendWidgets[0] );

        if ( legendLabel )
            legendLabel->setChecked( on );
    }

    ui->qwtPlot->replot();
}
void QDispgraphic::InitCurve(QString name, QColor color)
{
    DispCurve *curve = NULL;
    curve = new DispCurve(name);
//    curve->setStyle (DispCurve::Sticks);
    curve->setStyle (DispCurve::Lines);
//    curve->setCurveAttribute(DispCurve::Fitted);
    curve->setLegendAttribute( QwtPlotCurve::LegendShowLine );
    curve->setSymbol( new QwtSymbol( QwtSymbol::Star2, Qt::NoBrush,
                                     QPen( Qt::red ), QSize( 7, 7 ) ) );
    curve->setPen(color);
    curve->setZ( curve->z() - 2 );
    curve->attach(ui->qwtPlot);
    mCurveList.push_back (curve);
}
void QDispgraphic::FilterQuery()
{
    QString filter;
    if (ui->checkBox_DevIndex->isChecked ()) {
        filter = QString("DeviceId ='") + ui->comboBox_DevIndex->currentText ()+ "'";
    }
    //TODO filter by datetime
    if (ui->checkBox_StartEndDate->isChecked ()) {
        if(filter.size ()) {
            filter+= QString(" and ");
        }
        filter += QString ("AssayTime>='") +
                  ui->dateTimeEdit_Start->dateTime().toString ("yyyy/MM/dd/hh:mm:ss")+
                  QString("' and AssayTime<='") +
                  ui->dateTimeEdit_End->dateTime().toString ("yyyy/MM/dd/hh:mm:ss") +
                  QString("'");
    }
    if (ui->checkBox_PipeIndex->isChecked ()) {
        if(filter.size ()) {
            filter+= QString(" and ");
        }
        filter += QString("PipeID>='") +
                  ui->comboBox_PipeIndexStart->currentText ()+
                  "' and PipeID<='" +
                  ui->comboBox_PipeIndexEnd->currentText ()+
                  QString("'");
    }
    QString GraphicLable;
    GraphicLable =QString("X-->\n") + ui->comboBox->currentText () +QString(" Filter: ") +filter;

    ui->qwtPlot->setAxisTitle( QwtPlot::xBottom, GraphicLable );
    qDebug() <<"Filter:" << filter;
    mModel->setFilter(filter + QString(" ORDER BY AssayTime"));
    mModel->setFilter(filter);
    mModel->select ();
    queryData (GetTableColumByFriendName(ui->comboBox->currentText ()));
}
