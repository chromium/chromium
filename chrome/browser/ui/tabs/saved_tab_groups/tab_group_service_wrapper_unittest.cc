// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_service_wrapper.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "components/saved_tab_groups/features.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
class TabGroupServiceWrapperUnitTest
    : public TestWithBrowserView,
      public ::testing::WithParamInterface<bool> {
 public:
  TabGroupServiceWrapperUnitTest() {
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

  ~TabGroupServiceWrapperUnitTest() = default;
  TabGroupServiceWrapperUnitTest(const TabGroupServiceWrapperUnitTest&) =
      delete;
  TabGroupServiceWrapperUnitTest& operator=(
      const TabGroupServiceWrapperUnitTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::SHOW_STATE_DEFAULT;
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

  TabGroupServiceWrapper* service() { return wrapper_service_.get(); }
  Browser* GetBrowser() { return browser_; }

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
    browser_ = AddBrowser();

    if (IsMigrationEnabled()) {
      wrapper_service_ = std::make_unique<TabGroupServiceWrapper>(
          TabGroupSyncServiceFactory::GetForProfile(browser_->profile()),
          /*saved_tab_group_keyed_service=*/nullptr);
    } else {
      wrapper_service_ = std::make_unique<TabGroupServiceWrapper>(
          /*tab_group_sync_service=*/nullptr,
          SavedTabGroupServiceFactory::GetForProfile(browser_->profile()));
    }
  }

  void TearDown() override {
    wrapper_service_.reset();
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  std::vector<std::unique_ptr<Browser>> browsers_;
  raw_ptr<Browser> browser_;
  std::unique_ptr<TabGroupServiceWrapper> wrapper_service_;
};

// Verify we can add a group to both services correctly.
TEST_P(TabGroupServiceWrapperUnitTest, AddGroup) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  const std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(local_id, retrieved_group->local_group_id());
  EXPECT_EQ(kGroupId, retrieved_group->saved_guid());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->ContainsTab(kFirstTabId));
  EXPECT_TRUE(retrieved_group->ContainsTab(kSecondTabId));
  EXPECT_TRUE(retrieved_group->ContainsTab(kThirdTabId));
}

// Verify we can remove a group from the services using the local id correctly.
TEST_P(TabGroupServiceWrapperUnitTest, RemoveGroupUsingLocalId) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  service()->RemoveGroup(local_id);
  EXPECT_FALSE(service()->GetGroup(local_id).has_value());
  EXPECT_FALSE(service()->GetGroup(kGroupId).has_value());
}

// Verify we can remove a group from the services using the sync id correctly.
TEST_P(TabGroupServiceWrapperUnitTest, RemoveGroupUsingSyncId) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  service()->RemoveGroup(kGroupId);
  EXPECT_FALSE(service()->GetGroup(local_id).has_value());
  EXPECT_FALSE(service()->GetGroup(kGroupId).has_value());
}

// Verify we can update a groups visual data  from the services correctly.
TEST_P(TabGroupServiceWrapperUnitTest, UpdateVisualData) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(kGroupTitle, retrieved_group->title());
  EXPECT_EQ(kGroupColor, retrieved_group->color());

  const std::u16string new_title = u"New Title";
  const TabGroupColorId new_color = TabGroupColorId::kCyan;
  TabGroupVisualData new_visual_data(new_title, new_color);
  service()->UpdateVisualData(local_id, &new_visual_data);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(new_title, retrieved_group->title());
  EXPECT_EQ(new_color, retrieved_group->color());
}

// Verifies that we add tabs to a group at the correct position.
TEST_P(TabGroupServiceWrapperUnitTest, AddTab) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(kGroupTitle, kGroupColor, {FirstTab(kGroupId)}, 0,
                      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->ContainsTab(kFirstTabId));

  SavedTabGroupTab second_tab = SecondTab(kGroupId);
  SavedTabGroupTab third_tab = ThirdTab(kGroupId);

  service()->AddTab(local_id, kSecondTabToken, second_tab.title(),
                    second_tab.url(), 0);
  service()->AddTab(local_id, kThirdTabToken, third_tab.title(),
                    third_tab.url(), 2);

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(3u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->ContainsTab(kFirstTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kSecondTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kThirdTabToken));

  // Get the order of tabs.
  const std::vector<SavedTabGroupTab> tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kSecondTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kFirstTabToken, tabs[1].local_tab_id());
  EXPECT_EQ(kThirdTabToken, tabs[2].local_tab_id());
}

// Verifies that we can update the title and url of a tab in a  saved group.
TEST_P(TabGroupServiceWrapperUnitTest, UpdateTab) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(kGroupTitle, kGroupColor, {FirstTab(kGroupId)}, 0,
                      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->ContainsTab(kFirstTabId));

  const std::u16string new_title = u"This is the new title";
  GURL new_url = GURL("https://not_first_tab.com");

  service()->UpdateTab(local_id, kFirstTabToken, new_title, new_url,
                       /*position=*/std::nullopt);
  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->ContainsTab(kFirstTabId));

  const SavedTabGroupTab* retrieved_tab = retrieved_group->GetTab(kFirstTabId);
  EXPECT_EQ(new_title, retrieved_tab->title());
  EXPECT_EQ(new_url, retrieved_tab->url());
}

// Verifies that we can remove a tab in a group and that after removing all of
// the tabs, the group is deleted.
TEST_P(TabGroupServiceWrapperUnitTest, RemoveTab) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  // Remove the first tab: [ Tab 1, Tab 2, Tab 3 ] -> [ Tab 2, Tab 3]
  service()->RemoveTab(local_id, kFirstTabToken);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(2u, retrieved_group->saved_tabs().size());
  EXPECT_FALSE(retrieved_group->ContainsTab(kFirstTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kSecondTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kThirdTabToken));

  // Remove the third tab: [ Tab 2, Tab 3 ] -> [ Tab 2 ]
  service()->RemoveTab(local_id, kThirdTabToken);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_FALSE(retrieved_group->ContainsTab(kFirstTabToken));
  EXPECT_TRUE(retrieved_group->ContainsTab(kSecondTabToken));
  EXPECT_FALSE(retrieved_group->ContainsTab(kThirdTabToken));

  // Remove the second tab. This should delete the group.
  service()->RemoveTab(local_id, kSecondTabToken);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_FALSE(retrieved_group.has_value());
}

// Verifies that we can move the tabs in a saved group correctly.
TEST_P(TabGroupServiceWrapperUnitTest, MoveTab) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  // Move tab 3 to the front: [ Tab 1, Tab 2, Tab 3 ] -> [ Tab 3, Tab 1, Tab 2]
  service()->MoveTab(local_id, kThirdTabToken, 0);
  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  std::vector<SavedTabGroupTab> tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kThirdTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kFirstTabToken, tabs[1].local_tab_id());
  EXPECT_EQ(kSecondTabToken, tabs[2].local_tab_id());

  // Move tab 2 to the middle: [ Tab 3, Tab 1, Tab 2 ] -> [ Tab 3, Tab 2, Tab 1]
  service()->MoveTab(local_id, kSecondTabToken, 1);
  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kThirdTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kSecondTabToken, tabs[1].local_tab_id());
  EXPECT_EQ(kFirstTabToken, tabs[2].local_tab_id());

  // Move tab 1 to the front: [ Tab 3, Tab 2, Tab 1 ] -> [ Tab 1, Tab 3, Tab 2]
  service()->MoveTab(local_id, kFirstTabToken, 0);
  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kFirstTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kThirdTabToken, tabs[1].local_tab_id());
  EXPECT_EQ(kSecondTabToken, tabs[2].local_tab_id());

  // Move tab 3 to the end: [ Tab 1, Tab 3, Tab 2 ] -> [ Tab 1, Tab 2, Tab 3]
  service()->MoveTab(local_id, kThirdTabToken, 2);
  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  tabs = retrieved_group->saved_tabs();
  EXPECT_EQ(kFirstTabToken, tabs[0].local_tab_id());
  EXPECT_EQ(kSecondTabToken, tabs[1].local_tab_id());
  EXPECT_EQ(kThirdTabToken, tabs[2].local_tab_id());
}

// Verifies that we can update the local tab group mapping of a saved group
// after it is added to the service.
TEST_P(TabGroupServiceWrapperUnitTest, UpdateLocalTabGroupMapping) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  tab_groups::TabGroupId new_local_id = tab_groups::TabGroupId::GenerateNew();
  service()->UpdateLocalTabGroupMapping(kGroupId, new_local_id);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(new_local_id, retrieved_group->local_group_id());
}

// Verifies that we can remove the local tab group mapping of a saved group
// after it is added to the service.
TEST_P(TabGroupServiceWrapperUnitTest, RemoveLocalTabGroupMapping) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  service()->RemoveLocalTabGroupMapping(local_id);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(std::nullopt, retrieved_group->local_group_id());
}

// Verifies that we can update the local tab id mapping for a tab in a saved
// group after it is added to the service.
TEST_P(TabGroupServiceWrapperUnitTest, UpdateLocalTabId) {
  tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();

  SavedTabGroup group(
      kGroupTitle, kGroupColor,
      {FirstTab(kGroupId), SecondTab(kGroupId), ThirdTab(kGroupId)}, 0,
      kGroupId, local_id);
  service()->AddGroup(std::move(group));

  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());

  LocalTabID new_local_tab_id = base::Token::CreateRandom();
  service()->UpdateLocalTabId(local_id, kFirstTabId, new_local_tab_id);

  retrieved_group = service()->GetGroup(kGroupId);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_TRUE(retrieved_group->ContainsTab(new_local_tab_id));
  EXPECT_FALSE(retrieved_group->ContainsTab(kFirstTabToken));
}

// Verifies that when a new tab group is created in the browser it is saved by
// default. When it is closed, the group should still be saved but no longer
// have a local id.
TEST_P(TabGroupServiceWrapperUnitTest, DefaultSaveNewGroups) {
  EXPECT_EQ(0u, service()->GetAllGroups().size());

  // Add some tabs and create a single tab group.
  AddTabToBrowser(GetBrowser(), 0);
  AddTabToBrowser(GetBrowser(), 0);
  TabGroupId local_group_id =
      GetBrowser()->tab_strip_model()->AddToNewGroup({0});

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  EXPECT_TRUE(retrieved_group.has_value());

  base::Uuid saved_id = retrieved_group->saved_guid();

  // Ensure the group is still saved but no longer references `local_group_id`.
  GetBrowser()->tab_strip_model()->CloseAllTabsInGroup(local_group_id);
  EXPECT_FALSE(service()->GetGroup(local_group_id).has_value());
  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(std::nullopt, retrieved_group->local_group_id());
}

INSTANTIATE_TEST_SUITE_P(TabGroupServiceWrapper,
                         TabGroupServiceWrapperUnitTest,
                         testing::Bool());

}  // namespace tab_groups
