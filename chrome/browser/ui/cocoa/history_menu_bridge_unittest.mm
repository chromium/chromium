// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/cocoa/history_menu_bridge.h"

#import <Cocoa/Cocoa.h>

#include <initializer_list>
#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/app_controller_mac.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/sessions/chrome_tab_restore_service_client.h"
#include "chrome/browser/sessions/tab_restore_service_factory.h"
#include "chrome/browser/ui/cocoa/test/cocoa_test_helper.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/favicon_base/favicon_types.h"
#include "components/sessions/content/content_test_helper.h"
#include "components/sessions/core/serialized_navigation_entry_test_helper.h"
#include "components/sessions/core/tab_restore_service_impl.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"

namespace {

class MockTRS : public sessions::TabRestoreServiceImpl {
 public:
  explicit MockTRS(Profile* profile)
      : sessions::TabRestoreServiceImpl(
            std::make_unique<ChromeTabRestoreServiceClient>(profile),
            profile->GetPrefs(),
            nullptr) {}
  MOCK_CONST_METHOD0(entries, const sessions::TabRestoreService::Entries&());
};

sessions::tab_restore::Tab* CreateSessionTab(SessionID::id_type id,
                                             const std::string& url,
                                             const std::string& title) {
  auto* tab = new sessions::tab_restore::Tab;
  tab->id = SessionID::FromSerializedValue(id);
  tab->current_navigation_index = 0;
  tab->navigations.push_back(
      sessions::ContentTestHelper::CreateNavigation(url, title));
  return tab;
}

MockTRS::Entries CreateSessionEntries(
    std::initializer_list<sessions::tab_restore::Entry*> entries) {
  MockTRS::Entries ret;
  for (auto* entry : entries)
    ret.emplace_back(entry);
  return ret;
}

sessions::tab_restore::Window* CreateSessionWindow(
    SessionID::id_type id,
    std::initializer_list<sessions::tab_restore::Tab*> tabs) {
  auto* window = new sessions::tab_restore::Window;
  window->id = SessionID::FromSerializedValue(id);
  window->tabs.reserve(tabs.size());
  for (auto* tab : tabs)
    window->tabs.emplace_back(std::move(tab));
  return window;
}

sessions::tab_restore::Group* CreateSessionGroup(
    SessionID::id_type id,
    tab_groups::TabGroupVisualData visual_data,
    std::initializer_list<sessions::tab_restore::Tab*> tabs) {
  auto* group = new sessions::tab_restore::Group;
  group->id = SessionID::FromSerializedValue(id);
  group->visual_data = visual_data;
  group->tabs.reserve(tabs.size());
  for (auto* tab : tabs)
    group->tabs.emplace_back(std::move(tab));
  return group;
}

std::unique_ptr<HistoryMenuBridge::HistoryItem> CreateItem(
    const std::u16string& title) {
  auto item = std::make_unique<HistoryMenuBridge::HistoryItem>();
  item->title = title;
  item->url = GURL(title);
  return item;
}

class MockBridge : public HistoryMenuBridge {
 public:
  explicit MockBridge(Profile* profile)
      : HistoryMenuBridge(profile),
        menu_([[NSMenu alloc] initWithTitle:@"History"]) {}

  NSMenu* HistoryMenu() override { return menu_; }

 private:
  NSMenu* __strong menu_;
};

class HistoryMenuBridgeTest : public BrowserWithTestWindowTest {
 public:
  bool ShouldMenuItemBeVisible(NSMenuItem* item) {
    return bridge_->ShouldMenuItemBeVisible(item);
  }

 protected:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    [AppController.sharedController setLastProfileForTesting:profile()];

    bridge_ = std::make_unique<MockBridge>(profile());
  }

  void TearDown() override {
    bridge_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() override {
    return {TestingProfile::TestingFactory{
                FaviconServiceFactory::GetInstance(),
                FaviconServiceFactory::GetDefaultFactory()},
            TestingProfile::TestingFactory{
                HistoryServiceFactory::GetInstance(),
                HistoryServiceFactory::GetDefaultFactory()}};
  }

  // We are a friend of HistoryMenuBridge (and have access to
  // protected methods), but none of the classes generated by TEST_F()
  // are. Wraps common commands.
  void ClearMenuSection(NSMenu* menu,
                        NSInteger tag) {
    bridge_->ClearMenuSection(menu, tag);
  }

  void AddItemToBridgeMenu(std::unique_ptr<HistoryMenuBridge::HistoryItem> item,
                           NSMenu* menu,
                           NSInteger tag,
                           NSInteger index) {
    bridge_->AddItemToMenu(std::move(item), menu, tag, index);
  }

  NSMenuItem* AddItemToMenu(NSMenu* menu,
                            NSString* title,
                            SEL selector,
                            int tag) {
    NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:nullptr
                                           keyEquivalent:@""];
    [item setTag:tag];
    if (selector) {
      [item setAction:selector];
      [item setTarget:bridge_->controller_];
    }
    [menu addItem:item];
    return item;
  }

  void GetFaviconForHistoryItem(HistoryMenuBridge::HistoryItem* item) {
    bridge_->GetFaviconForHistoryItem(item);
  }

  void GotFaviconData(HistoryMenuBridge::HistoryItem* item,
                      const favicon_base::FaviconImageResult& image_result) {
    bridge_->GotFaviconData(item, image_result);
  }

  void CancelFaviconRequest(HistoryMenuBridge::HistoryItem* item) {
    bridge_->CancelFaviconRequest(item);
  }

  const std::map<NSMenuItem*, std::unique_ptr<HistoryMenuBridge::HistoryItem>>&
  menu_item_map() {
    return bridge_->menu_item_map_;
  }

 private:
  CocoaTestHelper cocoa_test_helper_;

 protected:
  std::unique_ptr<MockBridge> bridge_;
};

class HistoryMenuBridgeLifetimeTest : public testing::Test {
 public:
  NSMenu* HistoryMenu(HistoryMenuBridge* bridge) {
    return bridge->HistoryMenu();
  }

  const std::map<NSMenuItem*, std::unique_ptr<HistoryMenuBridge::HistoryItem>>&
  menu_item_map(HistoryMenuBridge* bridge) {
    return bridge->menu_item_map_;
  }

  void AddItemToBridgeMenu(HistoryMenuBridge* bridge,
                           std::unique_ptr<HistoryMenuBridge::HistoryItem> item,
                           NSMenu* menu,
                           NSInteger tag,
                           NSInteger index) {
    bridge->AddItemToMenu(std::move(item), menu, tag, index);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
};

void CheckMenuItemVisibility(HistoryMenuBridgeTest* test, bool is_incognito) {
  // Make sure the items belong to both original and incognito mode are visible.
  NSInteger always_visible_items[] = {IDC_HOME, IDC_BACK, IDC_FORWARD};
  for (size_t i = 0; i < std::size(always_visible_items); i++) {
    // Create a fake item with tag.
    NSMenuItem* item = [[NSMenuItem alloc] init];
    item.tag = always_visible_items[i];
    EXPECT_TRUE(test->ShouldMenuItemBeVisible(item));
  }

  // Check visibility of items belong to regular mode. They should be visible
  // for regular mode, not for incognito mode.
  NSInteger regular_visible_items[] = {
      HistoryMenuBridge::kRecentlyClosedSeparator,
      HistoryMenuBridge::kRecentlyClosedTitle,
      HistoryMenuBridge::kVisitedSeparator,
      HistoryMenuBridge::kVisitedTitle,
      HistoryMenuBridge::kShowFullSeparator,
      IDC_SHOW_HISTORY};
  for (size_t i = 0; i < std::size(regular_visible_items); i++) {
    // Create a fake item with tag.
    NSMenuItem* item = [[NSMenuItem alloc] init];
    item.tag = regular_visible_items[i];
    EXPECT_EQ(!is_incognito, test->ShouldMenuItemBeVisible(item));
  }
}

// Edge case test for clearing until the end of a menu.
TEST_F(HistoryMenuBridgeTest, ClearHistoryMenuUntilEnd) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"history foo"];
  AddItemToMenu(menu, @"HEADER", nullptr, HistoryMenuBridge::kVisitedTitle);

  NSInteger tag = HistoryMenuBridge::kVisited;
  AddItemToMenu(menu, @"alpha", @selector(openHistoryMenuItem:), tag);
  AddItemToMenu(menu, @"bravo", @selector(openHistoryMenuItem:), tag);
  AddItemToMenu(menu, @"charlie", @selector(openHistoryMenuItem:), tag);
  AddItemToMenu(menu, @"delta", @selector(openHistoryMenuItem:), tag);

  ClearMenuSection(menu, HistoryMenuBridge::kVisited);

  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"HEADER",
      [[menu itemWithTag:HistoryMenuBridge::kVisitedTitle] title]);
}

// Skip menu items that are not hooked up to |-openHistoryMenuItem:|.
TEST_F(HistoryMenuBridgeTest, ClearHistoryMenuSkipping) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"history foo"];
  AddItemToMenu(menu, @"HEADER", nullptr, HistoryMenuBridge::kVisitedTitle);

  NSInteger tag = HistoryMenuBridge::kVisited;
  AddItemToMenu(menu, @"alpha", @selector(openHistoryMenuItem:), tag);
  AddItemToMenu(menu, @"bravo", @selector(openHistoryMenuItem:), tag);
  AddItemToMenu(menu, @"TITLE", nullptr,
                HistoryMenuBridge::kRecentlyClosedTitle);
  AddItemToMenu(menu, @"charlie", @selector(openHistoryMenuItem:), tag);

  ClearMenuSection(menu, tag);

  EXPECT_EQ(2, [menu numberOfItems]);
  EXPECT_NSEQ(@"HEADER",
      [[menu itemWithTag:HistoryMenuBridge::kVisitedTitle] title]);
  EXPECT_NSEQ(@"TITLE",
      [[menu itemAtIndex:1] title]);
}

// Edge case test for clearing an empty menu.
TEST_F(HistoryMenuBridgeTest, ClearHistoryMenuEmpty) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"history foo"];
  AddItemToMenu(menu, @"HEADER", nullptr, HistoryMenuBridge::kVisited);

  ClearMenuSection(menu, HistoryMenuBridge::kVisited);

  EXPECT_EQ(1, [menu numberOfItems]);
  EXPECT_NSEQ(@"HEADER",
      [[menu itemWithTag:HistoryMenuBridge::kVisited] title]);
}

// Test that AddItemToMenu() properly adds HistoryItem objects as menus.
TEST_F(HistoryMenuBridgeTest, AddItemToMenu) {
  NSMenu* menu = [[NSMenu alloc] initWithTitle:@"history foo"];

  const std::u16string short_url = u"http://foo/";
  const std::u16string long_url =
      u"http://super-duper-long-url--."
      u"that.cannot.possibly.fit.even-in-80-columns"
      u"or.be.reasonably-displayed-in-a-menu"
      u"without.looking-ridiculous.com/";  // 140 chars total

  AddItemToBridgeMenu(CreateItem(short_url), menu, 100, 0);
  AddItemToBridgeMenu(CreateItem(long_url), menu, 101, 1);

  EXPECT_EQ(2, [menu numberOfItems]);

  EXPECT_EQ(@selector(openHistoryMenuItem:), [[menu itemAtIndex:0] action]);
  EXPECT_EQ(@selector(openHistoryMenuItem:), [[menu itemAtIndex:1] action]);

  EXPECT_EQ(100, [[menu itemAtIndex:0] tag]);
  EXPECT_EQ(101, [[menu itemAtIndex:1] tag]);

  // Make sure a short title looks fine
  NSString* s = [[menu itemAtIndex:0] title];
  EXPECT_EQ(base::SysNSStringToUTF16(s), short_url);

  // Make sure a super-long title gets trimmed
  s = [[menu itemAtIndex:0] title];
  EXPECT_TRUE([s length] < long_url.length());

  // Confirm tooltips and confirm they are not trimmed (like the item
  // name might be).  Add tolerance for URL fixer-upping;
  // e.g. http://foo becomes http://foo/)
  EXPECT_GE([[[menu itemAtIndex:0] toolTip] length], (2*short_url.length()-5));
  EXPECT_GE([[[menu itemAtIndex:1] toolTip] length], (2*long_url.length()-5));
}

// Test that the menu is created for a set of simple tabs.
TEST_F(HistoryMenuBridgeTest, RecentlyClosedTabs) {
  std::unique_ptr<MockTRS> trs(new MockTRS(profile()));
  auto entries{CreateSessionEntries({
    CreateSessionTab(24, "http://google.com", "Google"),
    CreateSessionTab(42, "http://apple.com", "Apple"),
  })};

  using ::testing::ReturnRef;
  EXPECT_CALL(*trs.get(), entries()).WillOnce(ReturnRef(entries));

  bridge_->TabRestoreServiceChanged(trs.get());

  NSMenu* menu = bridge_->HistoryMenu();
  ASSERT_EQ(2U, [[menu itemArray] count]);

  NSMenuItem* item1 = [menu itemAtIndex:0];
  MockBridge::HistoryItem* hist1 = bridge_->HistoryItemForMenuItem(item1);
  EXPECT_TRUE(hist1);
  EXPECT_EQ(24, hist1->session_id.id());
  EXPECT_NSEQ(@"Google", [item1 title]);

  NSMenuItem* item2 = [menu itemAtIndex:1];
  MockBridge::HistoryItem* hist2 = bridge_->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist2);
  EXPECT_EQ(42, hist2->session_id.id());
  EXPECT_NSEQ(@"Apple", [item2 title]);

  EXPECT_EQ(2u, menu_item_map().size());
  ClearMenuSection(menu, HistoryMenuBridge::kRecentlyClosed);
  EXPECT_EQ(0u, menu_item_map().size());
  EXPECT_EQ(0u, [[menu itemArray] count]);
}

// Test that the menu is created for a mix of windows and tabs.
TEST_F(HistoryMenuBridgeTest, RecentlyClosedTabsAndWindows) {
  std::unique_ptr<MockTRS> trs(new MockTRS(profile()));
  auto entries{CreateSessionEntries({
    CreateSessionTab(24, "http://google.com", "Google"),
    CreateSessionWindow(30, {
      CreateSessionTab(31, "http://foo.com", "foo"),
      CreateSessionTab(32, "http://bar.com", "bar"),
    }),
    CreateSessionTab(42, "http://apple.com", "Apple"),
    CreateSessionWindow(50, {
      CreateSessionTab(51, "http://magic.com", "magic"),
      CreateSessionTab(52, "http://goats.com", "goats"),
      CreateSessionTab(53, "http://teleporter.com", "teleporter"),
    }),
  })};

  using ::testing::ReturnRef;
  EXPECT_CALL(*trs.get(), entries()).WillOnce(ReturnRef(entries));

  bridge_->TabRestoreServiceChanged(trs.get());

  NSMenu* menu = bridge_->HistoryMenu();
  ASSERT_EQ(4U, [[menu itemArray] count]);

  NSMenuItem* item1 = [menu itemAtIndex:0];
  MockBridge::HistoryItem* hist1 = bridge_->HistoryItemForMenuItem(item1);
  EXPECT_TRUE(hist1);
  EXPECT_EQ(24, hist1->session_id.id());
  EXPECT_NSEQ(@"Google", [item1 title]);

  NSMenuItem* item2 = [menu itemAtIndex:1];
  MockBridge::HistoryItem* hist2 = bridge_->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist2);
  EXPECT_EQ(30, hist2->session_id.id());
  EXPECT_EQ(2U, hist2->tabs.size());
  // Do not test full menu item title because it is localized. Just verify that
  // it contains the right number of tabs.
  EXPECT_TRUE([[item2 title] containsString:@"2"]);
  NSMenu* submenu1 = [item2 submenu];
  EXPECT_EQ(4U, [[submenu1 itemArray] count]);
  // Do not test Restore All Tabs because it is localized.
  EXPECT_TRUE([[submenu1 itemAtIndex:1] isSeparatorItem]);
  EXPECT_NSEQ(@"foo", [[submenu1 itemAtIndex:2] title]);
  EXPECT_NSEQ(@"bar", [[submenu1 itemAtIndex:3] title]);
  EXPECT_EQ(31, hist2->tabs[0]->session_id.id());
  EXPECT_EQ(32, hist2->tabs[1]->session_id.id());

  NSMenuItem* item3 = [menu itemAtIndex:2];
  MockBridge::HistoryItem* hist3 = bridge_->HistoryItemForMenuItem(item3);
  EXPECT_TRUE(hist3);
  EXPECT_EQ(42, hist3->session_id.id());
  EXPECT_NSEQ(@"Apple", [item3 title]);

  NSMenuItem* item4 = [menu itemAtIndex:3];
  MockBridge::HistoryItem* hist4 = bridge_->HistoryItemForMenuItem(item4);
  EXPECT_TRUE(hist4);
  EXPECT_EQ(50, hist4->session_id.id());
  EXPECT_EQ(3U, hist4->tabs.size());
  // Do not test full menu item title because it is localized. Just verify that
  // it contains the right number of tabs.
  EXPECT_TRUE([[item4 title] containsString:@"3"]);
  NSMenu* submenu2 = [item4 submenu];
  EXPECT_EQ(5U, [[submenu2 itemArray] count]);
  // Do not test Restore All Tabs because it is localized.
  EXPECT_TRUE([[submenu2 itemAtIndex:1] isSeparatorItem]);
  EXPECT_NSEQ(@"magic", [[submenu2 itemAtIndex:2] title]);
  EXPECT_NSEQ(@"goats", [[submenu2 itemAtIndex:3] title]);
  EXPECT_NSEQ(@"teleporter", [[submenu2 itemAtIndex:4] title]);
  EXPECT_EQ(51, hist4->tabs[0]->session_id.id());
  EXPECT_EQ(52, hist4->tabs[1]->session_id.id());
  EXPECT_EQ(53, hist4->tabs[2]->session_id.id());

  // 9 items from |entries|, plus 2 "Restore All Tabs" items, one for each
  // window entry.
  EXPECT_EQ(11u, menu_item_map().size());
  ClearMenuSection(menu, HistoryMenuBridge::kRecentlyClosed);
  EXPECT_EQ(0u, menu_item_map().size());
  EXPECT_EQ(0u, [[menu itemArray] count]);
}

// Test that the menu is created for a set of groups.
TEST_F(HistoryMenuBridgeTest, RecentlyClosedGroups) {
  tab_groups::TabGroupVisualData visual_data1(
      std::u16string(), tab_groups::TabGroupColorId::kGrey);
  tab_groups::TabGroupVisualData visual_data2(
      u"title", tab_groups::TabGroupColorId::kBlue);

  std::unique_ptr<MockTRS> trs(new MockTRS(profile()));
  auto entries{CreateSessionEntries({
      CreateSessionGroup(30, visual_data1,
                         {
                             CreateSessionTab(31, "http://foo.com", "foo"),
                             CreateSessionTab(32, "http://bar.com", "bar"),
                         }),
      CreateSessionGroup(
          50, visual_data2,
          {
              CreateSessionTab(51, "http://magic.com", "magic"),
              CreateSessionTab(52, "http://goats.com", "goats"),
              CreateSessionTab(53, "http://teleporter.com", "teleporter"),
          }),
  })};

  using ::testing::ReturnRef;
  EXPECT_CALL(*trs.get(), entries()).WillOnce(ReturnRef(entries));

  bridge_->TabRestoreServiceChanged(trs.get());

  NSMenu* menu = bridge_->HistoryMenu();
  ASSERT_EQ(2U, [[menu itemArray] count]);

  NSMenuItem* item1 = [menu itemAtIndex:0];
  MockBridge::HistoryItem* hist1 = bridge_->HistoryItemForMenuItem(item1);
  EXPECT_TRUE(hist1);
  EXPECT_EQ(30, hist1->session_id.id());
  EXPECT_EQ(2U, hist1->tabs.size());
  // Do not test full menu item title because it is localized. Just verify that
  // it contains the right number of tabs and no title.
  EXPECT_TRUE([[item1 title] containsString:@"2"]);
  EXPECT_FALSE([[item1 title] containsString:@"title"]);

  NSMenu* submenu1 = [item1 submenu];
  EXPECT_EQ(4U, [[submenu1 itemArray] count]);
  // Do not test Restore All Tabs because it is localized.
  EXPECT_TRUE([[submenu1 itemAtIndex:1] isSeparatorItem]);
  EXPECT_NSEQ(@"foo", [[submenu1 itemAtIndex:2] title]);
  EXPECT_NSEQ(@"bar", [[submenu1 itemAtIndex:3] title]);
  EXPECT_EQ(31, hist1->tabs[0]->session_id.id());
  EXPECT_EQ(32, hist1->tabs[1]->session_id.id());

  NSMenuItem* item2 = [menu itemAtIndex:1];
  MockBridge::HistoryItem* hist2 = bridge_->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist2);
  EXPECT_EQ(50, hist2->session_id.id());
  EXPECT_EQ(3U, hist2->tabs.size());
  // Do not test full menu item title because it is localized. Just verify that
  // it contains the right number of tabs and title.
  EXPECT_TRUE([[item2 title] containsString:@"3"]);
  EXPECT_TRUE([[item2 title] containsString:@"title"]);

  NSMenu* submenu2 = [item2 submenu];
  EXPECT_EQ(5U, [[submenu2 itemArray] count]);
  // Do not test Restore All Tabs because it is localized.
  EXPECT_TRUE([[submenu2 itemAtIndex:1] isSeparatorItem]);
  EXPECT_NSEQ(@"magic", [[submenu2 itemAtIndex:2] title]);
  EXPECT_NSEQ(@"goats", [[submenu2 itemAtIndex:3] title]);
  EXPECT_NSEQ(@"teleporter", [[submenu2 itemAtIndex:4] title]);
  EXPECT_EQ(51, hist2->tabs[0]->session_id.id());
  EXPECT_EQ(52, hist2->tabs[1]->session_id.id());
  EXPECT_EQ(53, hist2->tabs[2]->session_id.id());
}

// Tests that we properly request an icon from the FaviconService.
TEST_F(HistoryMenuBridgeTest, GetFaviconForHistoryItem) {
  // Create a fake item.
  HistoryMenuBridge::HistoryItem item;
  item.title = u"Title";
  item.url = GURL("http://google.com");

  // Request the icon.
  GetFaviconForHistoryItem(&item);

  // Make sure the item was modified properly.
  EXPECT_TRUE(item.icon_requested);
  EXPECT_NE(base::CancelableTaskTracker::kBadTaskId, item.icon_task_id);

  // Cancel the request.
  CancelFaviconRequest(&item);
}

TEST_F(HistoryMenuBridgeTest, GotFaviconData) {
  // Create a dummy bitmap.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(25, 25);
  bitmap.eraseARGB(255, 255, 0, 0);

  // Set up the HistoryItem.
  HistoryMenuBridge::HistoryItem item;
  item.menu_item = [[NSMenuItem alloc] init];
  GetFaviconForHistoryItem(&item);

  // Cancel the request so there will be no race.
  CancelFaviconRequest(&item);

  // Pretend to be called back.
  favicon_base::FaviconImageResult image_result;
  image_result.image = gfx::Image::CreateFrom1xBitmap(bitmap);
  GotFaviconData(&item, image_result);

  // Make sure the callback works.
  EXPECT_FALSE(item.icon_requested);
  EXPECT_TRUE(item.icon);
  EXPECT_TRUE([item.menu_item image]);
}

TEST_F(HistoryMenuBridgeTest, MenuItemVisibilityForRegularMode) {
  CheckMenuItemVisibility(this, false);
}

TEST_F(HistoryMenuBridgeTest, MenuItemVisibilityForIncognitoMode) {
  bridge_ = std::make_unique<MockBridge>(
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true));
  CheckMenuItemVisibility(this, true);
}

// Does a full setup and tear down of the bridge.
TEST_F(HistoryMenuBridgeLifetimeTest, ShutdownAfterProfile) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      TabRestoreServiceFactory::GetInstance(),
      TabRestoreServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();

  auto bridge = std::make_unique<HistoryMenuBridge>(profile.get());
  // Should not crash.
  bridge.reset();
  profile.reset();
}

// Does a full setup and tear down of the bridge.
TEST_F(HistoryMenuBridgeLifetimeTest, ShutdownBeforeProfile) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(
      TabRestoreServiceFactory::GetInstance(),
      TabRestoreServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();

  auto bridge = std::make_unique<HistoryMenuBridge>(profile.get());
  bridge.reset();
  profile.reset();
}

// Initializes the menu, then destroys the Profile but keeps the
// HistoryMenuBridge around.
TEST_F(HistoryMenuBridgeLifetimeTest, StillValidAfterProfileShutdown) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(FaviconServiceFactory::GetInstance(),
                                    FaviconServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(
      TabRestoreServiceFactory::GetInstance(),
      TabRestoreServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  base::FilePath profile_dir = profile->GetPath();
  // Ensure the AppController is the NSApp delegate.
  std::ignore = AppController.sharedController;

  auto bridge = std::make_unique<MockBridge>(profile.get());
  std::unique_ptr<MockTRS> trs(new MockTRS(profile.get()));
  auto entries{CreateSessionEntries({
      CreateSessionTab(24, "http://google.com", "Google"),
      CreateSessionTab(42, "http://apple.com", "Apple"),
  })};

  using ::testing::ReturnRef;
  EXPECT_CALL(*trs.get(), entries()).WillOnce(ReturnRef(entries));

  bridge->TabRestoreServiceChanged(trs.get());

  NSMenu* menu = HistoryMenu(bridge.get());
  AddItemToBridgeMenu(bridge.get(), CreateItem(u"http://example.com/"), menu,
                      100, 2);
  AddItemToBridgeMenu(bridge.get(), CreateItem(u"http://example.org/"), menu,
                      101, 3);

  // Destroy the Profile.
  bridge->OnProfileWillBeDestroyed();
  trs.reset();
  profile.reset();
  ASSERT_EQ(nullptr, bridge->profile());
  EXPECT_EQ(profile_dir, bridge->profile_dir());

  // The menu should still contain items after Profile destruction.
  ASSERT_EQ(4u, [[menu itemArray] count]);

  NSMenuItem* item1 = [menu itemAtIndex:0];
  MockBridge::HistoryItem* hist1 = bridge->HistoryItemForMenuItem(item1);
  EXPECT_TRUE(hist1);
  EXPECT_EQ(24, hist1->session_id.id());
  EXPECT_NSEQ(@"Google", [item1 title]);

  NSMenuItem* item2 = [menu itemAtIndex:1];
  MockBridge::HistoryItem* hist2 = bridge->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist2);
  EXPECT_EQ(42, hist2->session_id.id());
  EXPECT_NSEQ(@"Apple", [item2 title]);

  NSMenuItem* item3 = [menu itemAtIndex:2];
  MockBridge::HistoryItem* hist3 = bridge->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist3);
  EXPECT_NSEQ(@"http://example.com/", [item3 title]);

  NSMenuItem* item4 = [menu itemAtIndex:3];
  MockBridge::HistoryItem* hist4 = bridge->HistoryItemForMenuItem(item2);
  EXPECT_TRUE(hist4);
  EXPECT_NSEQ(@"http://example.org/", [item4 title]);

  EXPECT_EQ(4u, menu_item_map(bridge.get()).size());

  bridge.reset();
}

TEST_F(HistoryMenuBridgeLifetimeTest, EmptyTabRestoreService) {
  TestingProfile::Builder profile_builder;
  profile_builder.AddTestingFactory(FaviconServiceFactory::GetInstance(),
                                    FaviconServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(
      TabRestoreServiceFactory::GetInstance(),
      TabRestoreServiceFactory::GetDefaultFactory());
  profile_builder.AddTestingFactory(HistoryServiceFactory::GetInstance(),
                                    HistoryServiceFactory::GetDefaultFactory());
  std::unique_ptr<TestingProfile> profile = profile_builder.Build();
  base::FilePath profile_dir = profile->GetPath();

  auto bridge = std::make_unique<MockBridge>(profile.get());
  NSMenu* menu = HistoryMenu(bridge.get());

  // Prepopulate the menu with some recently closed item.
  AddItemToBridgeMenu(bridge.get(), CreateItem(u"http://foo/"), menu,
                      HistoryMenuBridge::kRecentlyClosed, 0);
  EXPECT_EQ(1, [menu numberOfItems]);

  // Load an empty `TabRestoreService`. `TabRestoreServiceChanged()` is not
  // called because the service is empty.
  std::unique_ptr<MockTRS> trs(new MockTRS(profile.get()));
  MockTRS::Entries no_entries;
  EXPECT_CALL(*trs.get(), entries()).WillOnce(testing::ReturnRef(no_entries));
  bridge->TabRestoreServiceLoaded(trs.get());

  // Recently closed tabs are removed.
  EXPECT_EQ(0, [menu numberOfItems]);

  bridge.reset();
}

}  // namespace
