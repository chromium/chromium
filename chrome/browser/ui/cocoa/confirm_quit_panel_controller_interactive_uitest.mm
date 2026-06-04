// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/test/run_until.h"
#import "chrome/browser/app_controller_mac.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
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
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
  NSWindow* hudWindow = controller.window;

  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        return NO;
      };

  BOOL shouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                          dismissedCallback:nil];
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
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
  NSWindow* hudWindow = controller.window;

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

  BOOL shouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                          dismissedCallback:nil];
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
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
  NSWindow* hudWindow = controller.window;

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
  BOOL firstShouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                               dismissedCallback:nil];
  EXPECT_FALSE(firstShouldQuit);
  EXPECT_TRUE(hudShown);
  EXPECT_EQ(browserWindow.alphaValue, 1.0);

  callCount = 0;
  BOOL secondShouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                                dismissedCallback:nil];
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
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
  NSWindow* hudWindow = controller.window;

  // Tap.
  __block int callCount = 0;
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        // Simulate key down once.
        callCount++;
        return (BOOL)(callCount < 2);
      };
  BOOL firstShouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                               dismissedCallback:nil];
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
  BOOL secondShouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                                dismissedCallback:nil];
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
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  CGRect childContentRect = {browserWindow.frame.origin, {100, 100}};
  NSWindow* childWindow =
      [[NSWindow alloc] initWithContentRect:childContentRect
                                  styleMask:NSWindowStyleMaskTitled
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  [childWindow orderFront:nil];
  [browserWindow addChildWindow:childWindow ordered:NSWindowAbove];

  EXPECT_EQ(childWindow.alphaValue, 1.0);

  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
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

  BOOL shouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                          dismissedCallback:nil];
  EXPECT_TRUE(shouldQuit);
  EXPECT_TRUE(browserWindowFaded);
  EXPECT_TRUE(childFaded);
}

// Verifies that a quit attempt blocked by a "confirm leave" (beforeunload)
// dialog correctly preserves the panel controller and fades the windows back in
// when cancelled.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       BeforeUnloadCancellationRestoresWindowsAndCleansUp) {
  NSWindow* browserWindow =
      browser()->GetWindow()->GetNativeWindow().GetNativeNSWindow();
  AppController* appController = AppController.sharedController;

  // Lazy-initialize the panel controller on AppController.
  ConfirmQuitPanelController* controller =
      appController.confirmQuitPanelControllerForTesting;
  NSWindow* hudWindow = controller.window;

  base::TimeTicks startTime = base::TimeTicks::Now();
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        // Simulate holding the key down long enough to confirm quit.
        return (BOOL)((base::TimeTicks::Now() - startTime) <
                      confirm_quit::kShowDuration);
      };
  BOOL shouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                          dismissedCallback:nil];
  EXPECT_TRUE(shouldQuit);

  // The browser windows should now be completely faded out/hidden.
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 0.0);
  EXPECT_FLOAT_EQ(hudWindow.alphaValue, 1.0);

  // Simulate that the quit has been initiated and is underway.
  browser_shutdown::SetTryingToQuit(true);

  // Now, simulate the "confirm leave" dialog taking focus and AppKit closing
  // the HUD.
  [controller close];

  // Ensure that the panel controller has not been recreated.
  EXPECT_EQ(appController.confirmQuitPanelControllerForTesting, controller);

  // Now, cancel the quit attempt (e.g. user clicked "Stay on this page").
  [appController stopTryingToTerminateApplication];

  // The browser windows should be fully restored to full opacity.
  EXPECT_FLOAT_EQ(browserWindow.alphaValue, 1.0);

  // The panel controller should now be cleanly deallocated and become nil.
  EXPECT_EQ([appController valueForKey:@"_confirmQuitPanelController"], nil);
}

// Verifies that the dismissed callback is invoked when the panel is dismissed.
IN_PROC_BROWSER_TEST_F(ConfirmQuitControllerPanelInteractiveUITest,
                       DismissedCallbackIsInvoked) {
  ConfirmQuitPanelController* controller =
      [[ConfirmQuitPanelController alloc] init];
  NSWindow* hudWindow = controller.window;

  // Simulates a quick tap, which will not trigger a quit.
  ConfirmQuitPanelController.isKeyDownForKeyCodeMock =
      ^(unsigned short keyCode) {
        return NO;
      };

  bool callback_invoked = false;
  bool* callback_invoked_ptr = &callback_invoked;
  BOOL shouldQuit = [controller runConfirmQuitLoopWithEvent:cmd_q_event_
                                          dismissedCallback:^{
                                            *callback_invoked_ptr = true;
                                          }];
  EXPECT_FALSE(shouldQuit);
  EXPECT_FLOAT_EQ(hudWindow.alphaValue, 1.0);

  // Wait for the panel to fade out and close, which should trigger the
  // callback.
  EXPECT_TRUE(base::test::RunUntil([&] { return callback_invoked; }));
}

}  // namespace
