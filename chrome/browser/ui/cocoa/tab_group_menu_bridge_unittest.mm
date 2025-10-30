// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/tab_group_menu_bridge.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/test_support/fake_tab_group_sync_service.h"
#include "components/tab_groups/tab_group_color.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

class TabGroupMenuBridgeTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    service_ = std::make_unique<tab_groups::FakeTabGroupSyncService>();

    // Create a dummy main menu.
    main_menu_ = [[NSMenu alloc] init];
    NSMenuItem* app_menu_item = [[NSMenuItem alloc] init];
    [main_menu_ addItem:app_menu_item];
    [NSApp setMainMenu:main_menu_];

    // Create the "Saved Tab Groups" menu that the bridge will use.
    tab_groups_menu_root_ = [[NSMenuItem alloc] initWithTitle:@"Tab Groups"
                                                       action:nil
                                                keyEquivalent:@""];
    tab_groups_menu_root_.tag = IDC_SAVED_TAB_GROUPS_MENU;
    tab_groups_menu_ = [[NSMenu alloc] initWithTitle:@"Tab Groups"];
    tab_groups_menu_root_.submenu = tab_groups_menu_;
    [main_menu_ addItem:tab_groups_menu_root_];

    // Add the static "New Tab Group" item.
    NSMenuItem* new_group_item =
        [[NSMenuItem alloc] initWithTitle:@"New Tab Group"
                                   action:nil
                            keyEquivalent:@""];
    new_group_item.tag = IDC_CREATE_NEW_TAB_GROUP;
    [tab_groups_menu_ addItem:new_group_item];
  }

  void TearDown() override {
    [NSApp setMainMenu:nil];
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  NSMenu* menu() { return tab_groups_menu_; }
  tab_groups::TabGroupSyncService* service() { return service_.get(); }

  // Helper to add a group to the service.
  void AddGroup(const std::u16string& title,
                const tab_groups::TabGroupColorId& color,
                const std::vector<GURL>& urls) {
    tab_groups::SavedTabGroup group(title, color, {}, std::nullopt);
    for (const auto& url : urls) {
      tab_groups::SavedTabGroupTab tab(url, u"Tab Title", group.saved_guid(),
                                       /*position=*/std::nullopt);
      group.AddTabLocally(std::move(tab));
    }
    service()->AddGroup(std::move(group));
  }

  void ExpectGroupTitlesInMenu(const std::vector<std::string>& titles) {
    std::vector<std::string> actual_titles;
    bool found_separator = false;
    for (NSMenuItem* item in [menu() itemArray]) {
      if (item.tag == IDC_CREATE_NEW_TAB_GROUP) {
        continue;
      }
      if ([item isSeparatorItem]) {
        found_separator = true;
        continue;
      }
      if (item.hasSubmenu) {
        actual_titles.push_back(base::SysNSStringToUTF8(item.title));
      }
    }

    EXPECT_EQ(!titles.empty(), found_separator);

    std::sort(actual_titles.begin(), actual_titles.end());
    std::vector<std::string> sorted_titles = titles;
    std::sort(sorted_titles.begin(), sorted_titles.end());

    ASSERT_EQ(sorted_titles.size(), actual_titles.size());
    for (size_t i = 0; i < sorted_titles.size(); ++i) {
      EXPECT_EQ(sorted_titles[i], actual_titles[i]);
    }
  }

  std::unique_ptr<tab_groups::TabGroupSyncService> service_;
  NSMenu* __strong main_menu_;
  NSMenuItem* __strong tab_groups_menu_root_;
  NSMenu* __strong tab_groups_menu_;
};

TEST_F(TabGroupMenuBridgeTest, CreatesBlankMenu) {
  TabGroupMenuBridge bridge(profile(), service());
  bridge.BuildMenu();
  // Only the static "New Tab Group" item should be present.
  EXPECT_EQ(1, menu().numberOfItems);
  ExpectGroupTitlesInMenu({});
}

TEST_F(TabGroupMenuBridgeTest, TracksGroupUpdates) {
  TabGroupMenuBridge bridge(profile(), service());
  bridge.BuildMenu();

  AddGroup(u"Group 1", tab_groups::TabGroupColorId::kGrey,
           {GURL("https://a.com")});
  ExpectGroupTitlesInMenu({"Group 1"});

  AddGroup(u"Group 2", tab_groups::TabGroupColorId::kBlue,
           {GURL("https://b.com")});
  ExpectGroupTitlesInMenu({"Group 1", "Group 2"});

  const auto& groups_before_remove = service()->GetAllGroups();
  ASSERT_EQ(2u, groups_before_remove.size());
  base::Uuid group1_uuid = groups_before_remove[0].saved_guid();
  if (groups_before_remove[0].title() != u"Group 1") {
    group1_uuid = groups_before_remove[1].saved_guid();
  }
  service()->RemoveGroup(group1_uuid);
  ExpectGroupTitlesInMenu({"Group 2"});
}

TEST_F(TabGroupMenuBridgeTest, SubmenuHasCorrectItems) {
  TabGroupMenuBridge bridge(profile(), service());
  AddGroup(u"Group 1", tab_groups::TabGroupColorId::kGrey,
           {GURL("https://a.com"), GURL("https://b.com")});
  bridge.BuildMenu();

  ExpectGroupTitlesInMenu({"Group 1"});
  NSMenuItem* group_item = [menu() itemWithTitle:@"Group 1"];
  ASSERT_TRUE(group_item);
  ASSERT_TRUE(group_item.hasSubmenu);
  NSMenu* submenu = group_item.submenu;

  // Expected items:
  // 0: Open in Browser
  // 1: Open/Move to New Window
  // 2: Pin/Unpin
  // 3: Delete/Leave
  // 4: --- separator ---
  // 5: Tab 1
  // 6: Tab 2
  EXPECT_EQ(7, submenu.numberOfItems);
  EXPECT_EQ(base::SysNSStringToUTF16([[submenu itemAtIndex:0] title]),
            l10n_util::GetStringUTF16(IDS_OPEN_GROUP_IN_BROWSER_MENU));
  EXPECT_EQ(base::SysNSStringToUTF16([[submenu itemAtIndex:1] title]),
            l10n_util::GetStringUTF16(
                IDS_TAB_GROUP_HEADER_CXMENU_OPEN_GROUP_IN_NEW_WINDOW));
  EXPECT_EQ(base::SysNSStringToUTF16([[submenu itemAtIndex:2] title]),
            l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_PIN_GROUP));
  EXPECT_EQ(
      base::SysNSStringToUTF16([[submenu itemAtIndex:3] title]),
      l10n_util::GetStringUTF16(IDS_TAB_GROUP_HEADER_CXMENU_DELETE_GROUP));
  EXPECT_TRUE([[submenu itemAtIndex:4] isSeparatorItem]);
  EXPECT_EQ(base::SysNSStringToUTF8([[submenu itemAtIndex:5] title]),
            "Tab Title");
  EXPECT_EQ(base::SysNSStringToUTF8([[submenu itemAtIndex:6] title]),
            "Tab Title");
}

TEST_F(TabGroupMenuBridgeTest, ClickingTabOpensUrl) {
  TabGroupMenuBridge bridge(profile(), service());

  AddGroup(u"Group 1", tab_groups::TabGroupColorId::kGrey,
           {GURL("https://a.com")});

  NSMenuItem* group_item = [menu() itemWithTitle:@"Group 1"];
  ASSERT_TRUE(group_item);
  NSMenu* submenu = group_item.submenu;
  NSMenuItem* tab_item = nil;
  for (NSMenuItem* item in [submenu itemArray]) {
    if ([item.title isEqualToString:@"Tab Title"]) {
      tab_item = item;
      break;
    }
  }
  ASSERT_TRUE(tab_item);

  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  [submenu performActionForItemAtIndex:[submenu indexOfItem:tab_item]];

  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* new_tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  EXPECT_EQ(GURL("https://a.com"), new_tab->GetVisibleURL());
}
