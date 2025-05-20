// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_tab_state.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_sync_service_proxy.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/saved_tab_groups/public/utils.h"
#include "components/saved_tab_groups/test_support/saved_tab_group_test_utils.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/tabs/public/tab_group.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"

namespace tab_groups {

class SavedTabGroupKeyedServiceUnitTest : public BrowserWithTestWindowTest {
 public:
  SavedTabGroupKeyedServiceUnitTest() = default;
  SavedTabGroupKeyedServiceUnitTest(const SavedTabGroupKeyedServiceUnitTest&) =
      delete;
  SavedTabGroupKeyedServiceUnitTest& operator=(
      const SavedTabGroupKeyedServiceUnitTest&) = delete;

  Browser* AddBrowser() {
    Browser::CreateParams native_params(profile_.get(), true);
    native_params.initial_show_state = ui::mojom::WindowShowState::kDefault;
    std::unique_ptr<Browser> browser =
        CreateBrowserWithTestWindowForParams(native_params);
    Browser* browser_ptr = browser.get();
    browsers_.emplace_back(std::move(browser));
    return browser_ptr;
  }

  tabs::TabInterface* AddTabToBrowser(Browser* browser, int index) {
    std::unique_ptr<tabs::TabModel> tab = std::make_unique<tabs::TabModel>(
        content::WebContentsTester::CreateTestWebContents(profile_.get(),
                                                          nullptr),
        browser->tab_strip_model());
    tabs::TabInterface* tab_ptr = tab.get();

    browser->tab_strip_model()->AddTab(
        std::move(tab), index, ui::PageTransition::PAGE_TRANSITION_TYPED,
        AddTabTypes::ADD_ACTIVE);

    return tab_ptr;
  }

  tab_groups::TabGroupId LocalIDFromSyncID(const base::Uuid& sync_id) {
    return service()->proxy()->GetGroup(sync_id)->local_group_id().value();
  }

  tab_groups::TabGroupId CreateNewGroupInBrowser(Browser* browser) {
    AddTabToBrowser(browser, 0);
    tab_groups::TabGroupId local_id =
        browser->tab_strip_model()->AddToNewGroup({0});
    return local_id;
  }

  // Returns the sync id of the group that was added.
  base::Uuid EnforceGroupSaved(tab_groups::SavedTabGroup group) {
    const LocalTabGroupID local_id = group.local_group_id().value();
    return service()->proxy()->GetGroup(local_id).value().saved_guid();
  }

  base::Uuid AddGroupFromSync() {
    SavedTabGroup group = test::CreateTestSavedTabGroup();
    service()->proxy()->AddGroup(group);
    return group.saved_guid();
  }

  base::Uuid AddGroupFromLocal(Browser* browser) {
    return EnforceGroupSaved(SavedTabGroupUtils::CreateSavedTabGroupFromLocalId(
        CreateNewGroupInBrowser(browser)));
  }
  TestingProfile* profile() { return profile_.get(); }
  SavedTabGroupKeyedService* service() { return service_; }

 protected:
  void SetUp() override {
    if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
      // SavedTabGroupKeyedService is unused when the migration flag is enabled.
      // These tests should no longer run. Once the migration is completed this
      // file will be deleted. See crbug.com/350514491 for the migration status.
      GTEST_SKIP();
    }

    profile_ = std::make_unique<TestingProfile>();
    // We use the SavedTabGroupKeyedService from the profile rather than
    // constructing a fresh one for test because
    // SavedTabGroupWebContentsListener holds on to the real one supplied by the
    // tab feature.
    service_ = SavedTabGroupServiceFactory::GetForProfile(profile_.get());
  }
  void TearDown() override {
    if (tab_groups::IsTabGroupSyncServiceDesktopMigrationEnabled()) {
      // SavedTabGroupKeyedService is unused when the migration flag is enabled.
      // These tests should no longer run. Once the migration is completed this
      // file will be deleted. See crbug.com/350514491 for the migration status.
      GTEST_SKIP();
    }

    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

 private:
  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<SavedTabGroupKeyedService> service_ = nullptr;

  std::vector<std::unique_ptr<Browser>> browsers_;
};

TEST_F(SavedTabGroupKeyedServiceUnitTest, GetBrowserWithTabGroupId) {
  Browser* browser_1 = AddBrowser();

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  EXPECT_EQ(browser_1, SavedTabGroupUtils::GetBrowserWithTabGroupId(group_id));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       UngroupingStopsListeningToWebContents) {
  Browser* browser_1 = AddBrowser();

  // Create a new tab and add it to a group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  tabs::TabInterface* tab = AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());

  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0, 1});

  auto& group_listener_map =
      service()->listener()->GetLocalTabGroupListenerMapForTesting();

  // Now the group should be listened to.
  EXPECT_EQ(1u, group_listener_map.count(group_id));

  // Remove `web_contents_ptr`.
  tab->GetContents()->Close();
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());

  // Expect that the group is still listened to since there's still
  // 1 tab in the group.
  EXPECT_EQ(1u, group_listener_map.count(group_id));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, AddedTabIsListenedTo) {
  Browser* browser_1 = AddBrowser();

  auto sync_id = AddGroupFromLocal(browser_1);

  // The listener for the tab should be correctly set up.
  SavedTabGroupWebContentsListener* listener1 =
      browser_1->tab_strip_model()
          ->GetTabAtIndex(0)
          ->GetTabFeatures()
          ->saved_tab_group_web_contents_listener();
  EXPECT_TRUE(listener1->saved_group());

  // Add a second tab and expect that it is observed too.
  AddTabToBrowser(browser_1, 1);
  browser_1->tab_strip_model()->AddToExistingGroup({1},
                                                   LocalIDFromSyncID(sync_id));

  // The listener for the tab should be correctly set up.
  SavedTabGroupWebContentsListener* listener2 =
      browser_1->tab_strip_model()
          ->GetTabAtIndex(1)
          ->GetTabFeatures()
          ->saved_tab_group_web_contents_listener();
  EXPECT_TRUE(listener2->saved_group());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, PauseResumeTracking) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with two tabs, one in a saved group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({1});

  base::Uuid saved_group_id = service()->model()->Get(group_id)->saved_guid();

  // We should be listening to one group and one tab in that group.
  auto& group_listener_map =
      service()->listener()->GetLocalTabGroupListenerMapForTesting();
  ASSERT_EQ(1u, group_listener_map.count(group_id));

  // Pause tracking.
  service()->PauseTrackingLocalTabGroup(group_id);

  // Remove the tab in the group.
  tab_groups::TabGroupVisualData visual_data = *(browser_1->tab_strip_model()
                                                     ->group_model()
                                                     ->GetTabGroup(group_id)
                                                     ->visual_data());
  std::unique_ptr<tabs::TabModel> detached_tab =
      browser_1->tab_strip_model()->DetachTabAtForInsertion(1);
  // This kills the group.
  ASSERT_FALSE(
      browser_1->tab_strip_model()->group_model()->ContainsTabGroup(group_id));

  // Recreate the local group and add the tab to it (same browser is fine).
  browser_1->tab_strip_model()->AddTabGroup(group_id, visual_data);
  browser_1->tab_strip_model()->InsertDetachedTabAt(
      1, std::move(detached_tab), AddTabTypes::ADD_NONE, group_id);

  // Resume tracking.
  service()->ResumeTrackingLocalTabGroup(saved_group_id, group_id);

  // Validate that tracking still works.
  // Check that the local and saved ids are still linked in the model.
  EXPECT_EQ(saved_group_id, service()->model()->Get(group_id)->saved_guid());
  // Check that there is still one tab in the model's saved group.
  EXPECT_EQ(1u, service()->model()->Get(group_id)->saved_tabs().size());
  // The listener state should be the same as well.
  EXPECT_EQ(1u, group_listener_map.count(group_id));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, ResumeTrackingValidatesConsistency) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with two tabs.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0, 1});
  base::Uuid saved_group_id = service()->model()->Get(group_id)->saved_guid();

  // Reordering during paused tracking is okay.
  service()->PauseTrackingLocalTabGroup(group_id);
  browser_1->tab_strip_model()->MoveWebContentsAt(0, 1, false);
  service()->ResumeTrackingLocalTabGroup(saved_group_id, group_id);

  // Removing a tab from the group during paused tracking is not okay.
  service()->PauseTrackingLocalTabGroup(group_id);
  browser_1->tab_strip_model()->RemoveFromGroup({1});
  EXPECT_DEATH(service()->ResumeTrackingLocalTabGroup(saved_group_id, group_id),
               "");
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, AlreadyOpenedGroupIsFocused) {
  Browser* browser_1 = AddBrowser();
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());

  // Add 2 tabs to the browser.
  AddTabToBrowser(browser_1, 0);
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());

  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();
  browser_1->tab_strip_model()->AddToGroupForRestore({0}, local_id);
  const base::Uuid sync_id = base::Uuid::GenerateRandomV4();

  // Store the guid to local_id association in the keyed service. We should
  // expect at the end of the test, `local_id_3` has no association with the
  // SavedTabGroupModel at all.
  service()->ConnectRestoredGroupToSaveId(sync_id, local_id);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading in persisted data on startup.
  std::vector<SavedTabGroupTab> group_1_tabs = {SavedTabGroupTab(
      GURL(chrome::kChromeUINewTabURL), u"New Tab", sync_id, /*position=*/0)};

  SavedTabGroup saved_group_1(u"Group 1", tab_groups::TabGroupColorId::kGrey,
                              std::move(group_1_tabs), std::nullopt, sync_id);

  service()->model()->AddedLocally(saved_group_1);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});

  // Activate the second tab.
  browser_1->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser_1->tab_strip_model()->active_index());

  std::optional<tab_groups::TabGroupId> opened_group_id =
      service()->OpenSavedTabGroupInBrowser(
          browser_1, sync_id, tab_groups::OpeningSource::kUnknown);
  EXPECT_TRUE(opened_group_id.has_value());
  EXPECT_EQ(local_id, opened_group_id.value());

  // Ensure the first tab in the saved group is activated.
  EXPECT_EQ(0, browser_1->tab_strip_model()->active_index());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       ActiveTabInAlreadyOpenedGroupIsFocused) {
  Browser* browser_1 = AddBrowser();
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());

  // Add 2 tabs to the browser_1.
  AddTabToBrowser(browser_1, 0);
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());

  const tab_groups::TabGroupId tab_group_id =
      tab_groups::TabGroupId::GenerateNew();
  browser_1->tab_strip_model()->AddToGroupForRestore({0}, tab_group_id);

  const base::Uuid guid_1 = base::Uuid::GenerateRandomV4();

  // Store the guid to tab_group_id association in the keyed service. We should
  // expect at the end of the test, `tab_group_id_3` has no association with the
  // SavedTabGroupModel at all.
  service()->ConnectRestoredGroupToSaveId(guid_1, tab_group_id);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading in persisted data on startup.
  std::vector<SavedTabGroupTab> group_1_tabs = {
      SavedTabGroupTab(GURL("https://www.google.com"), u"Google", guid_1,
                       /*position=*/0),
      SavedTabGroupTab(GURL("https://www.youtube.com"), u"Youtube", guid_1,
                       /*position=*/1)};
  SavedTabGroup saved_group_1(u"Group 1", tab_groups::TabGroupColorId::kGrey,
                              std::move(group_1_tabs), std::nullopt, guid_1);

  service()->model()->AddedLocally(saved_group_1);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});

  // Activate the second tab.
  browser_1->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(1, browser_1->tab_strip_model()->active_index());

  std::optional<tab_groups::TabGroupId> opened_group_id =
      service()->OpenSavedTabGroupInBrowser(
          browser_1, guid_1, tab_groups::OpeningSource::kUnknown);
  EXPECT_TRUE(opened_group_id.has_value());
  EXPECT_EQ(tab_group_id, opened_group_id.value());

  // Ensure the active tab in the saved group is not changed.
  EXPECT_EQ(1, browser_1->tab_strip_model()->active_index());

  // Activate the third tab.
  browser_1->tab_strip_model()->ActivateTabAt(2);
  EXPECT_EQ(2, browser_1->tab_strip_model()->active_index());

  opened_group_id = service()->OpenSavedTabGroupInBrowser(
      browser_1, guid_1, tab_groups::OpeningSource::kUnknown);
  EXPECT_TRUE(opened_group_id.has_value());
  EXPECT_EQ(tab_group_id, opened_group_id.value());

  // If there is no active tab in the saved tab group, the first tab of the
  // saved tab group is activated. Ensure the first tab in the saved group is
  // activated.
  EXPECT_EQ(0, browser_1->tab_strip_model()->active_index());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       KeyedServiceUpdatesRestoredGroupWithOneLessTabToMatchSavedGroup) {
  Browser* browser = AddBrowser();
  ASSERT_EQ(0, browser->tab_strip_model()->count());

  // Add 1 tab to the browser.
  AddTabToBrowser(browser, 0);
  ASSERT_EQ(1, browser->tab_strip_model()->count());

  const tab_groups::TabGroupId local_id = tab_groups::TabGroupId::GenerateNew();
  browser->tab_strip_model()->AddToGroupForRestore({0}, local_id);
  const base::Uuid sync_guid = base::Uuid::GenerateRandomV4();

  // Store the guid to tab_group_id association in the keyed service.
  service()->ConnectRestoredGroupToSaveId(sync_guid, local_id);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading persisted data on startup.
  std::vector<SavedTabGroupTab> group_tabs = {
      SavedTabGroupTab(GURL("https://www.google.com"), u"Google", sync_guid,
                       /*position=*/0),
      SavedTabGroupTab(GURL("https://www.youtube.com"), u"Youtube", sync_guid,
                       /*position=*/1)};

  SavedTabGroup saved_group(u"Group", tab_groups::TabGroupColorId::kGrey,
                            std::move(group_tabs), std::nullopt, sync_guid);
  service()->model()->AddedLocally(saved_group);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});

  // Retrieve the saved group from the SavedTabGroupModel.
  SavedTabGroupModel* model = service()->model();
  const SavedTabGroup* retrieved_saved_group = model->Get(sync_guid);

  // Retrieve the tab group from the TabStripModel.
  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  const TabGroup* tab_group =
      tab_strip_model->group_model()->GetTabGroup(local_id);
  ASSERT_TRUE(tab_group);

  // Verify the number of tabs in the TabGroup and SavedTabGroup are the same.
  const gfx::Range& tab_range = tab_group->ListTabs();
  ASSERT_EQ(tab_range.length(), retrieved_saved_group->saved_tabs().size());

  // Remove the first tab from the saved group.
  service()->model()->RemoveTabFromGroupFromSync(
      sync_guid, retrieved_saved_group->saved_tabs().at(0).saved_tab_guid());

  // Verify the number of tabs in the TabGroup and SavedTabGroup are the same.
  const gfx::Range& modified_tab_range = tab_group->ListTabs();
  ASSERT_EQ(modified_tab_range.length(),
            retrieved_saved_group->saved_tabs().size());

  // TODO(crbug.com/40915240): Compare tabs and ensure they are in the same
  // order and contain the same data.
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       KeyedServiceUpdatesRestoredGroupWithExtraTabToMatchSavedGroup) {
  Browser* browser = AddBrowser();
  ASSERT_EQ(0, browser->tab_strip_model()->count());

  // Add 2 tabs to the browser.
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 1);
  ASSERT_EQ(2, browser->tab_strip_model()->count());

  const tab_groups::TabGroupId tab_group_id =
      tab_groups::TabGroupId::GenerateNew();
  browser->tab_strip_model()->AddToGroupForRestore({0, 1}, tab_group_id);
  const base::Uuid guid = base::Uuid::GenerateRandomV4();

  // Store the guid to tab_group_id association in the keyed service.
  service()->ConnectRestoredGroupToSaveId(guid, tab_group_id);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading persisted data on startup.
  std::vector<SavedTabGroupTab> group_tabs = {
      SavedTabGroupTab(GURL("https://www.google.com"), u"Google", guid,
                       /*position=*/0)};

  // Populate the savedTabGroupModel with some test data, To test the added
  // savedTabGroupModel.
  auto added_group_tab =
      SavedTabGroupTab(GURL("https://www.youtube.com"), u"Youtube", guid,
                       /*position=*/0);

  SavedTabGroup saved_group(u"Group", tab_groups::TabGroupColorId::kGrey,
                            std::move(group_tabs), std::nullopt, guid);
  service()->model()->AddedLocally(saved_group);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries(/*groups=*/{}, /*tabs=*/{});

  // Retrieve the saved group from the SavedTabGroupModel.
  SavedTabGroupModel* model = service()->model();
  const SavedTabGroup* retrieved_saved_group = model->Get(guid);

  // Retrieve the tab group from the TabStripModel.
  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  const TabGroup* tab_group =
      tab_strip_model->group_model()->GetTabGroup(tab_group_id);
  ASSERT_TRUE(tab_group);

  // Verify the number of tabs in the TabGroup and SavedTabGroup are the same.
  const gfx::Range& tab_range = tab_group->ListTabs();
  ASSERT_EQ(tab_range.length(), retrieved_saved_group->saved_tabs().size());

  // Add the group tab in the saved group.
  service()->model()->AddTabToGroupFromSync(guid, added_group_tab);

  // Verify the number of tabs in the TabGroup and SavedTabGroup are the same.
  const gfx::Range& modified_tab_range = tab_group->ListTabs();
  ASSERT_EQ(modified_tab_range.length(),
            retrieved_saved_group->saved_tabs().size());

  // TODO(crbug.com/40915240): Compare tabs and ensure they are in the same
  // order and contain the same data.
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, NewTabFromSyncOpensInLocalGroup) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with one tab.
  ASSERT_EQ(0, tabstrip->count());

  auto sync_id = AddGroupFromLocal(browser);
  auto group_id = LocalIDFromSyncID(sync_id);

  // Add a tab to the saved group.
  const SavedTabGroupTab added_tab(GURL(chrome::kChromeUINewTabURL), u"New Tab",
                                   sync_id, /*position=*/0);
  service()->model()->AddTabToGroupFromSync(sync_id, added_tab);

  // Tab should have opened in local group too.
  EXPECT_EQ(2, tabstrip->count());
  EXPECT_EQ(
      2u, tabstrip->group_model()->GetTabGroup(group_id)->ListTabs().length());
}

// Verifies that changes from sync will navigate the corresponding tab unless it
// was a fragment change happening on the same domain (Ex:
// https://www.example.com and https:://www.example.com#this_is_a_fragment).
TEST_F(SavedTabGroupKeyedServiceUnitTest,
       NavigateTabFromSyncNavigatesLocalTab) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  auto sync_id = AddGroupFromLocal(browser);

  const SavedTabGroup* const saved_group = service()->model()->Get(sync_id);
  const base::Uuid saved_tab_id =
      saved_group->saved_tabs().at(0).saved_tab_guid();

  // Navigate the saved tab.
  const GURL url = GURL("https://www.example.com");
  SavedTabGroupTab navigated_tab = *saved_group->GetTab(saved_tab_id);
  navigated_tab.SetURL(url);
  navigated_tab.SetTitle(u"Example Page");
  auto* tester = content::WebContentsTester::For(tabstrip->GetWebContentsAt(0));
  service()->model()->MergeRemoteTab(navigated_tab);
  tester->CommitPendingNavigation();
  // The local tab should have navigated too.
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url);

  // URL fragments from sync should not navigate the tab. This is because this
  // can cause destructive behavior across devices which includes things like
  // data loss (forms being reset), and users losing their place while reading
  // or editing a document.
  const GURL url_2 = GURL("https://www.example.com#section_1");
  navigated_tab = *saved_group->GetTab(saved_tab_id);
  navigated_tab.SetURL(url_2);
  navigated_tab.SetTitle(u"Example Page - Section 1");
  service()->model()->MergeRemoteTab(navigated_tab);

  // The local tab should not have changed.
  EXPECT_NE(tabstrip->GetWebContentsAt(0)->GetURL(), url_2);
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, SimulateLocalThenSyncTabNavigations) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  AddGroupFromLocal(browser);

  const GURL url_1 = GURL("https://www.example.com");
  const GURL url_2 = GURL("https://www.example2.com");

  // Manually navigate the webcontents of the saved tab locally.
  tabstrip->GetWebContentsAt(0)->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url_1));
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url_1);

  // Simulate a sync navigation on the same tab.
  tabstrip->GetTabAtIndex(0)
      ->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url_2);
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url_2);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, SimulateSyncThenLocalTabNavigations) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  AddGroupFromLocal(browser);

  const GURL url_1 = GURL("https://www.example.com");
  const GURL url_2 = GURL("https://www.example2.com");

  // Simulate a sync navigation on the same tab.
  tabstrip->GetTabAtIndex(0)
      ->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url_2);
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url_2);

  // Manually navigate the webcontents of the saved tab locally.
  tabstrip->GetWebContentsAt(0)->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url_1));
  EXPECT_EQ(tabstrip->GetWebContentsAt(0)->GetURL(), url_1);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, RemoveTabFromSyncRemovesLocalTab) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with two tabs.
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 1);
  const tab_groups::TabGroupId group_id = tabstrip->AddToNewGroup({0, 1});

  const SavedTabGroup* const saved_group = service()->model()->Get(group_id);

  // Remove one tab from the saved group.
  service()->model()->RemoveTabFromGroupFromSync(
      saved_group->saved_guid(),
      saved_group->saved_tabs().at(0).saved_tab_guid());

  // It should have been removed from the local group too.
  EXPECT_EQ(1, tabstrip->count());
  EXPECT_EQ(
      1u, tabstrip->group_model()->GetTabGroup(group_id)->ListTabs().length());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       RemoveGroupFromSyncRemovesLocalTabAndLocalGroup) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  auto sync_id = AddGroupFromLocal(browser);
  auto group_id = LocalIDFromSyncID(sync_id);

  // Add an extra tab so closing the grouped tab doesn't close the browser.
  AddTabToBrowser(browser, 1);

  // Remove the saved group.
  service()->model()->RemovedFromSync(sync_id);

  // The local group should have been closed.
  EXPECT_FALSE(tabstrip->group_model()->ContainsTabGroup(group_id));

  // The tab was deleted.
  EXPECT_EQ(1, tabstrip->count());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       ReorderTabLocallyUpdatesSavedTabGroupTabOrder) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with two tabs.
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 1);
  const tab_groups::TabGroupId group_id = tabstrip->AddToNewGroup({0, 1});

  const SavedTabGroup* group = service()->model()->Get(group_id);
  LocalTabID first_tab_id = tabstrip->GetTabAtIndex(0)->GetHandle().raw_value();
  LocalTabID second_tab_id =
      tabstrip->GetTabAtIndex(1)->GetHandle().raw_value();

  ASSERT_EQ(2u, group->saved_tabs().size());
  EXPECT_EQ(first_tab_id, group->saved_tabs()[0].local_tab_id().value());
  EXPECT_EQ(second_tab_id, group->saved_tabs()[1].local_tab_id().value());

  // Expect after moving the first tab to the right of the second, that the
  // group updated the positions of the tabs accordingly.
  browser->tab_strip_model()->MoveWebContentsAt(0, 1, false);

  EXPECT_EQ(second_tab_id, group->saved_tabs()[0].local_tab_id().value());
  EXPECT_EQ(first_tab_id, group->saved_tabs()[1].local_tab_id().value());

  // Expect moving an entire group to the right, still keeps the saved tabs in
  // the correct order.
  AddTabToBrowser(browser, 2);
  browser->tab_strip_model()->MoveGroupTo(group_id, 1);

  EXPECT_EQ(second_tab_id, group->saved_tabs()[0].local_tab_id().value());
  EXPECT_EQ(first_tab_id, group->saved_tabs()[1].local_tab_id().value());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, ReorderTabFromSyncReordersLocalTab) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with two tabs.
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 1);
  const tab_groups::TabGroupId group_id = tabstrip->AddToNewGroup({0, 1});

  const base::Uuid saved_group_id =
      service()->model()->Get(group_id)->saved_guid();

  // TODO Fill out this test once swap tabs from sync is implemented
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       DiscardingATabDoesntCrashWhenReordering) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with two tabs.
  tabs::TabInterface* tab_0 = AddTabToBrowser(browser, 0);
  tabs::TabInterface* tab_1 = AddTabToBrowser(browser, 1);
  const tab_groups::TabGroupId group_id = tabstrip->AddToNewGroup({0, 1});

  const SavedTabGroup* group = service()->model()->Get(group_id);

  LocalTabID tab_0_local_id = tab_0->GetHandle().raw_value();
  LocalTabID tab_1_local_id = tab_1->GetHandle().raw_value();

  std::unique_ptr<content::WebContents> replacement_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);
  browser->tab_strip_model()->DiscardWebContentsAt(
      0, std::move(replacement_web_contents));

  // Expect after moving the first tab to the right of the second, that the
  // group updated the positions of the tabs accordingly.
  browser->tab_strip_model()->MoveWebContentsAt(0, 1, false);

  EXPECT_EQ(tab_1_local_id, group->saved_tabs()[0].local_tab_id().value());
  EXPECT_EQ(tab_0_local_id, group->saved_tabs()[1].local_tab_id().value());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       NewBadTabFromSyncOpensInLocalGroupOnNewTabPage) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  auto sync_id = AddGroupFromLocal(browser);

  // Add a tab to the saved group.
  const SavedTabGroupTab added_tab(GURL("chrome://settings"), u"New Tab",
                                   sync_id, /*position=*/0);
  service()->model()->AddTabToGroupFromSync(sync_id, added_tab);

  // Tab should have opened in local group too.
  EXPECT_EQ(2, tabstrip->count());

  const gfx::Range grouped_tabs = tabstrip->group_model()
                                      ->GetTabGroup(LocalIDFromSyncID(sync_id))
                                      ->ListTabs();
  EXPECT_EQ(2u, grouped_tabs.length());
  for (auto index = grouped_tabs.start(); index < grouped_tabs.end(); ++index) {
    EXPECT_EQ(tabstrip->GetWebContentsAt(index)->GetURL(),
              GURL(chrome::kChromeUINewTabURL));
  }
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       NavigateTabFromSyncWithBadURLDoesntNavigateLocalTab) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  auto sync_id = AddGroupFromLocal(browser);

  const SavedTabGroup* const saved_group = service()->model()->Get(sync_id);
  auto saved_tab_id = saved_group->saved_tabs().at(0).saved_tab_guid();

  // Navigate the saved tab.
  const GURL url = GURL("chrome://settings");
  SavedTabGroupTab navigated_tab = *saved_group->GetTab(saved_tab_id);
  navigated_tab.SetURL(url);
  navigated_tab.SetTitle(u"Example Page");
  service()->model()->MergeRemoteTab(navigated_tab);

  // The local tab should not navigate to the new URL.
  EXPECT_NE(tabstrip->GetWebContentsAt(0)->GetURL(), url);
}

// Tests that unsupported tab URL is saved.
TEST_F(SavedTabGroupKeyedServiceUnitTest, UnsupportedTabURLSaved) {
  Browser* browser_1 = AddBrowser();
  GURL url("file://1");

  // Create a saved tab group with one tab.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  tabs::TabInterface* added_tab = AddTabToBrowser(browser_1, 0);
  added_tab->GetContents()->GetController().LoadURLWithParams(
      content::NavigationController::LoadURLParams(url));
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  const SavedTabGroup* const saved_group = service()->model()->Get(group_id);

  // The URL should be saved into the tab group.
  EXPECT_EQ(saved_group->saved_tabs().at(0).url(), url);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       TabLocalNavigationToBadURLUpdateModel) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with one good tab.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  tabs::TabInterface* added_tab = AddTabToBrowser(browser_1, 0);
  GURL good_gurl = GURL("http://www.google.com");
  GURL bad_gurl = GURL("file://1");

  auto* tester = content::WebContentsTester::For(added_tab->GetContents());
  tester->NavigateAndCommit(good_gurl);

  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  const SavedTabGroup* const saved_group = service()->model()->Get(group_id);

  // Navigate to a bad tab.
  tester->NavigateAndCommit(bad_gurl);

  // The SavedTabGroupTab should be changed to the bad one.
  EXPECT_EQ(saved_group->saved_tabs().at(0).url(), bad_gurl);
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       RedirectAfterDeleteRequestDoesntUpdateModel) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with one good tab.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  tabs::TabInterface* added_tab = AddTabToBrowser(browser_1, 0);
  GURL good_url = GURL("http://www.foo.com");
  GURL delete_url = GURL("http://www.delete.com");
  GURL redirect_url = GURL("http://www.redirect.com");

  auto* tester = content::WebContentsTester::For(added_tab->GetContents());
  tester->NavigateAndCommit(good_url);
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});

  const SavedTabGroup* const saved_group = service()->model()->Get(group_id);

  content::RenderFrameHost* render_frame_host =
      added_tab->GetContents()->GetPrimaryMainFrame();
  std::unique_ptr<content::NavigationSimulator> navigation =
      content::NavigationSimulator::CreateRendererInitiated(delete_url,
                                                            render_frame_host);
  navigation->SetMethod("DELETE");
  navigation->Start();
  navigation->Redirect(redirect_url);
  navigation->Commit();

  // The SavedTabGroupTab should still be at the good URL not the bad one.
  EXPECT_EQ(saved_group->saved_tabs().at(0).url(), good_url);
}

// Save group in front of others when `is_pinned` is true.
TEST_F(SavedTabGroupKeyedServiceUnitTest, SaveGroupIsPinned) {
  Browser* browser = AddBrowser();

  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  AddTabToBrowser(browser, 0);
  ASSERT_EQ(3, browser->tab_strip_model()->count());
  const tab_groups::TabGroupId tab_group_id_1 =
      browser->tab_strip_model()->AddToNewGroup({0});
  const tab_groups::TabGroupId tab_group_id_2 =
      browser->tab_strip_model()->AddToNewGroup({1});
  const tab_groups::TabGroupId tab_group_id_3 =
      browser->tab_strip_model()->AddToNewGroup({2});

  auto saved_tab_groups = service()->model()->saved_tab_groups();

  // Pinning reverses the saving order.
  ASSERT_EQ(tab_group_id_3, saved_tab_groups[0].local_group_id());
  ASSERT_EQ(tab_group_id_2, saved_tab_groups[1].local_group_id());
  ASSERT_EQ(tab_group_id_1, saved_tab_groups[2].local_group_id());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       LastTabRemoveFromSyncDoesNotCloseGroupAndCreatesPendingNTP) {
  Browser* browser = AddBrowser();

  tab_groups::SavedTabGroupKeyedService* service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  TabStripModel* const tab_strip_model = browser->tab_strip_model();

  // Create a saved tab group with one tab. Groups are default saved.
  AddTab(browser, GURL("https://www.test.com"));
  const tab_groups::TabGroupId group_id = tab_strip_model->AddToNewGroup({0});
  // service->SaveGroup(group_id);
  const SavedTabGroup* const saved_group = service->model()->Get(group_id);

  // Add an extra tab so closing the grouped tab doesn't close the browser.
  AddTab(browser, GURL("https://www.test.com"));

  // Remove the only tab from the saved group.
  service->model()->RemoveTabFromGroupFromSync(
      saved_group->saved_guid(),
      saved_group->saved_tabs().at(0).saved_tab_guid());

  // The group should have closed along with all of its tabs.
  EXPECT_EQ(2, tab_strip_model->count());
  // The local group should also have been closed, since it's now empty.
  EXPECT_TRUE(tab_strip_model->group_model()->ContainsTabGroup(group_id));
  EXPECT_EQ(1u, saved_group->saved_tabs().size());
  EXPECT_TRUE(saved_group->saved_tabs()[0].is_pending_ntp());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, GroupRemovedFromSyncClosesOpenGroup) {
  Browser* browser = AddBrowser();

  tab_groups::SavedTabGroupKeyedService* service =
      tab_groups::SavedTabGroupServiceFactory::GetForProfile(
          browser->profile());
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a tab group with one tab.
  AddTab(browser, GURL("https://www.test.com"));

  const tab_groups::TabGroupId group_id = tabstrip->AddToNewGroup({0});
  const base::Uuid saved_group_id =
      service->model()->Get(group_id)->saved_guid();

  // Add an extra tab so closing the grouped tab doesn't close the browser.
  AddTab(browser, GURL("https://www.test.com"));

  // Remove the saved group.
  service->model()->RemovedFromSync(saved_group_id);

  // The local group should have been closed.
  EXPECT_FALSE(tabstrip->group_model()->ContainsTabGroup(group_id));
  // The group should have closed with its tabs in the tabstrip.
  EXPECT_EQ(1, tabstrip->count());
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, CreateTabStateOnSyncNavigations) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with one tab.
  AddTabToBrowser(browser, 0);
  tabstrip->AddToNewGroup({0});

  const GURL url = GURL("https://www.example.com");
  const GURL url2 = GURL("https://www.example2.com");

  // Manually navigate the webcontents of the saved tab locally.
  tabs::TabInterface* tab = tabstrip->GetTabAtIndex(0);
  content::WebContents* contents = tab->GetContents();
  auto* tester = content::WebContentsTester::For(contents);
  tester->NavigateAndCommit(url);
  EXPECT_EQ(contents->GetURL(), url);
  EXPECT_FALSE(TabGroupSyncTabState::FromWebContents(contents));

  // Load a URL through sync.
  tab->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url2);
  tester->CommitPendingNavigation();
  EXPECT_EQ(contents->GetURL(), url2);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(contents));

  // Manually load a URL again.
  tester->NavigateAndCommit(url);
  EXPECT_EQ(contents->GetURL(), url);
  EXPECT_FALSE(TabGroupSyncTabState::FromWebContents(contents));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, TabStateClearedOnUserInput) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with one tab.
  AddTabToBrowser(browser, 0);
  tabstrip->AddToNewGroup({0});

  const GURL url = GURL("https://www.example.com");

  // Simulate a sync navigation on the tab.
  tabs::TabInterface* tab = tabstrip->GetTabAtIndex(0);
  content::WebContents* web_contents = tab->GetContents();
  tab->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url);
  auto* tester = content::WebContentsTester::For(web_contents);
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));

  tester->TestDidReceiveMouseDownEvent();
  EXPECT_FALSE(TabGroupSyncTabState::FromWebContents(web_contents));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       TabStateNotClearedOnForwardBackwardNavigations) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  // Create a saved tab group with one tab.
  AddTabToBrowser(browser, 0);
  tabstrip->AddToNewGroup({0});

  const GURL url = GURL("https://www.example.com");
  const GURL url2 = GURL("https://www.example2.com");

  // Manually navigate the webcontents of the saved tab locally.
  tabs::TabInterface* tab = tabstrip->GetTabAtIndex(0);
  content::WebContents* web_contents = tab->GetContents();

  auto* tester = content::WebContentsTester::For(web_contents);
  tester->NavigateAndCommit(url);
  EXPECT_EQ(web_contents->GetURL(), url);
  EXPECT_FALSE(TabGroupSyncTabState::FromWebContents(web_contents));

  // Simulate a sync navigation on the tab.
  tab->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url2);
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url2);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));

  // Go back to the previous page, tab state shouldn't be reset.
  web_contents->GetController().GoBack();
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));

  // Go forward to the previous page, tab state shouldn't be reset.
  web_contents->GetController().GoForward();
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url2);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, TabStateNotClearedOnReload) {
  Browser* const browser = AddBrowser();
  TabStripModel* const tabstrip = browser->tab_strip_model();

  AddGroupFromLocal(browser);
  const GURL url = GURL("https://www.example.com");

  tabs::TabInterface* tab = tabstrip->GetTabAtIndex(0);
  content::WebContents* web_contents = tab->GetContents();
  auto* tester = content::WebContentsTester::For(web_contents);

  // Simulate a sync navigation on the tab.
  tab->GetTabFeatures()
      ->saved_tab_group_web_contents_listener()
      ->NavigateToUrlForTest(url);
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));

  // Reload the page.
  web_contents->GetController().Reload(content::ReloadType::NORMAL,
                                       /*check_for_repost=*/true);
  tester->CommitPendingNavigation();
  EXPECT_EQ(web_contents->GetURL(), url);
  EXPECT_TRUE(TabGroupSyncTabState::FromWebContents(web_contents));
}

}  // namespace tab_groups
