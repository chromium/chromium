// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/test/run_until.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/confirm_quit.h"
#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace {

class ConfirmQuitControllerPanelInteractiveUITest
    : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    constexpr unsigned short kQKeyCode = 12;
    cmd_q_event_ = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                    location:NSZeroPoint
                               modifierFlags:NSEventModifierFlagCommand
                                   timestamp:0
                                windowNumber:0
                                     context:nil
                                  characters:@"q"
                 charactersIgnoringModifiers:@"q"
                                   isARepeat:NO
                                     keyCode:kQKeyCode];
  }

  void TearDownOnMainThread() override {
    ConfirmQuitPanelController.isKeyDownForKeyCodeMock = nil;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  NSEvent* cmd_q_event_;
};

// Verifies that a single quick tap of the accelerator does not trigger a quit.
// The HUD window should show for |confirm_quit::kWindowFadeDuration| and the
// browser window should not change opacity.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       SingleTapDoesNotTriggerQuit) {
  NSWindow* browserWindow =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  NSWindow* hudWindow = [ConfirmQuitPanelController sharedController].window;

  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        return NO;
      };

  BOOL shouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_FALSE(shouldQuit);
  EXPECT_FLOAT_EQ(hudWindow.alphaValue, 1.0);
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 1.0);
  EXPECT_TRUE(
      base::test::RunUntil([=] { return hudWindow.alphaValue == 0.0; }));
}

// Verifies that holding the accelerator long enough triggers a quit. The HUD
// window should stay visible, and the browser window should fade out.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       SustainedHoldTriggersQuit) {
  NSWindow* browserWindow =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  NSWindow* hudWindow = [ConfirmQuitPanelController sharedController].window;

  base::TimeTicks startTime = base::TimeTicks::Now();
  __block BOOL windowsFaded = NO;
  __block BOOL hudShown = NO;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        if (browserWindow.alphaValue < 1.0) {
          windowsFaded = YES;
        }

        if (hudWindow.alphaValue > 0.0) {
          hudShown = YES;
        }

        // Simulate a key down for long enough to trigger a quit.
        return (BOOL)((base::TimeTicks::Now() - startTime) <
                      confirm_quit::kShowDuration);
      };

  BOOL shouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_TRUE(shouldQuit);
  EXPECT_TRUE(windowsFaded);
  EXPECT_TRUE(hudShown);
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 0.0);
}

// Verifies that tapping the accelerator twice quickly triggers a quit. Both
// windows should stay visible during the first tap, and should be dismissed
// immediately after the second tap.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       DoubleTapTriggersQuit) {
  NSWindow* browserWindow =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  NSWindow* hudWindow = [ConfirmQuitPanelController sharedController].window;

  __block int callCount = 0;
  __block BOOL hudShown = NO;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        if (hudWindow.alphaValue > 0.0) {
          hudShown = YES;
        }

        // Simulate key down once.
        callCount++;
        return (BOOL)(callCount < 2);
      };
  BOOL firstShouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_FALSE(firstShouldQuit);
  EXPECT_TRUE(hudShown);
  EXPECT_EQ(browserWindow.alphaValue, 1.0);

  callCount = 0;
  BOOL secondShouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_TRUE(secondShouldQuit);
  // Double tap hides windows instantly.
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 0.0);
}

// Verifies that a quick tap followed by a long hold triggers a quit. The first
// tap should cause the HUD to appear, and holding the key down after the first
// tap should cause the browser window to fade out while keeping the HUD
// visible.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       SingleTapThenHoldTriggersQuit) {
  NSWindow* browserWindow =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  NSWindow* hudWindow = [ConfirmQuitPanelController sharedController].window;

  // Tap.
  __block int callCount = 0;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        // Simulate key down once.
        callCount++;
        return (BOOL)(callCount < 2);
      };
  BOOL firstShouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_FALSE(firstShouldQuit);
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 1.0);
  EXPECT_FLOAT_EQ(hudWindow.alphaValue, 1.0);

  // Press and hold.
  base::TimeTicks startTime = base::TimeTicks::Now();
  __block BOOL browserWindowFaded = NO;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        // Ensure the browser window fades while the key is held.
        if (browserWindow.alphaValue < 1.0) {
          browserWindowFaded = YES;
        }

        // Simulate a key down for long enough to trigger a quit.
        return (BOOL)((base::TimeTicks::Now() - startTime) <
                      confirm_quit::kShowDuration + base::Seconds(1));
      };
  BOOL secondShouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_TRUE(secondShouldQuit);
  EXPECT_TRUE(browserWindowFaded);
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 0.0);
  EXPECT_FLOAT_EQ(hudWindow.alphaValue, 1.0);
}

// Verifies that child windows (like JS dialogs) fade out when the browser
// windows do.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       SustainedHoldFadesAllWindows) {
  NSWindow* browserWindow =
      browser()->window()->GetNativeWindow().GetNativeNSWindow();
  CGRect childContentRect = {browserWindow.frame.origin, {100, 100}};
  NSWindow* childWindow =
      [[NSWindow alloc] initWithContentRect:childContentRect
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  [childWindow orderFront:nil];
  [browserWindow addChildWindow:childWindow ordered:NSWindowAbove];

  EXPECT_EQ(childWindow.alphaValue, 1.0);

  base::TimeTicks startTime = base::TimeTicks::Now();
  __block BOOL browserWindowFaded = NO;
  __block BOOL childFaded = NO;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        if (browserWindow.alphaValue < 1.0) {
          browserWindowFaded = YES;
        }

        if (childWindow.alphaValue < 1.0) {
          childFaded = YES;
        }

        // Return YES for enough time to trigger a quit and see the fade.
        base::TimeDelta elapsed = base::TimeTicks::Now() - startTime;
        return (BOOL)(elapsed <
                      (confirm_quit::kShowDuration + base::Milliseconds(500)));
      };

  BOOL shouldQuit = [[ConfirmQuitPanelController sharedController]
      runConfirmQuitLoopWithEvent:cmd_q_event_];
  EXPECT_TRUE(shouldQuit);
  EXPECT_TRUE(browserWindowFaded);
  EXPECT_TRUE(childFaded);
}
}  // namespace
