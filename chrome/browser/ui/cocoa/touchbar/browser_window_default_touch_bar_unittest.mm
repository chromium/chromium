// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/mac/scoped_nsobject.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_profile_test.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "third_party/ocmock/OCMock/OCMock.h"
#import "ui/base/cocoa/touch_bar_util.h"

namespace {

// Touch bar identifiers.
NSString* const kBrowserWindowTouchBarId = @"browser-window";
NSString* const kTabFullscreenTouchBarId = @"tab-fullscreen";

// Touch bar items identifiers.
NSString* const kBackForwardTouchId = @"BACK-FWD";
NSString* const kReloadOrStopTouchId = @"RELOAD-STOP";
NSString* const kHomeTouchId = @"HOME";
NSString* const kSearchTouchId = @"SEARCH";
NSString* const kStarTouchId = @"BOOKMARK";
NSString* const kNewTabTouchId = @"NEW-TAB";
NSString* const kFullscreenOriginLabelTouchId = @"FULLSCREEN-ORIGIN-LABEL";

// The button indexes in the back and forward segment control.
const int kBackSegmentIndex = 0;
const int kForwardSegmentIndex = 1;

}  // namespace

class BrowserWindowDefaultTouchBarUnitTest : public CocoaProfileTest {
 public:
  void SetUp() override {
    CocoaProfileTest::SetUp();
    ASSERT_TRUE(browser());

    command_updater_ = browser()->command_controller();

    if (@available(macOS 10.12.2, *)) {
      touch_bar_.reset([[BrowserWindowDefaultTouchBar alloc]
          initWithBrowser:browser()
               controller:nil]);
    }
  }

  NSString* GetFullscreenTouchBarItemId(NSString* id) {
    return ui::GetTouchBarItemId(kTabFullscreenTouchBarId, id);
  }

  NSString* GetBrowserTouchBarItemId(NSString* id) {
    return ui::GetTouchBarItemId(kBrowserWindowTouchBarId, id);
  }

  void UpdateCommandEnabled(int id, bool enabled) {
    command_updater_->UpdateCommandEnabled(id, enabled);
  }

  void TearDown() override {
    if (@available(macOS 10.12.2, *))
      touch_bar_.reset();
    CocoaProfileTest::TearDown();
  }

  CommandUpdater* command_updater_;  // Weak, owned by Browser.

  API_AVAILABLE(macos(10.12.2))
  base::scoped_nsobject<BrowserWindowDefaultTouchBar> touch_bar_;
};

// Tests to check if the touch bar contains the correct items.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, TouchBarItems) {
  if (@available(macOS 10.12.2, *)) {
    // Set to tab fullscreen.
    FullscreenController* fullscreen_controller =
        browser()->exclusive_access_manager()->fullscreen_controller();
    fullscreen_controller->set_is_tab_fullscreen_for_testing(true);
    EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());

    // The touch bar should only contain an item that displays the origin of the
    // tab content fullscreen.
    NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
    NSArray* touch_bar_items = [touch_bar itemIdentifiers];
    EXPECT_TRUE(
        [touch_bar_items containsObject:GetFullscreenTouchBarItemId(
                                            kFullscreenOriginLabelTouchId)]);
    EXPECT_EQ(1u, [touch_bar_items count]);

    // Exit fullscreen.
    fullscreen_controller->set_is_tab_fullscreen_for_testing(false);
    EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

    PrefService* prefs = profile()->GetPrefs();
    DCHECK(prefs);
    prefs->SetBoolean(prefs::kShowHomeButton, true);
    touch_bar_items = [[touch_bar_ makeTouchBar] itemIdentifiers];
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kBackForwardTouchId)]);
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kReloadOrStopTouchId)]);
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kHomeTouchId)]);
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kSearchTouchId)]);
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kStarTouchId)]);
    EXPECT_TRUE([touch_bar_items
        containsObject:GetBrowserTouchBarItemId(kNewTabTouchId)]);
  }
}

// Tests the reload or stop touch bar item.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, ReloadOrStopTouchBarItem) {
  if (@available(macOS 10.12.2, *)) {
    NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
    [touch_bar_ setIsPageLoading:NO];

    NSTouchBarItem* item = [touch_bar_
                     touchBar:touch_bar
        makeItemForIdentifier:GetBrowserTouchBarItemId(kReloadOrStopTouchId)];
    EXPECT_EQ(IDC_RELOAD, [[item view] tag]);

    [touch_bar_ setIsPageLoading:YES];
    item = [touch_bar_ touchBar:touch_bar
          makeItemForIdentifier:GetBrowserTouchBarItemId(kReloadOrStopTouchId)];
    EXPECT_EQ(IDC_STOP, [[item view] tag]);
  }
}

// Tests to see if the back/forward items on the touch bar is in sync with the
// back and forward commands.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, BackForwardCommandUpdate) {
  if (@available(macOS 10.12.2, *)) {
    NSSegmentedControl* back_forward_control = [touch_bar_ backForwardControl];

    UpdateCommandEnabled(IDC_BACK, true);
    UpdateCommandEnabled(IDC_FORWARD, true);
    EXPECT_TRUE([back_forward_control isEnabledForSegment:kBackSegmentIndex]);
    EXPECT_TRUE(
        [back_forward_control isEnabledForSegment:kForwardSegmentIndex]);

    UpdateCommandEnabled(IDC_BACK, false);
    EXPECT_FALSE([back_forward_control isEnabledForSegment:kBackSegmentIndex]);
    EXPECT_TRUE(
        [back_forward_control isEnabledForSegment:kForwardSegmentIndex]);

    UpdateCommandEnabled(IDC_FORWARD, false);
    EXPECT_FALSE([back_forward_control isEnabledForSegment:kBackSegmentIndex]);
    EXPECT_FALSE(
        [back_forward_control isEnabledForSegment:kForwardSegmentIndex]);
  }
}
