#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"

// Settings
#include "VDSettings.h"
// Session
#include "VDSession.h"
// Log
#include "VDLog.h"
// Spout
#include "CiSpoutIn.h"
#include "CiSpoutOut.h"
// NDI
#include "CinderNDISender.h"
#include "CinderNDIReceiver.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace VideoDromm;

class _TBOX_PREFIX_App : public App {

public:
	_TBOX_PREFIX_App();
	void mouseMove(MouseEvent event) override;
	void mouseDown(MouseEvent event) override;
	void mouseDrag(MouseEvent event) override;
	void mouseUp(MouseEvent event) override;
	void keyDown(KeyEvent event) override;
	void keyUp(KeyEvent event) override;
	void fileDrop(FileDropEvent event) override;
	void update() override;
	void draw() override;
	void cleanup() override;
	void setUIVisibility(bool visible);
private:
	// Settings
	VDSettingsRef					mVDSettings;
	// Session
	VDSessionRef					mVDSession;
	// Log
	VDLogRef							mVDLog;
	// imgui
	float									color[4];
	float									backcolor[4];
	int										playheadPositions[12];
	int										speeds[12];

	float									f = 0.0f;
	char									buf[64];
	unsigned int					i, j;

	bool									mouseGlobal;

	string								mError;
	// fbo
	bool									mIsShutDown;
	Anim<float>						mRenderWindowTimer;
	void									positionRenderWindow();
	bool									mFadeInDelay;
	SpoutIn								mSpoutIn;
	SpoutOut 							mSpoutOut;
	CinderNDIReceiver 		mReceiver;
	CinderNDISender				mSender;
	ci::SurfaceRef 				mSurface;
	ci::gl::Texture2dRef	mTexture;
};


_TBOX_PREFIX_App::_TBOX_PREFIX_App()
	: mSender("VDMixer")
	, mReceiver{}
	, mSpoutOut("NDIReceiver", app::getWindowSize())
{
	mSurface = ci::Surface::create(getWindowWidth(), getWindowHeight(), true, SurfaceChannelOrder::BGRA);
	mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight());

	// Settings
	mVDSettings = VDSettings::create();
	// Session
	mVDSession = VDSession::create(mVDSettings);
	//mVDSettings->mCursorVisible = true;
	setUIVisibility(mVDSettings->mCursorVisible);
	mVDSession->getWindowsResolution();

	mouseGlobal = false;
	mFadeInDelay = true;
	// windows
	mIsShutDown = false;
	mRenderWindowTimer = 0.0f;
	timeline().apply(&mRenderWindowTimer, 1.0f, 2.0f).finishFn([&] { positionRenderWindow(); });

}
void _TBOX_PREFIX_App::positionRenderWindow() {
	mVDSettings->mRenderPosXY = ivec2(mVDSettings->mRenderX, mVDSettings->mRenderY);//20141214 was 0
	setWindowPos(mVDSettings->mRenderX, mVDSettings->mRenderY);
	setWindowSize(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight);
}
void _TBOX_PREFIX_App::setUIVisibility(bool visible)
{
	if (visible)
	{
		showCursor();
	}
	else
	{
		hideCursor();
	}
}
void _TBOX_PREFIX_App::fileDrop(FileDropEvent event)
{
	mVDSession->fileDrop(event);
}
void _TBOX_PREFIX_App::update()
{
	mVDSession->setFloatUniformValueByIndex(mVDSettings->IFPS, getAverageFps());
	mVDSession->update();
	// NDI Receive	
	mReceiver.update();

	// Spout Receive
	/* if (mSpoutIn.getSize() != app::getWindowSize()) {
		app::setWindowSize(mSpoutIn.getSize());
		mTexture = ci::gl::Texture::create(getWindowWidth(), getWindowHeight(), ci::gl::Texture::Format().loadTopDown(true));
	} */

	mTexture = mSpoutIn.receiveTexture();
	// Ndi Send
	if (mTexture) {
		mSurface = Surface::create(mTexture->createSource());
	}

	long long timecode = app::getElapsedFrames();

	XmlTree msg{ "ci_meta", mSpoutIn.getSenderName() };
	mSender.sendMetadata(msg, timecode);
	mSender.sendSurface(*mSurface, timecode);	
}
void _TBOX_PREFIX_App::cleanup()
{
	if (!mIsShutDown)
	{
		mIsShutDown = true;
		CI_LOG_V("shutdown");
		// save settings
		mVDSettings->save();
		mVDSession->save();
		quit();
	}
}
void _TBOX_PREFIX_App::mouseMove(MouseEvent event)
{
	if (!mVDSession->handleMouseMove(event)) {
		// let your application perform its mouseMove handling here
	}
}
void _TBOX_PREFIX_App::mouseDown(MouseEvent event)
{
	if (!mVDSession->handleMouseDown(event)) {
		// let your application perform its mouseDown handling here
		if (event.isRightDown()) { // Select a sender
							   // SpoutPanel.exe must be in the executable folder
			mSpoutIn.getSpoutReceiver().SelectSenderPanel(); // DirectX 11 by default
		}
	}
}
void _TBOX_PREFIX_App::mouseDrag(MouseEvent event)
{
	if (!mVDSession->handleMouseDrag(event)) {
		// let your application perform its mouseDrag handling here
	}	
}
void _TBOX_PREFIX_App::mouseUp(MouseEvent event)
{
	if (!mVDSession->handleMouseUp(event)) {
		// let your application perform its mouseUp handling here
	}
}

void _TBOX_PREFIX_App::keyDown(KeyEvent event)
{
	if (!mVDSession->handleKeyDown(event)) {
		switch (event.getCode()) {
		case KeyEvent::KEY_ESCAPE:
			// quit the application
			quit();
			break;
		case KeyEvent::KEY_h:
			// mouse cursor and ui visibility
			mVDSettings->mCursorVisible = !mVDSettings->mCursorVisible;
			setUIVisibility(mVDSettings->mCursorVisible);
			break;
		}
	}
}
void _TBOX_PREFIX_App::keyUp(KeyEvent event)
{
	if (!mVDSession->handleKeyUp(event)) {
	}
}

void _TBOX_PREFIX_App::draw()
{
	gl::clear(Color::black());
	if (mFadeInDelay) {
		mVDSettings->iAlpha = 0.0f;
		if (getElapsedFrames() > mVDSession->getFadeInDelay()) {
			mFadeInDelay = false;
			timeline().apply(&mVDSettings->iAlpha, 0.0f, 1.0f, 1.5f, EaseInCubic());
		}
	}

	//gl::setMatricesWindow(toPixels(getWindowSize()),false);
	gl::setMatricesWindow(mVDSettings->mRenderWidth, mVDSettings->mRenderHeight, false);
	gl::draw(mVDSession->getMixTexture(), getWindowBounds());
	// NDI receive
	auto meta = mReceiver.getMetadata();
	auto tex = mReceiver.getVideoTexture();
	if (tex.first) {
		Rectf centeredRect = Rectf(tex.first->getBounds()).getCenteredFit(getWindowBounds(), true);
		gl::draw(tex.first, centeredRect);
	}
	CI_LOG_I(" Frame: " << tex.second << ", metadata: " << meta.first << " : " << meta.second);

	// Spout Send
	mSpoutOut.sendViewport();
	getWindow()->setTitle(mVDSettings->sFps + " fps VDMixer");
	// imgui
	if (!mVDSettings->mCursorVisible) return;
	if (mTexture) {
		gl::draw(mTexture, getWindowBounds());
	}

}

void prepareSettings(App::Settings *settings)
{
	settings->setWindowSize(640, 480);
}

CINDER_APP(_TBOX_PREFIX_App, RendererGl, prepareSettings)
