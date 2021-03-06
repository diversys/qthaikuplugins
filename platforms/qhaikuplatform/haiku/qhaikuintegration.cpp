/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Copyright (C) 2015-2017 Gerasim Troeglazov,
** Contact: 3dEyes@gmail.com
**
** This file is part of the plugins of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qhaikuintegration.h"
#include "qhaikuwindow.h"
#include "qhaikucommon.h"
#include "qhaikutheme.h"

#include <QtEventDispatcherSupport/private/qgenericunixeventdispatcher_p.h>
#include <qpa/qplatformfontdatabase.h>
#include <QtFontDatabaseSupport/private/qgenericunixfontdatabase_p.h>
#include <QtFontDatabaseSupport/private/qfreetypefontdatabase_p.h>
#include <QtGui/private/qpixmap_raster_p.h>
#include <QtGui/private/qguiapplication_p.h>

#include <private/qsimpledrag_p.h>

#include <qpa/qplatformservices.h>
#include <qpa/qplatformopenglcontext.h>

#include "qhaikuclipboard.h"
#include "qhaikuservices.h"
#include "qhaikuplatformfontdatabase.h"

#if !defined(QT_NO_OPENGL)
#include <GLView.h>
#endif

QT_BEGIN_NAMESPACE

template <typename BaseEventDispatcher>
class QHaikuEventDispatcher : public BaseEventDispatcher
{
public:
    explicit QHaikuEventDispatcher(QObject *parent = 0)
        : BaseEventDispatcher(parent)
    {
    }

    bool processEvents(QEventLoop::ProcessEventsFlags flags)
    {
        bool didSendEvents = BaseEventDispatcher::processEvents(flags);

        return QWindowSystemInterface::sendWindowSystemEvents(flags) || didSendEvents;
    }

    bool hasPendingEvents()
    {
        return BaseEventDispatcher::hasPendingEvents()
            || QWindowSystemInterface::windowSystemEventsQueued();
    }

    void flush()
    {
        if (qApp)
            qApp->sendPostedEvents();
        BaseEventDispatcher::flush();
    }
};

QHaikuIntegration::QHaikuIntegration(const QStringList &parameters, int &argc, char **argv)
	: QPlatformIntegration()
	, m_clipboard(0)
    , m_drag(new QSimpleDrag())
    , m_services(new QHaikuServices)
{
    m_fontDatabase.reset(new QHaikuPlatformFontDatabase());
    screenAdded(new QHaikuScreen);
}

QHaikuIntegration::~QHaikuIntegration()
{
    delete m_drag;
	delete m_clipboard;
}

bool QHaikuIntegration::hasCapability(QPlatformIntegration::Capability cap) const
{
    switch (cap) {
    case ThreadedPixmaps: return true;
    case OpenGL: return true;
    case ThreadedOpenGL: return true;
    case MultipleWindows: return true;
  //  case RasterGLSurface: return true;
    case AllGLFunctionsQueryable: return true;

    default: return QPlatformIntegration::hasCapability(cap);
    }
}

QPlatformWindow *QHaikuIntegration::createPlatformWindow(QWindow *window) const
{
    Q_UNUSED(window);
    QPlatformWindow *w = new QHaikuWindow(window);
    w->requestActivateWindow();
    return w;
}

QStringList QHaikuIntegration::themeNames() const
{
    return QStringList(QHaikuTheme::name());
}

QPlatformTheme *QHaikuIntegration::createPlatformTheme(const QString &name) const
{
    if (name == QHaikuTheme::name())
        return new QHaikuTheme(this);
    return NULL;
}

QPlatformBackingStore *QHaikuIntegration::createPlatformBackingStore(QWindow *window) const
{
    return new QHaikuBackingStore(window);
}

#if !defined(QT_NO_OPENGL)
QPlatformOpenGLContext *QHaikuIntegration::createPlatformOpenGLContext(QOpenGLContext *context) const
{
	// Ugly temporary solution for opengl applications blacklisting
	QStringList blaskList;
	blaskList << "qupzilla" << "otter-browser" << "otter" << "browser";
	QString appName = QCoreApplication::applicationName().remove("_x86");
   	if (blaskList.contains(appName, Qt::CaseInsensitive))
   		return NULL;

   	return new QHaikuGLContext(context);
}
#endif

QAbstractEventDispatcher *QHaikuIntegration::createEventDispatcher() const
{
    return createUnixEventDispatcher();
}

QPlatformFontDatabase *QHaikuIntegration::fontDatabase() const
{
    return m_fontDatabase.data();
}

QPlatformDrag *QHaikuIntegration::drag() const
{
    return m_drag;
}

QPlatformClipboard *QHaikuIntegration::clipboard() const
{
    if (!m_clipboard)
        m_clipboard = new QHaikuClipboard;
        
    return m_clipboard;
}

QPlatformServices *QHaikuIntegration::services() const
{
    return m_services;
}

#if !defined(QT_NO_OPENGL)
QHaikuGLContext::QHaikuGLContext(QOpenGLContext *context)
	: QPlatformOpenGLContext()  
{
	d_format = context->format();

    if (d_format.renderableType() == QSurfaceFormat::DefaultRenderableType)
        d_format.setRenderableType(QSurfaceFormat::OpenGL);
    if (d_format.renderableType() != QSurfaceFormat::OpenGL)
        return;

	glview = new BGLView(BRect(0,0,640,480), "bglview", B_FOLLOW_NONE, 0, BGL_RGB);
	qDebug() << "QHaikuGLContext";
}

QHaikuGLContext::~QHaikuGLContext()
{
	qDebug() << "~QHaikuGLContext";
//	delete glview;
}

void (*QHaikuGLContext::getProcAddress(const QByteArray &procName)) ()
{
	qDebug() << procName.constData();
	return (void (*)())glview->GetGLProcAddress(procName.constData());
}


bool QHaikuGLContext::makeCurrent(QPlatformSurface *surface)
{
	QSize size = surface->surface()->size();
	QHaikuWindow *window = static_cast<QHaikuWindow *>(surface);
    if (!window)
        return false;
        	
	if (window->m_window->fGLView == NULL) {
		window->m_window->fGLView = glview;
		window->m_window->Lock();
		window->m_window->AddChild(glview);
		glview->ResizeTo(size.width(), size.height());
		glViewport(0, 0, size.width(), size.height());
		window->m_window->Unlock();
	}
	
//	QPlatformOpenGLContext::makeCurrent(surface);
//	window->m_window->fGLView->LockGL();
	return true;
}

void QHaikuGLContext::doneCurrent()
{
//	QPlatformOpenGLContext::doneCurrent();
//	glview->UnlockGL();
}

void QHaikuGLContext::swapBuffers(QPlatformSurface *surface)
{
	QHaikuWindow *window = static_cast<QHaikuWindow *>(surface);

	//window->m_window->fGLView->LockGL();
	glview->SwapBuffers();
	//window->m_window->fGLView->UnlockGL();
}

QSurfaceFormat QHaikuGLContext::format() const
{
    return d_format;
}

bool QHaikuGLContext::isSharing() const
{
    return false;
}

bool QHaikuGLContext::isValid() const
{
    return true;
}

#endif

QT_END_NAMESPACE
