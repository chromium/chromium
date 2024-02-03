// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/renderer_context_menu/chrome_swizzle_services_menu_updater.h"

#include "base/apple/foundation_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/application_launch.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#import "testing/gtest_mac.h"

class RenderViewContextMenuMacCocoaBrowserTest : public InProcessBrowserTest {
 public:
  RenderViewContextMenuMacCocoaBrowserTest() = default;

  RenderViewContextMenuMacCocoaBrowserTest(
      const RenderViewContextMenuMacCocoaBrowserTest&) = delete;
  RenderViewContextMenuMacCocoaBrowserTest& operator=(
      const RenderViewContextMenuMacCocoaBrowserTest&) = delete;

 protected:
  void SetUpOnMainThread() override {
    filtered_items_ = [[NSMutableArray alloc] init];
    [ChromeSwizzleServicesMenuUpdater
        storeFilteredEntriesForTestingInArray:filtered_items_];

    // Add a textfield, which we'll use to present a contextual menu for
    // testing. Fill it with a URL, as the services that need to be filtered
    // primarily appear for URLs.
    text_field_ =
        [[NSTextField alloc] initWithFrame:NSMakeRect(20, 20, 100, 20)];
    [text_field_ setStringValue:@"http://someurl.com/"];
    NSWindow* window =
        browser()->window()->GetNativeWindow().GetNativeNSWindow();
    [[window contentView] addSubview:text_field_];
  }

  void TearDownOnMainThread() override {
    [text_field_ removeFromSuperview];
    [ChromeSwizzleServicesMenuUpdater
        storeFilteredEntriesForTestingInArray:nil];
  }

  NSMutableArray* __strong filtered_items_;
  NSTextField* __strong text_field_;
};

// Confirm that the private classes used to filter Safari's redundant Services
// items exist and implement the expected methods, and that the filtering code
// successfully removes those Services items.
IN_PROC_BROWSER_TEST_F(RenderViewContextMenuMacCocoaBrowserTest,
                       ServicesFiltering) {
  // Confirm that the _NSServicesMenuUpdater class exists and implements the
  // method we expect it to.
  Class menuUpdaterClass = NSClassFromString(@"_NSServicesMenuUpdater");
  EXPECT_TRUE(menuUpdaterClass);
  EXPECT_TRUE([menuUpdaterClass instancesRespondToSelector:@selector
                                (populateMenu:withServiceEntries:forDisplay:)]);

  // Confirm that the _NSServiceEntry class exists and implements the
  // method we expect it to.
  Class serviceEntryClass = NSClassFromString(@"_NSServiceEntry");
  EXPECT_TRUE(serviceEntryClass);
  EXPECT_TRUE([serviceEntryClass
      instancesRespondToSelector:@selector(bundleIdentifier)]);

  // Make the testing textfield the browser window's first responder, in
  // preparation for the contextual menu we're about to display. Even though the
  // code to filter Services items lives in render_view_context_menu_mac, it
  // filters all context menus no matter which control invokes them (as well as
  // the application Services menu). So to test, we just need a control with a
  // bit of selected text.
  NSWindow* window = browser()->window()->GetNativeWindow().GetNativeNSWindow();
  [window makeFirstResponder:text_field_];
  [text_field_ selectText:nil];

  // Create a contextual menu.
  NSMenu* popupMenu = [[NSMenu alloc] initWithTitle:@"menu"];
  [popupMenu addItemWithTitle:@"Menu Item" action:nullptr keyEquivalent:@""];

  // Arrange to dismiss the contextual menu in the future (to break out of the
  // upcoming modal loop).
  dispatch_async(dispatch_get_main_queue(), ^{
    [popupMenu cancelTrackingWithoutAnimation];
  });

  // Bring up the contextual menu from the textfield (actually its field
  // editor).
  NSView* firstResponder =
      base::apple::ObjCCast<NSView>([window firstResponder]);
  [NSMenu popUpContextMenu:popupMenu
                 withEvent:[NSApp currentEvent]
                   forView:firstResponder];

  // Confirm that Services items were removed from the contextual menu.

  bool was_safari_item_removed = false;
  bool was_open_url_item_removed = false;

  for (id item in filtered_items_) {
    if ([[item valueForKey:@"bundleIdentifier"]
            isEqualToString:@"com.apple.Safari"]) {
      was_safari_item_removed = true;
    }
    if ([[item valueForKey:@"message"] isEqualToString:@"openURL"]) {
      was_open_url_item_removed = true;
    }
  }

  EXPECT_TRUE(was_safari_item_removed);
  EXPECT_TRUE(was_open_url_item_removed);
}
