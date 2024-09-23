// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "ui/accessibility/accessibility_switches.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/test/ns_ax_tree_validator.h"

// Test harness for Mac-specific behaviors of BrowserWindow.
class BrowserWindowMacTest : public InProcessBrowserTest {
 public:
  BrowserWindowMacTest() = default;

  BrowserWindowMacTest(const BrowserWindowMacTest&) = delete;
  BrowserWindowMacTest& operator=(const BrowserWindowMacTest&) = delete;
};

// Test that mainMenu commands do not attempt to validate against a Browser*
// that is destroyed.
IN_PROC_BROWSER_TEST_F(BrowserWindowMacTest, MenuCommandsAfterDestroy) {
  // Simulate AppKit (e.g. NSMenu) retaining an NSWindow.
  NSWindow* window = browser()->window()->GetNativeWindow().GetNativeNSWindow();
  NSMenuItem* bookmark_menu_item =
      [[[[NSApp mainMenu] itemWithTag:IDC_BOOKMARKS_MENU] submenu]
          itemWithTag:IDC_BOOKMARK_THIS_TAB];

  EXPECT_TRUE(window);
  EXPECT_TRUE(bookmark_menu_item);

  chrome::CloseAllBrowsersAndQuit();
  ui_test_utils::WaitForBrowserToClose();

  EXPECT_EQ([bookmark_menu_item action], @selector(commandDispatch:));

  // Try validating a command via the NSUserInterfaceValidation protocol.
  // With the delegates removed, CommandDispatcher ends up calling into the
  // NSWindow (e.g. NativeWidgetMacNSWindow)'s defaultValidateUserInterfaceItem,
  // which currently asks |super|. That is, NSWindow. Which says YES.
  EXPECT_TRUE([window validateUserInterfaceItem:bookmark_menu_item]);
}

// Test that mainMenu commands from child windows are validated by the window
// chain.
// TODO(crbug.com/40898643): Disabled because this test is flaky.
IN_PROC_BROWSER_TEST_F(BrowserWindowMacTest,
                       DISABLED_MenuCommandsFromChildWindow) {
  NativeWidgetMacNSWindow* window =
      base::apple::ObjCCastStrict<NativeWidgetMacNSWindow>(
          browser()->window()->GetNativeWindow().GetNativeNSWindow());

  // Create a child window.
  NativeWidgetMacNSWindow* child_window = [[NativeWidgetMacNSWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSWindowStyleMaskBorderless
                  backing:NSBackingStoreBuffered
                    defer:NO];
  child_window.releasedWhenClosed = NO;
  [window addChildWindow:child_window ordered:NSWindowAbove];

  NSMenuItem* show_bookmark_bar_menu_item =
      [[[[NSApp mainMenu] itemWithTag:IDC_VIEW_MENU] submenu]
          itemWithTag:IDC_SHOW_BOOKMARK_BAR];

  // Make sure both windows validate the bookmark bar menu item.
  EXPECT_TRUE([window validateUserInterfaceItem:show_bookmark_bar_menu_item]);
  EXPECT_TRUE(
      [child_window validateUserInterfaceItem:show_bookmark_bar_menu_item]);

  browser()->command_controller()->UpdateCommandEnabled(
      show_bookmark_bar_menu_item.tag, false);

  // Make sure both windows find the bookmark bar menu item invalid. The child
  // window check is the focus of this test. The child window does not have a
  // UserInterfaceItemCommandHandler. This test ensures the validation request
  // bubbles up to the parent window.
  EXPECT_FALSE([window validateUserInterfaceItem:show_bookmark_bar_menu_item]);
  EXPECT_FALSE(
      [child_window validateUserInterfaceItem:show_bookmark_bar_menu_item]);
}

class BrowserWindowMacA11yTest : public BrowserWindowMacTest {
 public:
  BrowserWindowMacA11yTest() = default;

  BrowserWindowMacA11yTest(const BrowserWindowMacA11yTest&) = delete;
  BrowserWindowMacA11yTest& operator=(const BrowserWindowMacA11yTest&) = delete;

  ~BrowserWindowMacA11yTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BrowserWindowMacTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kForceRendererAccessibility);
  }
};

IN_PROC_BROWSER_TEST_F(BrowserWindowMacA11yTest, A11yTreeIsWellFormed) {
  NSWindow* window = browser()->window()->GetNativeWindow().GetNativeNSWindow();
  size_t nodes_visited = 0;
  std::optional<ui::NSAXTreeProblemDetails> details =
      ui::ValidateNSAXTree(window, &nodes_visited);
  EXPECT_FALSE(details.has_value()) << details->ToString();

  // There should be at least a handful of AX nodes in the tree - fail this test
  // if for some reason (eg) the window has no children, which would otherwise
  // be a well-formed AX tree.
  EXPECT_GE(nodes_visited, 10U);

  if (HasFailure())
    ui::PrintNSAXTree(window);
}
