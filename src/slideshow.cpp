#include "slideshow.h"

/**
 * @brief SlideShow::SlideShow
 * @param parent
 * @param dir Directory with photos
 * @param speed How many seconds between photos
 */
SlideShow::SlideShow(QList<QDir> *dirs, unsigned int speed, QObject *parent) : QObject(parent)
{
    _dirs = dirs;
    _speed = speed * 1000;
    _last = 0;
    _current = 0;
    _pause = false;
    _dirs_valid = false;
    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(nextImage()));
    timer->setInterval(_speed);
    timer->setSingleShot(false); // timer fires continuously
    qsrand(QDateTime::currentDateTime().toTime_t ());
    pathScanner = new PathScanner();
    connect(pathScanner, SIGNAL(finished(QStringList*)), this, SLOT(initDone(QStringList*)));
    scanningActive = false;
}

void SlideShow::setDirs(QList<QDir> *dirs)
{
    // check if list changed:
    bool equal = true;
    if (_dirs->count() == dirs->count()) {
        // size is the same -> could be equal
        QListIterator<QDir> dirs1(*_dirs);
        QListIterator<QDir> dirs2(*dirs);
        while (dirs1.hasNext()) {
            if (dirs1.next().absolutePath() != dirs2.next().absolutePath()) {
                // not the same elements -> unequal
                equal = false;
                break;
            }
        }
    } else {
        equal = false;
    }

    if (!equal) {
        _dirs = dirs;
        init();
    }
}

void SlideShow::setSpeed(unsigned int speed)
{
    _speed = speed * 1000;
    timer->setInterval(_speed);
}

void SlideShow::init(void)
{
  emit initStart();
  scanningActive = true;
  qDebug() << "[SlideShow] Initializing";

  _last = 0;
  _pause = false;
  timer->stop();
  _previous_images.clear();

  pathScanner->setPaths(_dirs);
  pathScanner->start();
}

void SlideShow::initDone(QStringList *images)
{
  _images = images;
  nextImage();
  emit initStop();
  scanningActive = false;
  timer->start();
}

void SlideShow::nextImage(void)
{
    if (_images->size() == 0) {
        return;
    }
    // we are going back in the list and want to show the next image in the list
    if (_previous_images.size() != 0 && _current < (unsigned int) _previous_images.size()) {
        ++_current;
        _current_path = _images->at(_previous_images.at(_current - 1));
    // we are at the end of the list and want to pick a random image
    } else {
      unsigned int random = _last;
      while (_last == random) {
          random = qrand() % _images->size();
      }
      _last = random;
      _current_path = _images->at(random);
      _previous_images.append(random);
      ++_current;
    }
    loadImage(_current_path, 0);
}

void SlideShow::nextImageClicked(void)
{
    // TODO: why pause? config?
    if (!_pause) {
        emit communicatePause();
    }
    nextImage();
}

void SlideShow::previousImageClicked(void)
{
    if (_previous_images.size() < 2 || _current < 2)
        return;

    if (!_pause) {
        emit communicatePause();
    }
    --_current;

    _current_path = _images->at(_previous_images.at(_current-1));
    loadImage(_current_path, 0);
}

void SlideShow::pause(void)
{
    _pause = !_pause;
    if (_pause) {
        timer->stop();
    } else {
        nextImage();
        timer->start();
    }
}

void SlideShow::imageClicked(void) {
    if (scanningActive)
        return;
    QString clickSetting = _settingsManager->readSetting(SETTING_ON_CLICK_ACTION).toString();
    if (clickSetting == SETTING_ON_CLICK_ACTION_NOTHING) {
        return;
    }
    if (!_dirs_valid || _images->size() == 0) {
        return;
    }
    if (clickSetting == SETTING_ON_CLICK_ACTION_PAUSE) {
        emit communicatePause();
        return;
    }
    // else SETTING_ON_CLICK_OPEN_FOLDER
    if (!_pause) {
        emit communicatePause();
    }
    QString path = QDir::toNativeSeparators(QFileInfo(_current_path).absolutePath());
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SlideShow::loadImage(QString path, int direction)
{
  // TODO: read EXIF data and rotate if necessary
  QPixmap pix(path);
  pix = pix.transformed(QTransform().rotate(direction));
  emit showImage(&pix);
  emit showPath(path);
}

bool SlideShow::paused(void)
{
    return _pause;
}

/**
 * @brief SlideShow::rotateCurrentImage Rotates the current image in the given direction.
 * Direction > 0 means right, direction < 0 means left.
 * @param direction > 0 means right, direction < 0 means left.
 */
void SlideShow::rotateCurrentImage(int direction)
{
    unsigned int new_orientation = _images_orientation[_current];
    if (new_orientation == 0) {
        // image was not yet rotated
        new_orientation = 90 * direction / abs(direction);
    } else {
        new_orientation = (new_orientation + (90 * direction / abs(direction)) ) % 360;
    }
    _images_orientation.insert(_current, new_orientation);
    loadImage(_images->at(_previous_images.at(_current)), new_orientation);
    qDebug() << "new orientation: " << new_orientation;
}
