/*
                
        Basic Spout sender for Cinder

        Search for "SPOUT" to see what is required
        Uses the Spout dll

        Based on the RotatingBox CINDER example without much modification
        Nothing fancy about this, just the basics.

        Search for "SPOUT" to see what is required

    ==========================================================================
    Copyright (C) 2014 Lynn Jarvis.

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
    ==========================================================================

*/

#include "cinder/app/AppNative.h"
#include "cinder/ImageIo.h"
#include "cinder/gl/Texture.h"
#include "cinder/Camera.h"
#include "cinder/Utilities.h"
#include "cinder/ImageIo.h"
#include "cinder/Surface.h"
#include "cinder/System.h"
#include "cinder/Capture.h"
#include "cinder/gl/Fbo.h"
#include "cinder/MayaCamUI.h"
#include "cinder/ip/EdgeDetect.h"
#include "cinder/ip/Blend.h"
#include "cinder/params/Params.h"

#include <vector>

// spout
#include "spout.h"

using namespace ci;
using namespace ci::app;
using namespace std;
using namespace ci::gl;

class ReymentaRuttEtraApp : public AppNative {
public:
    void prepareSettings(Settings *settings);
	void setup();
	void update();
	void draw();
    void shutdown();
	void mouseDown(MouseEvent event);
	void mouseDrag(MouseEvent event);

private:
    // -------- SPOUT -------------
    SpoutSender spoutsender;                    // Create a Spout sender object
    bool bInitialized;                          // true if a sender initializes OK
    bool bMemoryMode;                           // tells us if texture share compatible
    unsigned int g_Width, g_Height;             // size of the texture being sent out
    char SenderName[256];                       // sender name 
    gl::Texture spoutTexture;                   // Local Cinder texture used for sharing
    // RuttEtra
	Capture mInput;
	gl::Texture webcam;
	vector <Vec3f> pixData;

	CameraPersp mCam;
	MayaCamUI maya;
	int stepX, stepY, lineBreak;
	int blackBreak;
	float elevation;
	bool enableSobel, enableWebcamPreview, enableGrayScale;
	Surface8u edge;
	string pos;
	params::InterfaceGl mParams;

};

// -------- SPOUT -------------
void ReymentaRuttEtraApp::prepareSettings(Settings *settings)
{
        g_Width  = 640;
        g_Height = 512;
        settings->setWindowSize( g_Width, g_Height );
        settings->setFullScreen( false );
        settings->setResizable( false ); // keep the screen size constant for a sender
        settings->setFrameRate( 60.0f );
}
// ----------------------------

void ReymentaRuttEtraApp::setup()
{
    glEnable( GL_TEXTURE_2D );
    gl::enableDepthRead();
    gl::enableDepthWrite(); 

    // -------- SPOUT -------------
    // Set up the texture we will use to send out
    // We grab the screen so it has to be the same size
    spoutTexture =  gl::Texture(g_Width, g_Height);
    strcpy_s(SenderName, "Reymenta RuttEtra Sender"); // we have to set a sender name first
    // Optionally test for texture share compatibility
    // bMemoryMode informs us whether Spout initialized for texture share or memory share
    bMemoryMode = spoutsender.GetMemoryShareMode();
    // Initialize a sender
    bInitialized = spoutsender.CreateSender(SenderName, g_Width, g_Height);
    // RuttEtra
	elevation = 10;
	enableWebcamPreview = enableSobel = false;
	enableGrayScale = true;

	mCam.setPerspective(45.0, getWindowAspectRatio(), 0.1f, 10000.0f);
	mCam.lookAt(Vec3f(0.0, 0.0, 500.0), Vec3f::zero(), Vec3f::yAxis());
	mCam.setCenterOfInterestPoint(Vec3f::zero());

	maya.setCurrentCam(mCam);
	try {
		mInput = Capture(640, 480);
		mInput.start();
	}
	catch (...) {
		console() << "Failed to initialize capture" << std::endl;
	}

	for (int i = 0; i < mInput.getWidth()*mInput.getHeight(); i++)
	{
		pixData.push_back(Vec3f::zero());
	}
	stepY = 5;
	stepX = 5;
	blackBreak = 64;
	mParams = params::InterfaceGl("Rutt-Etra", Vec2i(225, 200));
	mParams.addParam("Elevation damping", &elevation, "min=1 max=10 step=0.01");
	mParams.addParam("Steps X", &stepX, "min=1 max=10");
	mParams.addParam("StepsY", &stepY, "min=1 max=10");
	mParams.addParam("BlackBreak", &blackBreak, "min=0 max=255");

	mParams.addSeparator();
	mParams.addParam("Enable preview", &enableWebcamPreview);
	mParams.addParam("Enable sobel", &enableSobel);
	mParams.addParam("Enable grayscale", &enableGrayScale);

	edge = Surface8u(mInput.getWidth(), mInput.getHeight(), false, SurfaceChannelOrder::RGB);

}
void ReymentaRuttEtraApp::mouseDown(MouseEvent event){
	maya.mouseDown(event.getPos());

}
void ReymentaRuttEtraApp::mouseDrag(MouseEvent event){
	bool middle = event.isMiddleDown() || (event.isMetaDown() && event.isLeftDown());
	bool right = event.isRightDown() || (event.isControlDown() && event.isLeftDown());
	maya.mouseDrag(event.getPos(), event.isLeftDown() && !middle && !right, middle, right);
	pos = toString(event.getPos().x) + "-" + toString(event.getPos().y);
}
void ReymentaRuttEtraApp::update()
{
	if (mInput.isCapturing())
	{
		if (enableSobel)
		{
			ci::ip::edgeDetectSobel(mInput.getSurface(), &edge);
			if (enableWebcamPreview) webcam = gl::Texture(edge);

		}
		else
		{
			if (enableWebcamPreview) webcam = gl::Texture(mInput.getSurface());
		}

		Surface::Iter surfIter = enableSobel ? surfIter = edge.getIter(edge.getBounds()) : surfIter = mInput.getSurface().getIter(mInput.getBounds());

		int p = 0;
		while (surfIter.line())
		{
			while (surfIter.pixel())
			{
				float brightness = surfIter.r() + surfIter.g() + surfIter.b();

				if (surfIter.x() % stepX == 0 && surfIter.y() % stepY == 0)
				{
					pixData[p].set(surfIter.x(), surfIter.mWidth - surfIter.y(), brightness / elevation); // !!!inversion
					p++;
				}

			}
		}

		lineBreak = p;
	}
}

void ReymentaRuttEtraApp::draw()
{
    gl::clear( Color( 0.39f, 0.025f, 0.0f ) ); // red/brown to be different
 
	gl::color(Color::white());
	gl::disableDepthRead();
	if (enableWebcamPreview) gl::draw(webcam, webcam.getBounds(), getWindowBounds());
	gl::enableDepthWrite();
	gl::enableDepthRead(true);
	gl::pushMatrices();
	gl::setMatrices(maya.getCamera());
	gl::color(Color::black());
	int drawnVertices = 0;
	gl::begin(GL_LINE_STRIP);
	int lb = 0;
	for (int x = 0; x < lineBreak; x++)
	{
		if (x < pixData.size())
		{
			if (x > 0)
			{

				if (pixData[x].z < blackBreak / (elevation / 3) || (pixData[x].x < pixData[x - 1].x && x != 0))
				{
					gl::end();
					gl::begin(GL_LINE_STRIP);

				}
				else{
					gl::vertex(pixData[x]);
					gl::color(enableGrayScale ? Color::gray(pixData[x].z / (255 * 3) * elevation) : Color::white());
					drawnVertices++;
				}
			}
		}
	}

	gl::end();

	gl::popMatrices();
	gl::enableAlphaBlending();
	gl::drawString("pos : " + pos + "Points : " + toString(lineBreak) + "\n Drawn Vertices:" + toString(drawnVertices), Vec2f(getWindowWidth() - 120, 20), Color::black());
	gl::disableAlphaBlending();
    // -------- SPOUT -------------
    if(bInitialized) {

        // Grab the screen (current read buffer) into the local spout texture
        spoutTexture.bind();
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, g_Width, g_Height);
        spoutTexture.unbind();

        // Send the texture for all receivers to use
        // NOTE : if SendTexture is called with a framebuffer object bound, that binding will be lost
        // and has to be restored afterwards because Spout uses an fbo for intermediate rendering
        spoutsender.SendTexture(spoutTexture.getId(), spoutTexture.getTarget(), g_Width, g_Height);

    }

    // Show the user what it is sending
    char txt[256];
    sprintf_s(txt, "Sending as [%s]", SenderName);
    gl::setMatricesWindow( getWindowSize() );
    gl::enableAlphaBlending();
    gl::drawString( txt, Vec2f( toPixels( 20 ), toPixels( 20 ) ), Color( 1, 1, 1 ), Font( "Verdana", toPixels( 24 ) ) );
    sprintf_s(txt, "fps : %2.2d", (int)getAverageFps());
    gl::drawString( txt, Vec2f(getWindowWidth() - toPixels( 100 ), toPixels( 20 ) ), Color( 1, 1, 1 ), Font( "Verdana", toPixels( 24 ) ) );
    gl::disableAlphaBlending();
    // ----------------------------
	mParams.draw();

}
// -------- SPOUT -------------
void ReymentaRuttEtraApp::shutdown()
{
    spoutsender.ReleaseSender();
}
// This line tells Cinder to actually create the application
CINDER_APP_NATIVE( ReymentaRuttEtraApp, RendererGl )
