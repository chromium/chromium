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
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "ui/base/test/ns_ax_tree_validator.h"

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
          itemWithTag:IDC_BOOKMARK_THIS_TAB],
      base::scoped_policy::RETAIN);

  // The mainMenu item doesn't have an action associated while the browser
  // window isn't focused, which we can't do in a browser test. So associate one
  // manually.
  EXPECT_EQ([bookmark_menu_item action], nullptr);
  [bookmark_menu_item setAction:@selector(commandDispatch:)];

  EXPECT_TRUE(window.get());
  EXPECT_TRUE(bookmark_menu_item.get());

  chrome::CloseAllBrowsersAndQuit();
  ui_test_utils::WaitForBrowserToClose();

  EXPECT_EQ([bookmark_menu_item action], @selector(commandDispatch:));

  // Try validating a command via the NSUserInterfaceValidation protocol.
  // With the delegates removed, CommandDispatcher ends up calling into the
  // NSWindow (e.g. NativeWidgetMacNSWindow)'s defaultValidateUserInterfaceItem,
  // which currently asks |super|. That is, NSWindow. Which says YES.
  EXPECT_TRUE([window validateUserInterfaceItem:bookmark_menu_item]);
}

class BrowserWindowMacA11yTest : public BrowserWindowMacTest {
 public:
  BrowserWindowMacA11yTest() = default;
  ~BrowserWindowMacA11yTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserWindowMacTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceRendererAccessibility);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserWindowMacA11yTest);
};

IN_PROC_BROWSER_TEST_F(BrowserWindowMacA11yTest, A11yTreeIsWellFormed) {
  NSWindow* window = browser()->window()->GetNativeWindow().GetNativeNSWindow();
  size_t nodes_visited = 0;
  base::Optional<ui::NSAXTreeProblemDetails> details =
      ui::ValidateNSAXTree(window, &nodes_visited);
  EXPECT_FALSE(details.has_value()) << details->ToString();

  // There should be at least a handful of AX nodes in the tree - fail this test
  // if for some reason (eg) the window has no children, which would otherwise
  // be a well-formed AX tree.
  EXPECT_GE(nodes_visited, 10U);

  if (HasFailure())
    ui::PrintNSAXTree(window);
}
