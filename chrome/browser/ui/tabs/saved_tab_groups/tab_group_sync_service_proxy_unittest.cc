// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace tab_groups {
class TabGroupSyncServiceProxyUnitTest
    : public TestWithBrowserView,
      public ::testing::WithParamInterface<bool> {
 public:
  TabGroupSyncServiceProxyUnitTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(tab_groups::kTabGroupsSaveV2);
    enabled_features.push_back(tab_groups::kTabGroupsSaveUIUpdate);

    if (IsMigrationEnabled()) {
      enabled_features.push_back(
          tab_groups::kTabGroupSyncServiceDesktopMigration);
    }

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~TabGroupSyncServiceProxyUnitTest() = default;
  TabGroupSyncServiceProxyUnitTest(const TabGroupSyncServiceProxyUnitTest&) =
      delete;
  TabGroupSyncServiceProxyUnitTest& operator=(
      const TabGroupSyncServiceProxyUnitTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::mojom::WindowShowState::kDefault;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  content::WebContents* AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr);

    content::WebContents* web_contents_ptr = web_contents.get();

    browser->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  bool IsMigrationEnabled() { return GetParam(); }

  TabGroupSyncService* service() { return service_.get(); }

  // Return a distant tab at position 0 with the "first" ids.
  SavedTabGroupTab FirstTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kFirstTabURL, kFirstTabTitle, group_guid,
                            std::make_optional(0), kFirstTabId, kFirstTabToken);
  }

  // Return a distant tab at position 1 with the "second" ids.
  SavedTabGroupTab SecondTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kSecondTabURL, kSecondTabTitle, group_guid,
                            std::make_optional(1), kSecondTabId,
                            kSecondTabToken);
  }

  // Return a distant tab at position 2 with the "third" ids.
  SavedTabGroupTab ThirdTab(base::Uuid group_guid) {
    return SavedTabGroupTab(kThirdTabURL, kThirdTabTitle, group_guid,
                            std::make_optional(2), kThirdTabId, kThirdTabToken);
  }

  const std::u16string kGroupTitle = u"Test Group Title";
  const TabGroupColorId kGroupColor = TabGroupColorId::kGrey;
  const base::Uuid kGroupId = base::Uuid::GenerateRandomV4();
  const base::Uuid kFirstTabId = base::Uuid::GenerateRandomV4();
  const base::Uuid kSecondTabId = base::Uuid::GenerateRandomV4();
  const base::Uuid kThirdTabId = base::Uuid::GenerateRandomV4();
  const LocalTabID kFirstTabToken = base::Token::CreateRandom();
  const LocalTabID kSecondTabToken = base::Token::CreateRandom();
  const LocalTabID kThirdTabToken = base::Token::CreateRandom();
  const std::u16string kFirstTabTitle = u"first tab";
  const std::u16string kSecondTabTitle = u"second tab";
  const std::u16string kThirdTabTitle = u"third tab";
  const GURL kFirstTabURL = GURL("https://first_tab.com");
  const GURL kSecondTabURL = GURL("https://second_tab.com");
  const GURL kThirdTabURL = GURL("https://third_tab.com");

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();

    service_ =
        tab_groups::SavedTabGroupUtils::GetServiceForProfile(profile_.get());
    service_->SetIsInitializedForTesting(true);
  }

  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::vector<std::unique_ptr<Browser>> browsers_;
  raw_ptr<TabGroupSyncService> service_ = nullptr;
};

// Verify we can add a group to both services correctly.
TEST_P(TabGroupSyncServiceProxyUnitTest, AddGroup) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0, 1, 2});

  const std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_id);
  ASSERT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(local_id, retrieved_group->local_group_id());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());
}

// Verify we can remove a group from the services using the local id correctly.
TEST_P(TabGroupSyncServiceProxyUnitTest, RemoveGroupUsingLocalId) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  const base::Uuid sync_id = retrieved_group->saved_guid();
  service()->RemoveGroup(local_id);
  EXPECT_FALSE(service()->GetGroup(local_id).has_value());
  EXPECT_FALSE(service()->GetGroup(sync_id).has_value());
}

// Verify we can remove a group from the services using the sync id correctly.
TEST_P(TabGroupSyncServiceProxyUnitTest, RemoveGroupUsingSyncId) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  const base::Uuid sync_id = retrieved_group->saved_guid();
  service()->RemoveGroup(sync_id);
  EXPECT_FALSE(service()->GetGroup(local_id).has_value());
  EXPECT_FALSE(service()->GetGroup(sync_id).has_value());
}

// Verify we can update a groups visual data  from the services correctly.
TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateVisualData) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  const std::u16string new_title = u"New Title";
  const TabGroupColorId new_color = TabGroupColorId::kCyan;
  TabGroupVisualData new_visual_data(new_title, new_color);
  service()->UpdateVisualData(local_id, &new_visual_data);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(new_title, retrieved_group->title());
  EXPECT_EQ(new_color, retrieved_group->color());
}

TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateGroupPositionPinnedState) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  TabGroupId local_id = browser->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  const bool pinned_state = retrieved_group->is_pinned();
  service()->UpdateGroupPosition(retrieved_group->saved_guid(), !pinned_state,
                                 std::nullopt);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_NE(retrieved_group->is_pinned(), pinned_state);

  service()->UpdateGroupPosition(retrieved_group->saved_guid(), pinned_state,
                                 std::nullopt);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_EQ(retrieved_group->is_pinned(), pinned_state);
}

TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateGroupPositionIndex) {
  auto get_index = [&](const LocalTabGroupID& local_id) -> int {
    std::vector<SavedTabGroup> groups = service()->GetAllGroups();
    auto it = base::ranges::find_if(groups, [&](const SavedTabGroup& group) {
      return group.local_group_id() == local_id;
    });

    if (it == groups.end()) {
      return -1;
    }

    return std::distance(groups.begin(), it);
  };

  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  TabGroupId local_id_1 = browser->tab_strip_model()->AddToNewGroup({0});
  TabGroupId local_id_2 = browser->tab_strip_model()->AddToNewGroup({1});
  TabGroupId local_id_3 = browser->tab_strip_model()->AddToNewGroup({2});

  // Ensure the group was saved.
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_id_1);
  const base::Uuid& sync_id = retrieved_group->saved_guid();

  std::vector<SavedTabGroup> all_groups = service()->GetAllGroups();
  ASSERT_EQ(3u, all_groups.size());
  EXPECT_EQ(0, get_index(local_id_3));
  EXPECT_EQ(1, get_index(local_id_2));
  EXPECT_EQ(2, get_index(local_id_1));

  service()->UpdateGroupPosition(sync_id, std::nullopt, 0);
  EXPECT_EQ(0, get_index(local_id_1));
  EXPECT_EQ(1, get_index(local_id_3));
  EXPECT_EQ(2, get_index(local_id_2));

  service()->UpdateGroupPosition(sync_id, std::nullopt, 1);
  EXPECT_EQ(0, get_index(local_id_3));
  EXPECT_EQ(1, get_index(local_id_1));
  EXPECT_EQ(2, get_index(local_id_2));

  service()->UpdateGroupPosition(sync_id, std::nullopt, 2);
  EXPECT_EQ(0, get_index(local_id_3));
  EXPECT_EQ(1, get_index(local_id_2));
  EXPECT_EQ(2, get_index(local_id_1));
}

// Verifies that we add tabs to a group at the correct position.
TEST_P(TabGroupSyncServiceProxyUnitTest, AddTab) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());

  const base::Uuid sync_id = retrieved_group->saved_guid();
  SavedTabGroupTab second_tab = SecondTab(sync_id);
  SavedTabGroupTab third_tab = ThirdTab(sync_id);
  service()->AddTab(local_id, kSecondTabToken, second_tab.title(),
                    second_tab.url(), 0);
  service()->AddTab(local_id, kThirdTabToken, third_tab.title(),
                    third_tab.url(), 2);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->ContainsTab(kSecondTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kThirdTabToken));

  // Get the order of tabs.
  const std::vector<SavedTabGroupTab> tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kSecondTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kThirdTabToken, tabs[2].local_tab_id());
}

// Verifies that we can update the title and url of a tab in a  saved group.
TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateTab) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->saved_tabs()[0].local_tab_id().has_value());

  const LocalTabID tab_id =
      retrieved_group->saved_tabs()[0].local_tab_id().value();
  const base::Uuid sync_tab_id =
      retrieved_group->saved_tabs()[0].saved_tab_guid();

  const std::u16string new_title = u"This is the new title";
  GURL new_url = GURL("https://not_first_tab.com");

  SavedTabGroupTabBuilder tab_builder;
  tab_builder.SetTitle(new_title);
  tab_builder.SetURL(new_url);
  service()->UpdateTab(local_id, tab_id, std::move(tab_builder));
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->ContainsTab(sync_tab_id));

  const SavedTabGroupTab* retrieved_tab = retrieved_group->GetTab(sync_tab_id);
  EXPECT_EQ(new_title, retrieved_tab->title());
  EXPECT_EQ(new_url, retrieved_tab->url());
}

// Verifies that we can remove a tab in a group and that after removing all of
// the tabs, the group is deleted.
TEST_P(TabGroupSyncServiceProxyUnitTest, RemoveTab) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0, 1, 2});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());

  const LocalTabID tab_1_id =
      retrieved_group->saved_tabs()[0].local_tab_id().value();
  const LocalTabID tab_2_id =
      retrieved_group->saved_tabs()[1].local_tab_id().value();
  const LocalTabID tab_3_id =
      retrieved_group->saved_tabs()[2].local_tab_id().value();

  // Remove the first tab: [ Tab 1, Tab 2, Tab 3 ] -> [ Tab 2, Tab 3]
  service()->RemoveTab(local_id, tab_1_id);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(2u, retrieved_group->saved_tabs().size());
  EXPECT_FALSE(retrieved_group->ContainsTab(tab_1_id));
  EXPECT_TRUE(retrieved_group->ContainsTab(tab_2_id));
  EXPECT_TRUE(retrieved_group->ContainsTab(tab_3_id));

  // Remove the third tab: [ Tab 2, Tab 3 ] -> [ Tab 2 ]
  service()->RemoveTab(local_id, tab_3_id);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_FALSE(retrieved_group->ContainsTab(tab_1_id));
  EXPECT_TRUE(retrieved_group->ContainsTab(tab_2_id));
  EXPECT_FALSE(retrieved_group->ContainsTab(tab_3_id));

  // Remove the second tab. This should delete the group.
  const base::Uuid sync_id = retrieved_group->saved_guid();
  service()->RemoveTab(local_id, tab_2_id);

  retrieved_group = service()->GetGroup(sync_id);
  EXPECT_FALSE(retrieved_group.has_value());
}

// Verifies that we can move the tabs in a saved group correctly.
TEST_P(TabGroupSyncServiceProxyUnitTest, MoveTab) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0, 1, 2});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());

  const LocalTabID tab_1_id =
      retrieved_group->saved_tabs()[0].local_tab_id().value();
  const LocalTabID tab_2_id =
      retrieved_group->saved_tabs()[1].local_tab_id().value();
  const LocalTabID tab_3_id =
      retrieved_group->saved_tabs()[2].local_tab_id().value();

  // Move tab 3 to the front: [ Tab 1, Tab 2, Tab 3 ] -> [ Tab 3, Tab 1, Tab 2]
  service()->MoveTab(local_id, tab_3_id, 0);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  std::vector<SavedTabGroupTab> tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(tab_3_id, tabs[0].local_tab_id());
  EXPECT_EQ(tab_1_id, tabs[1].local_tab_id());
  EXPECT_EQ(tab_2_id, tabs[2].local_tab_id());

  // Move tab 2 to the middle: [ Tab 3, Tab 1, Tab 2 ] -> [ Tab 3, Tab 2, Tab 1]
  service()->MoveTab(local_id, tab_2_id, 1);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(tab_3_id, tabs[0].local_tab_id());
  EXPECT_EQ(tab_2_id, tabs[1].local_tab_id());
  EXPECT_EQ(tab_1_id, tabs[2].local_tab_id());

  // Move tab 1 to the front: [ Tab 3, Tab 2, Tab 1 ] -> [ Tab 1, Tab 3, Tab 2]
  service()->MoveTab(local_id, tab_1_id, 0);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(tab_1_id, tabs[0].local_tab_id());
  EXPECT_EQ(tab_3_id, tabs[1].local_tab_id());
  EXPECT_EQ(tab_2_id, tabs[2].local_tab_id());

  // Move tab 3 to the end: [ Tab 1, Tab 3, Tab 2 ] -> [ Tab 1, Tab 2, Tab 3]
  service()->MoveTab(local_id, tab_3_id, 2);
  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(tab_1_id, tabs[0].local_tab_id());
  EXPECT_EQ(tab_2_id, tabs[1].local_tab_id());
  EXPECT_EQ(tab_3_id, tabs[2].local_tab_id());
}

// Verifies that we can update the local tab group mapping of a saved group
// after it is added to the service.
TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateLocalTabGroupMapping) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());

  const base::Uuid sync_id = retrieved_group->saved_guid();

  tab_groups::TabGroupId new_local_id = tab_groups::TabGroupId::GenerateNew();
  service()->UpdateLocalTabGroupMapping(sync_id, new_local_id,
                                        OpeningSource::kOpenedFromRevisitUi);

  retrieved_group = service()->GetGroup(sync_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(new_local_id, retrieved_group->local_group_id());
}

// Verifies that we can remove the local tab group mapping of a saved group
// after it is added to the service.
TEST_P(TabGroupSyncServiceProxyUnitTest, RemoveLocalTabGroupMapping) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());

  const base::Uuid sync_id = retrieved_group->saved_guid();
  service()->RemoveLocalTabGroupMapping(local_id, ClosingSource::kClosedByUser);

  retrieved_group = service()->GetGroup(sync_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(std::nullopt, retrieved_group->local_group_id());
}

// Verifies that we can update the local tab id mapping for a tab in a saved
// group after it is added to the service.
TEST_P(TabGroupSyncServiceProxyUnitTest, UpdateLocalTabId) {
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);

  tab_groups::TabGroupId local_id =
      browser->tab_strip_model()->AddToNewGroup({0});

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());

  const LocalTabID tab_1_id =
      retrieved_group->saved_tabs()[0].local_tab_id().value();
  const base::Uuid tab_1_guid =
      retrieved_group->saved_tabs()[0].saved_tab_guid();

  LocalTabID new_local_tab_id = base::Token::CreateRandom();
  service()->UpdateLocalTabId(local_id, tab_1_guid, new_local_tab_id);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->ContainsTab(new_local_tab_id));
  EXPECT_FALSE(retrieved_group->ContainsTab(tab_1_id));
}

// Verifies that when a new tab group is created in the browser it is saved by
// default. When it is closed, the group should still be saved but no longer
// have a local id.
TEST_P(TabGroupSyncServiceProxyUnitTest, DefaultSaveNewGroups) {
  EXPECT_EQ(0u, service()->GetAllGroups().size());

  // Add some tabs and create a single tab group.
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  TabGroupId local_group_id = browser->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  EXPECT_TRUE(retrieved_group.has_value());

  base::Uuid saved_id = retrieved_group->saved_guid();

  // Ensure the group is still saved but no longer references `local_group_id`.
  browser->tab_strip_model()->CloseAllTabsInGroup(local_group_id);
  EXPECT_FALSE(service()->GetGroup(local_group_id).has_value());
  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(std::nullopt, retrieved_group->local_group_id());
}

// Verifies that opening a saved group in the same window properly opens it and
// associates the local id with the saved id.
TEST_P(TabGroupSyncServiceProxyUnitTest, OpenTabGroupInSameWindow) {
  EXPECT_EQ(0u, service()->GetAllGroups().size());

  // Add some tabs and create a single tab group.
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  TabGroupId local_group_id = browser->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  EXPECT_TRUE(retrieved_group.has_value());

  base::Uuid saved_id = retrieved_group->saved_guid();

  // Ensure the group is still saved but no longer references `local_group_id`.
  browser->tab_strip_model()->CloseAllTabsInGroup(local_group_id);
  EXPECT_FALSE(browser->tab_strip_model()->group_model()->ContainsTabGroup(
      local_group_id));

  std::unique_ptr<TabGroupActionContextDesktop> desktop_context =
      std::make_unique<TabGroupActionContextDesktop>(browser,
                                                     OpeningSource::kUnknown);
  service()->OpenTabGroup(saved_id, std::move(desktop_context));

  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->local_group_id().has_value());
  EXPECT_NE(local_group_id, retrieved_group->local_group_id());
  EXPECT_TRUE(browser->tab_strip_model()->group_model()->ContainsTabGroup(
      retrieved_group->local_group_id().value()));
}

// Verifies that opening a saved group in a different window properly opens it
// and associates the local id with the saved id.
TEST_P(TabGroupSyncServiceProxyUnitTest, OpenTabGroupInDifferentWindow) {
  EXPECT_EQ(0u, service()->GetAllGroups().size());

  // Add some tabs and create a single tab group.
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  TabGroupId local_group_id = browser->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  EXPECT_TRUE(retrieved_group.has_value());

  base::Uuid saved_id = retrieved_group->saved_guid();

  // Ensure the group is still saved but no longer references `local_group_id`.
  browser->tab_strip_model()->CloseAllTabsInGroup(local_group_id);
  EXPECT_FALSE(browser->tab_strip_model()->group_model()->ContainsTabGroup(
      local_group_id));

  // Create a second browser to open the group into.
  Browser* browser_2 = AddBrowser();
  AddTabToBrowser(browser_2, 0);

  std::unique_ptr<TabGroupActionContextDesktop> desktop_context =
      std::make_unique<TabGroupActionContextDesktop>(browser_2,
                                                     OpeningSource::kUnknown);
  service()->OpenTabGroup(saved_id, std::move(desktop_context));

  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->local_group_id().has_value());
  EXPECT_NE(local_group_id, retrieved_group->local_group_id());
  EXPECT_FALSE(browser->tab_strip_model()->group_model()->ContainsTabGroup(
      retrieved_group->local_group_id().value()));
  EXPECT_TRUE(browser_2->tab_strip_model()->group_model()->ContainsTabGroup(
      retrieved_group->local_group_id().value()));
}

// Verifies that opening a saved group that is already open will focus the
// first tab in the group instead of opening a new one.
TEST_P(TabGroupSyncServiceProxyUnitTest,
       OpenTabGroupFocusFirstTabIfOpenedAlready) {
  EXPECT_EQ(0u, service()->GetAllGroups().size());

  // Add some tabs and create a single tab group.
  Browser* browser = AddBrowser();
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);

  TabGroupId local_group_id = browser->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  EXPECT_TRUE(retrieved_group.has_value());

  base::Uuid saved_id = retrieved_group->saved_guid();

  // Ensure the non-grouped tab is focused.
  browser->tab_strip_model()->ActivateTabAt(1);
  EXPECT_NE(0, browser->tab_strip_model()->active_index());

  std::unique_ptr<TabGroupActionContextDesktop> desktop_context =
      std::make_unique<TabGroupActionContextDesktop>(browser,
                                                     OpeningSource::kUnknown);
  service()->OpenTabGroup(saved_id, std::move(desktop_context));

  // The tab group should now have the active index.
  EXPECT_EQ(0, browser->tab_strip_model()->active_index());

  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->local_group_id().has_value());
  EXPECT_EQ(local_group_id, retrieved_group->local_group_id());
  EXPECT_TRUE(browser->tab_strip_model()->group_model()->ContainsTabGroup(
      retrieved_group->local_group_id().value()));
}

INSTANTIATE_TEST_SUITE_P(TabGroupSyncServiceProxy,
                         TabGroupSyncServiceProxyUnitTest,
                         testing::Bool());

}  // namespace tab_groups
