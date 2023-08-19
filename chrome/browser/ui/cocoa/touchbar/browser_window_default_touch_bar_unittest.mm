// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"

#import <Cocoa/Cocoa.h>

#include "base/apple/foundation_util.h"
#include "base/mac/mac_util.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/command_updater.h"
#include "chrome/browser/search_engines/template_url_service_factory_test_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_default_touch_bar.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#import "ui/base/cocoa/touch_bar_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

class BrowserWindowDefaultTouchBarUnitTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();

    // The touch bar can show a search engine prompt, which requires this
    // service.
    template_service_util_ =
        std::make_unique<TemplateURLServiceFactoryTestUtil>(profile());
    template_service_util_->VerifyLoad();

    command_updater_ = browser()->command_controller();

    browser()->tab_strip_model()->AppendWebContents(
        content::WebContentsTester::CreateTestWebContents(profile(), nullptr),
        true);

    touch_bar_ = [[BrowserWindowDefaultTouchBar alloc] init];
    touch_bar_.browser = browser();
  }

  void UpdateCommandEnabled(int id, bool enabled) {
    command_updater_->UpdateCommandEnabled(id, enabled);
  }

  bool ShowsHomeButton() {
    return browser()->profile()->GetPrefs()->GetBoolean(prefs::kShowHomeButton);
  }

  void SetShowHomeButton(bool flag) {
    browser()->profile()->GetPrefs()->SetBoolean(prefs::kShowHomeButton, flag);
  }

  void TearDown() override {
    touch_bar_.browser = nullptr;
    touch_bar_ = nil;

    BrowserWithTestWindowTest::TearDown();
  }

  CocoaTestHelper cocoa_test_helper_;
  raw_ptr<CommandUpdater, DanglingUntriaged>
      command_updater_;  // Weak, owned by Browser.

  std::unique_ptr<TemplateURLServiceFactoryTestUtil> template_service_util_;

  BrowserWindowDefaultTouchBar* __strong touch_bar_;
};

// Test if any known identifiers no longer work. See the message in the test;
// these identifiers may be written out to disk on users' computers if they
// customize the Touch Bar, and the corresponding items will disappear if they
// can no longer be created.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, HistoricTouchBarItems) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  for (NSString* item_identifier : {
           @"BACK-FWD",
           @"BACK",
           @"FORWARD",
           @"RELOAD-STOP",
           @"HOME",
           @"SEARCH",
           @"BOOKMARK",
           @"NEW-TAB",
       }) {
    auto identifier = ui::GetTouchBarItemId(@"browser-window", item_identifier);
    EXPECT_NE(nil, [touch_bar itemForIdentifier:identifier])
        << "BrowserWindowDefaultTouchBar didn't return a Touch Bar item for "
           "an identifier that was once available ("
        << identifier.UTF8String
        << "). If a user's customized Touch Bar includes this item, it will "
           "disappear! Do not update or remove entries in this list just to "
           "make the test pass; keep supporting old identifiers when "
           "possible, even if they're no longer part of the set of "
           "default/customizable items.";
  }
}

// Tests if BrowserWindowDefaultTouchBar can produce the items it says it can
// and, for each kind of bar, also verify that the advertised/customizable lists
// include some representative items (if not, the lists might be wrong.)
TEST_F(BrowserWindowDefaultTouchBarUnitTest, TouchBarItems) {
  auto test_default_identifiers = [&](NSSet* expected_identifiers) {
    NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
    NSMutableSet<NSString*>* advertised_identifiers = [NSMutableSet set];
    [advertised_identifiers
        addObjectsFromArray:touch_bar.defaultItemIdentifiers];
    [advertised_identifiers
        addObjectsFromArray:touch_bar.customizationAllowedItemIdentifiers];
    [advertised_identifiers
        addObjectsFromArray:touch_bar.customizationRequiredItemIdentifiers];
    EXPECT_TRUE([expected_identifiers isSubsetOfSet:advertised_identifiers])
        << "Didn't find the expected identifiers "
        << expected_identifiers.description.UTF8String
        << " in the set of advertised identifiers "
        << advertised_identifiers.description.UTF8String << ".";
    for (NSString* identifier in advertised_identifiers) {
      EXPECT_NE(nil, [touch_bar itemForIdentifier:identifier])
          << "Didn't get a touch bar item for " << identifier.UTF8String;
    }
  };

  // Set to tab fullscreen.
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  fullscreen_controller->set_is_tab_fullscreen_for_testing(true);
  EXPECT_TRUE(fullscreen_controller->IsTabFullscreen());

  // The fullscreen Touch Bar should include *at least* these items.
  test_default_identifiers([NSSet setWithArray:@[
    BrowserWindowDefaultTouchBar.fullscreenOriginItemIdentifier,
  ]]);

  // Exit fullscreen.
  fullscreen_controller->set_is_tab_fullscreen_for_testing(false);
  EXPECT_FALSE(fullscreen_controller->IsTabFullscreen());

  // The default Touch Bar should include *at least* these items.
  test_default_identifiers([NSSet setWithArray:@[
    BrowserWindowDefaultTouchBar.backItemIdentifier,
    BrowserWindowDefaultTouchBar.forwardItemIdentifier,
    BrowserWindowDefaultTouchBar.reloadOrStopItemIdentifier,
  ]]);
}

// Tests the reload or stop touch bar item.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, ReloadOrStopTouchBarItem) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  [touch_bar_ setIsPageLoading:NO];

  NSTouchBarItem* item =
      [touch_bar itemForIdentifier:BrowserWindowDefaultTouchBar
                                       .reloadOrStopItemIdentifier];
  NSButton* button = base::apple::ObjCCast<NSButton>([item view]);
  EXPECT_EQ(IDC_RELOAD, [button tag]);
  EXPECT_EQ([BrowserWindowDefaultTouchBar reloadIcon], [button image]);

  [touch_bar_ setIsPageLoading:YES];
  item = [touch_bar itemForIdentifier:BrowserWindowDefaultTouchBar
                                          .reloadOrStopItemIdentifier];
  button = base::apple::ObjCCast<NSButton>([item view]);
  EXPECT_EQ(IDC_STOP, [button tag]);
  EXPECT_EQ([BrowserWindowDefaultTouchBar navigateStopIcon], [button image]);
}

// Tests the bookmark star touch bar item.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, BookmkarStarTouchBarItem) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];

  [touch_bar_ setIsStarred:NO];
  NSTouchBarItem* item =
      [touch_bar itemForIdentifier:BrowserWindowDefaultTouchBar
                                       .bookmarkStarItemIdentifier];
  NSButton* button = base::apple::ObjCCast<NSButton>([item view]);
  EXPECT_EQ([BrowserWindowDefaultTouchBar starDefaultIcon], [button image]);

  [touch_bar_ setIsStarred:YES];
  item = [touch_bar itemForIdentifier:BrowserWindowDefaultTouchBar
                                          .bookmarkStarItemIdentifier];
  button = base::apple::ObjCCast<NSButton>([item view]);
  EXPECT_EQ([BrowserWindowDefaultTouchBar starActiveIcon], [button image]);
}

// Tests if the back button on the touch bar is in sync with the back command.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, BackCommandUpdate) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  NSTouchBarItem* item = [touch_bar
      itemForIdentifier:BrowserWindowDefaultTouchBar.backItemIdentifier];
  NSButton* button = base::apple::ObjCCast<NSButton>(item.view);

  UpdateCommandEnabled(IDC_BACK, true);
  EXPECT_TRUE(button.enabled);
  UpdateCommandEnabled(IDC_BACK, false);
  EXPECT_FALSE(button.enabled);
}

// Tests if the forward button on the touch bar is in sync with the forward
// command.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, ForwardCommandUpdate) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  NSTouchBarItem* item = [touch_bar
      itemForIdentifier:BrowserWindowDefaultTouchBar.forwardItemIdentifier];
  NSButton* button = base::apple::ObjCCast<NSButton>(item.view);

  UpdateCommandEnabled(IDC_FORWARD, true);
  EXPECT_TRUE(button.enabled);
  UpdateCommandEnabled(IDC_FORWARD, false);
  EXPECT_FALSE(button.enabled);
}

TEST_F(BrowserWindowDefaultTouchBarUnitTest, BackAccessibilityLabel) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  NSTouchBarItem* item = [touch_bar
      itemForIdentifier:BrowserWindowDefaultTouchBar.backItemIdentifier];
  id<NSAccessibility> view = item.view;
  ASSERT_TRUE([view conformsToProtocol:@protocol(NSAccessibility)]);
  EXPECT_NSEQ(view.accessibilityTitle,
              l10n_util::GetNSString(IDS_ACCNAME_BACK));
}

TEST_F(BrowserWindowDefaultTouchBarUnitTest, ForwardAccessibilityLabel) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];
  NSTouchBarItem* item = [touch_bar
      itemForIdentifier:BrowserWindowDefaultTouchBar.forwardItemIdentifier];
  id<NSAccessibility> view = item.view;
  ASSERT_TRUE([view conformsToProtocol:@protocol(NSAccessibility)]);
  EXPECT_NSEQ(view.accessibilityTitle,
              l10n_util::GetNSString(IDS_ACCNAME_FORWARD));
}

// Tests that the home button in the Touch Bar is in sync with the setting.
TEST_F(BrowserWindowDefaultTouchBarUnitTest, HomeUpdate) {
  NSTouchBar* touch_bar = [touch_bar_ makeTouchBar];

  // Save the current state before we start mucking with preferences.
  bool home_button_showing = ShowsHomeButton();

  SetShowHomeButton(false);
  touch_bar = [touch_bar_ makeTouchBar];

  NSString* home_identifier = [BrowserWindowDefaultTouchBar homeItemIdentifier];

  EXPECT_FALSE(
      [[touch_bar defaultItemIdentifiers] containsObject:home_identifier]);

  SetShowHomeButton(true);
  touch_bar = [touch_bar_ makeTouchBar];

  EXPECT_TRUE(
      [[touch_bar defaultItemIdentifiers] containsObject:home_identifier]);

  // Restore the original state.
  SetShowHomeButton(home_button_showing);
}
