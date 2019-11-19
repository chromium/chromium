// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/run_loop.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/permissions/permission_request_manager_test_api.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "ui/base/test/ui_controls.h"
#import "ui/base/test/windowed_nsnotification_observer.h"
#include "ui/base/ui_base_features.h"
#import "ui/events/test/cocoa_test_event_utils.h"

class PermissionBubbleViewsInteractiveUITest : public InProcessBrowserTest {
 public:
  PermissionBubbleViewsInteractiveUITest() {}

  void EnsureWindowActive(NSWindow* window, const char* message) {
    SCOPED_TRACE(message);
    EXPECT_TRUE(window);

    // Activation is asynchronous on Mac. If the window didn't become active,
    // wait for it.
    if (![window isKeyWindow]) {
      base::scoped_nsobject<WindowedNSNotificationObserver> waiter(
          [[WindowedNSNotificationObserver alloc]
              initForNotification:NSWindowDidBecomeKeyNotification
                           object:window]);
      [waiter wait];
    }
    EXPECT_TRUE([window isKeyWindow]);

    // Whether or not we had to wait, flush the run loop. This ensures any
    // asynchronous close operations have completed.
    base::RunLoop().RunUntilIdle();
  }

  // Send Cmd+keycode in the key window to NSApp.
  void SendAccelerator(ui::KeyboardCode keycode) {
    bool shift = false;
    bool alt = false;
    bool control = false;
    bool command = true;
    // Note that although this takes an NSWindow, it's just used to create the
    // NSEvent* which will be dispatched to NSApp (i.e. not NSWindow).
    ui_controls::SendKeyPress([NSApp keyWindow], keycode, control, shift, alt,
                              command);
  }

  void SetUpOnMainThread() override {
    // Make the browser active (ensures the app can receive key events).
    EXPECT_TRUE(ui_test_utils::BringBrowserWindowToFront(browser()));

    test_api_ =
        std::make_unique<test::PermissionRequestManagerTestApi>(browser());
    EXPECT_TRUE(test_api_->manager());

    test_api_->AddSimpleRequest(ContentSettingsType::GEOLOCATION);

    EXPECT_TRUE([browser()->window()->GetNativeWindow().GetNativeNSWindow()
                     isKeyWindow]);

    // The PermissionRequestManager displays prompts asynchronously.
    base::RunLoop().RunUntilIdle();

    // The bubble should steal key focus when shown.
    EnsureWindowActive(test_api_->GetPromptWindow().GetNativeNSWindow(),
                       "show permission bubble");
  }

 private:
  std::unique_ptr<test::PermissionRequestManagerTestApi> test_api_;

  DISALLOW_COPY_AND_ASSIGN(PermissionBubbleViewsInteractiveUITest);
};

// There is only one tab. Cmd+w will close it along with the browser window.
IN_PROC_BROWSER_TEST_F(PermissionBubbleViewsInteractiveUITest,
                       CmdWClosesWindow) {
  base::scoped_nsobject<NSWindow> browser_window(
      browser()->window()->GetNativeWindow().GetNativeNSWindow(),
      base::scoped_policy::RETAIN);
  EXPECT_TRUE([browser_window isVisible]);

  SendAccelerator(ui::VKEY_W);

  // The actual window close happens via a posted task.
  EXPECT_TRUE([browser_window isVisible]);
  ui_test_utils::WaitForBrowserToClose(browser());
  EXPECT_FALSE([browser_window isVisible]);
}
