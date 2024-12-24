#include "game.h"
#include <ace/managers/key.h>
#include <ace/managers/game.h>
#include <ace/managers/system.h>
#include <ace/managers/viewport/simplebuffer.h>
#include <ace/managers/blit.h> // Blitting fns
#include <ace/utils/palette.h>


// Let's make code more readable by giving names to numbers
// It is a good practice to name constant stuff using uppercase
#define BALL_WIDTH 8
#define BALL_COLOR 1
#define PADDLE_WIDTH 8
#define PADDLE_HEIGHT 32
#define PADDLE_LEFT_COLOR 2
#define PADDLE_RIGHT_COLOR 3
#define SCORE_COLOR 1
#define WALL_HEIGHT 1
#define WALL_COLOR 1
#define PLAYFIELD_HEIGHT (256-32)
#define PADDLE_MAX_POS_Y (PLAYFIELD_HEIGHT - PADDLE_HEIGHT - 1)
#define PADDLE_SPEED 4
#define PADDLE_BG_BUFFER_WIDTH CEIL_TO_FACTOR(PADDLE_WIDTH, 16)
#define BALL_BG_BUFFER_WIDTH CEIL_TO_FACTOR(BALL_WIDTH, 16)
#define PADDLE_LEFT_BITMAP_OFFSET_Y 0)
#define PADDLE_RIGHT_BITMAP_OFFSET_Y PADDLE_HEIGHT
#define BALL_BITMAP_OFFSET_Y (PADDLE_RIGHT_BITMAP_OFFSET_Y + PADDLE_HEIGHT)


static tView *s_pView; // View containing all the viewports
static tVPort *s_pVpScore; // Viewport for score
static tSimpleBufferManager *s_pScoreBuffer;
static tVPort *s_pVpMain; // Viewport for playfield
static tSimpleBufferManager *s_pMainBuffer;
static tBitMap *s_pBmPaddleLeftBg;
static tBitMap *s_pBmPaddleRightBg;
static tBitMap *s_pBmPaddleBallBg;
static UBYTE s_hasBackgroundToRestore;
static UWORD uwPaddleLeftPosY = 0;
static UWORD uwPaddleRightPosY = 0;
static tBitMap *s_pBmObjects;
static tBitMap *s_pBmObjectsMask;

void gameGsCreate(void) {
  s_pView = viewCreate(0, TAG_END);

  // Viewport for score bar - on top of screen
  s_pVpScore = vPortCreate(0,
    TAG_VPORT_VIEW, s_pView,
    TAG_VPORT_BPP, 4,
    TAG_VPORT_HEIGHT, 32,
  TAG_END);
  s_pScoreBuffer = simpleBufferCreate(0,
    TAG_SIMPLEBUFFER_VPORT, s_pVpScore,
    TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
  TAG_END);

  // Now let's do the same for main playfield
  s_pVpMain = vPortCreate(0,
    TAG_VPORT_VIEW, s_pView,
    TAG_VPORT_BPP, 4,
  TAG_END);
  s_pMainBuffer = simpleBufferCreate(0,
    TAG_SIMPLEBUFFER_VPORT, s_pVpMain,
    TAG_SIMPLEBUFFER_BITMAP_FLAGS, BMF_CLEAR,
  TAG_END);

//-------------------------------------------------------------- NEW STUFF START
  // Load palette from file to first viewport - second one will use the same
  // due to TAG_VIEW_GLOBAL_PALETTE being by default set to 1. We're using
  // 4BPP display, which means max 16 colors.
  paletteLoadFromPath("data/pong.plt", s_pVpScore->pPalette, 16);

  // Load background graphics and draw them immediately
  tBitMap *pBmBackground = bitmapCreateFromPath("data/pong_bg.bm", 0);
  for(UWORD uwX = 0; uwX < s_pMainBuffer->uBfrBounds.uwX; uwX += 16) {
    for(UWORD uwY = 0; uwY < s_pMainBuffer->uBfrBounds.uwY; uwY += 16) {
      blitCopyAligned(pBmBackground, 0, 0, s_pMainBuffer->pBack, uwX, uwY, 16, 16);
    }
  }
  bitmapDestroy(pBmBackground);

  // Draw line separating score VPort and main VPort, leave one line blank after it
  blitLine(
    s_pScoreBuffer->pBack,
    0, s_pVpScore->uwHeight - 2,
    s_pVpScore->uwWidth - 1, s_pVpScore->uwHeight - 2,
    SCORE_COLOR, 0xFFFF, 0 // Try patterns 0xAAAA, 0xEEEE, etc.
  );

  s_pBmPaddleLeftBg = bitmapCreate(PADDLE_BG_BUFFER_WIDTH, PADDLE_HEIGHT, 4, 0);
  s_pBmPaddleRightBg = bitmapCreate(PADDLE_BG_BUFFER_WIDTH, PADDLE_HEIGHT, 4, 0);
  s_pBmPaddleBallBg = bitmapCreate(BALL_BG_BUFFER_WIDTH, BALL_WIDTH, 4, 0);
  s_pBmObjects = bitmapCreateFromPath("data/pong_paddles.bm", 0);
  s_pBmObjectsMask = bitmapCreateFromPath("data/pong_paddles_mask.bm", 1);

  s_hasBackgroundToRestore = 0;


  systemUnuse();

  // Load the view
  viewLoad(s_pView);
}

void gameGsLoop(void) {
  // This will loop every frame
  if(keyCheck(KEY_ESCAPE)) {
    gameExit();
    return; // early return - don't process anything else, no need for big `else` anymore
  }

  UWORD uwPaddleRightPosX = s_pVpMain->uwWidth - PADDLE_WIDTH;

  if(s_hasBackgroundToRestore) {
    // Restore background under left paddle
    blitCopy(
      s_pBmPaddleLeftBg, 0, 0,
      s_pMainBuffer->pBack, 0, uwPaddleLeftPosY,
      PADDLE_WIDTH, PADDLE_HEIGHT, MINTERM_COOKIE
    );

    // Restore background under right paddle
    blitCopy(
      s_pBmPaddleRightBg, 0, 0,
      s_pMainBuffer->pBack, uwPaddleRightPosX, uwPaddleRightPosY,
      PADDLE_WIDTH, PADDLE_HEIGHT, MINTERM_COOKIE
    );
  }

  // Update left paddle position
  if(keyCheck(KEY_S)) {
    uwPaddleLeftPosY = MIN(uwPaddleLeftPosY + PADDLE_SPEED, PADDLE_MAX_POS_Y);
  }
  else if(keyCheck(KEY_W)) {
    uwPaddleLeftPosY = MAX(uwPaddleLeftPosY - PADDLE_SPEED, 0);
  }

  // Update right paddle position
  if(keyCheck(KEY_DOWN)) {
    uwPaddleRightPosY = MIN(uwPaddleRightPosY + PADDLE_SPEED, PADDLE_MAX_POS_Y);
  }
  else if(keyCheck(KEY_UP)) {
    uwPaddleRightPosY = MAX(uwPaddleRightPosY - PADDLE_SPEED, 0);
  }

  // Save background under left paddle
  blitCopy(
    s_pMainBuffer->pBack, 0, uwPaddleLeftPosY,
    s_pBmPaddleLeftBg, 0, 0,
    PADDLE_WIDTH, PADDLE_HEIGHT, MINTERM_COOKIE
  );

  // Save background under right paddle
  blitCopy(
    s_pMainBuffer->pBack, uwPaddleRightPosX, uwPaddleRightPosY,
    s_pBmPaddleRightBg, 0, 0,
    PADDLE_WIDTH, PADDLE_HEIGHT, MINTERM_COOKIE
  );
  s_hasBackgroundToRestore = 1;
 // Draw left paddle
  blitUnsafeCopyMask(s_pBmObjects, 0, PADDLE_LEFT_BITMAP_OFFSET_Y , s_pMainBuffer->pBack, 0, uwPaddleLeftPosY,PADDLE_WIDTH, PADDLE_HEIGHT, s_pBmObjectsMask->Planes[0]);

  // Draw right paddle
  blitUnsafeCopyMask(
    s_pBmObjects, 0, PADDLE_RIGHT_BITMAP_OFFSET_Y,
    s_pMainBuffer->pBack, uwPaddleRightPosX, uwPaddleRightPosY,
    PADDLE_WIDTH, PADDLE_HEIGHT, s_pBmObjectsMask->Planes[0]
  );

  // Draw ball
  blitUnsafeCopyMask(
    s_pBmObjects, 0, BALL_BITMAP_OFFSET_Y,
    s_pMainBuffer->pBack,
    // x center: half of screen width minus half of ball
    (s_pVpMain->uwWidth - BALL_WIDTH) / 2,
    // y center: half of screen height minus half of ball
    (s_pVpMain->uwHeight - BALL_WIDTH) / 2,
    BALL_WIDTH, BALL_WIDTH, s_pBmObjectsMask->Planes[0]
  );
//---------------------------------------------------------------- NEW STUFF END
  vPortWaitForEnd(s_pVpMain);
}

void gameGsDestroy(void) {
  systemUse();

  bitmapDestroy(s_pBmPaddleLeftBg);
  bitmapDestroy(s_pBmPaddleRightBg);
  bitmapDestroy(s_pBmPaddleBallBg);
  bitmapDestroy(s_pBmObjects);
  bitmapDestroy(s_pBmObjectsMask);

  // This will also destroy all associated viewports and viewport managers
  viewDestroy(s_pView);
}