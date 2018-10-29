// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window.h"

#include <memory>

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_utils.h"

// Test harness for Mac-specific behaviors of BrowserWindow.
class BrowserWindowMacTest : public InProcessBrowserTest {
 public:
  BrowserWindowMacTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWindowMacTest);
};

// Test that mainMenu commands do not attempt to validate against a Browser*
// that is destroyed.
IN_PROC_BROWSER_TEST_F(BrowserWindowMacTest, MenuCommandsAfterDestroy) {
  // Simulate AppKit (e.g. NSMenu) retaining an NSWindow.
  base::scoped_nsobject<NSWindow> window(
      browser()->window()->GetNativeWindow().GetNativeNSWindow(),
      base::scoped_policy::RETAIN);
  base::scoped_nsobject<NSMenuItem> bookmark_menu_item(
      [[[[NSApp mainMenu] itemWithTag:IDC_BOOKMARKS_MENU] submenu]
          itemWithTag:IDC_BOOKMARK_PAGE],
      base::scoped_policy::RETAIN);

  // The mainMenu item doesn't have an action associated while the browser
  // window isn't focused, which we can't do in a browser test. So associate one
  // manually.
  EXPECT_EQ([bookmark_menu_item action], nullptr);
  [bookmark_menu_item setAction:@selector(commandDispatch:)];

  EXPECT_TRUE(window.get());
  EXPECT_TRUE(bookmark_menu_item.get());

  content::WindowedNotificationObserver close_observer(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      content::NotificationService::AllSources());
  chrome::CloseAllBrowsersAndQuit();
  close_observer.Wait();

  EXPECT_EQ([bookmark_menu_item action], @selector(commandDispatch:));

  // Try validating a command via the NSUserInterfaceValidation protocol.
  // With the delegates removed, CommandDispatcher ends up calling into the
  // NSWindow (e.g. NativeWidgetMacNSWindow)'s defaultValidateUserInterfaceItem,
  // which currently asks |super|. That is, NSWindow. Which says YES.
  EXPECT_TRUE([window validateUserInterfaceItem:bookmark_menu_item]);
}
