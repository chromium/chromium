// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"

#include <memory>

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model_listener.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/ui_base_types.h"

class SavedTabGroupKeyedServiceUnitTest : public BrowserWithTestWindowTest {
 public:
  SavedTabGroupKeyedServiceUnitTest() = default;
  SavedTabGroupKeyedServiceUnitTest(const SavedTabGroupKeyedServiceUnitTest&) =
      delete;
  SavedTabGroupKeyedServiceUnitTest& operator=(
      const SavedTabGroupKeyedServiceUnitTest&) = delete;

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

  SavedTabGroupKeyedService* service() { return service_.get(); }

 private:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    service_ = std::make_unique<SavedTabGroupKeyedService>(profile_.get());
  }
  void TearDown() override {
    for (auto& browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }
  }

  content::RenderViewHostTestEnabler rvh_test_enabler_;

  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<SavedTabGroupKeyedService> service_;

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
  content::WebContents* web_contents_ptr = AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0, 1});

  auto& group_listener_map =
      service()->listener()->GetLocalTabGroupListenerMapForTesting();

  // Expect that the group isn't being listened to yet.
  EXPECT_EQ(0u, group_listener_map.count(group_id));

  // Save the group.
  service()->SaveGroup(group_id);

  // Now the group should be listened to.
  EXPECT_EQ(1u, group_listener_map.count(group_id));

  // Expect that the listener map is listening to two tabs, including
  // `web_contents_ptr`.
  auto& tab_token_mapping =
      group_listener_map.at(group_id).GetWebContentsTokenMapForTesting();
  EXPECT_EQ(2u, tab_token_mapping.size());
  EXPECT_EQ(1u, tab_token_mapping.count(web_contents_ptr));

  // Remove `web_contents_ptr`.
  web_contents_ptr->Close();
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());

  // Expect that the group is still listened to since there's still
  // 1 tab in the group.
  EXPECT_EQ(1u, group_listener_map.count(group_id));

  // Expect that `web_contents_ptr` is not being listened to.
  EXPECT_EQ(0u, tab_token_mapping.count(web_contents_ptr));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, AddedTabIsListenedTo) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with one tab.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  ASSERT_EQ(1, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({0});
  service()->SaveGroup(group_id);

  // One tab should be observed in this group.
  auto& tab_token_mapping = service()
                                ->listener()
                                ->GetLocalTabGroupListenerMapForTesting()
                                .at(group_id)
                                .GetWebContentsTokenMapForTesting();
  ASSERT_EQ(1u, tab_token_mapping.size());

  // Add a second tab and expect that it is observed too.
  content::WebContents* added_tab = AddTabToBrowser(browser_1, 1);
  browser_1->tab_strip_model()->AddToExistingGroup({1}, group_id);
  EXPECT_EQ(2u, tab_token_mapping.size());
  EXPECT_TRUE(tab_token_mapping.contains(added_tab));
}

TEST_F(SavedTabGroupKeyedServiceUnitTest, PauseResumeTracking) {
  Browser* browser_1 = AddBrowser();

  // Create a saved tab group with two tabs, one in a saved group.
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());
  AddTabToBrowser(browser_1, 0);
  content::WebContents* grouped_tab_ptr = AddTabToBrowser(browser_1, 1);
  ASSERT_EQ(2, browser_1->tab_strip_model()->count());
  tab_groups::TabGroupId group_id =
      browser_1->tab_strip_model()->AddToNewGroup({1});
  service()->SaveGroup(group_id);
  base::Uuid saved_group_id = service()->model()->Get(group_id)->saved_guid();

  // We should be listening to one group and one tab in that group.
  auto& group_listener_map =
      service()->listener()->GetLocalTabGroupListenerMapForTesting();
  ASSERT_EQ(1u, group_listener_map.count(group_id));
  auto& tab_token_mapping =
      group_listener_map.at(group_id).GetWebContentsTokenMapForTesting();
  ASSERT_EQ(1u, tab_token_mapping.size());
  ASSERT_EQ(1u, tab_token_mapping.count(grouped_tab_ptr));

  // Pause tracking.
  service()->PauseTrackingLocalTabGroup(group_id);

  // Remove the tab in the group.
  tab_groups::TabGroupVisualData visual_data = *(browser_1->tab_strip_model()
                                                     ->group_model()
                                                     ->GetTabGroup(group_id)
                                                     ->visual_data());
  std::unique_ptr<content::WebContents> tab =
      browser_1->tab_strip_model()->DetachWebContentsAtForInsertion(1);
  // This kills the group.
  ASSERT_FALSE(
      browser_1->tab_strip_model()->group_model()->ContainsTabGroup(group_id));

  // Recreate the local group and add the tab to it (same browser is fine).
  browser_1->tab_strip_model()->group_model()->AddTabGroup(group_id,
                                                           visual_data);
  browser_1->tab_strip_model()->InsertWebContentsAt(
      1, std::move(tab), AddTabTypes::ADD_NONE, group_id);

  // Resume tracking.
  service()->ResumeTrackingLocalTabGroup(saved_group_id, group_id);

  // Validate that tracking still works.
  // Check that the local and saved ids are still linked in the model.
  EXPECT_EQ(saved_group_id, service()->model()->Get(group_id)->saved_guid());
  // Check that there is still one tab in the model's saved group.
  EXPECT_EQ(1u, service()->model()->Get(group_id)->saved_tabs().size());
  // The listener state should be the same as well.
  EXPECT_EQ(1u, group_listener_map.count(group_id));
  EXPECT_EQ(1u, tab_token_mapping.size());
  EXPECT_EQ(1u, tab_token_mapping.count(grouped_tab_ptr));
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
  service()->SaveGroup(group_id);
  base::Uuid saved_group_id = service()->model()->Get(group_id)->saved_guid();

  // Pause tracking.
  service()->PauseTrackingLocalTabGroup(group_id);

  // Swap the order of the tabs.
  browser_1->tab_strip_model()->MoveWebContentsAt(0, 1, false);

  EXPECT_DEATH(service()->ResumeTrackingLocalTabGroup(saved_group_id, group_id),
               "");
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       KeyedServiceLinksTabIdsToGuidsWhenModelIsLoaded) {
  Browser* browser_1 = AddBrowser();
  ASSERT_EQ(0, browser_1->tab_strip_model()->count());

  // Add 4 tabs to the browser.
  for (size_t i = 0; i < 4; ++i) {
    AddTabToBrowser(browser_1, 0);
  }

  ASSERT_EQ(4, browser_1->tab_strip_model()->count());

  const tab_groups::TabGroupId tab_group_id_1 =
      browser_1->tab_strip_model()->AddToNewGroup({0});
  const tab_groups::TabGroupId tab_group_id_2 =
      browser_1->tab_strip_model()->AddToNewGroup({1, 2});
  const tab_groups::TabGroupId tab_group_id_3 =
      browser_1->tab_strip_model()->AddToNewGroup({3});

  const base::Uuid guid_1 = base::Uuid::GenerateRandomV4();
  const base::Uuid guid_2 = base::Uuid::GenerateRandomV4();

  // Store the guid to tab_group_id association in the keyed service. We should
  // expect at the end of the test, `tab_group_id_3` has no association with the
  // SavedTabGroupModel at all.
  service()->StoreLocalToSavedId(guid_1, tab_group_id_1);
  service()->StoreLocalToSavedId(guid_2, tab_group_id_2);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading in persisted data on startup.
  std::vector<SavedTabGroupTab> group_1_tabs = {
      SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab", guid_1)};
  std::vector<SavedTabGroupTab> group_2_tabs = {
      SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab", guid_2),
      SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab", guid_2)};

  SavedTabGroup saved_group_1(u"Group 1", tab_groups::TabGroupColorId::kGrey,
                              std::move(group_1_tabs), guid_1);
  SavedTabGroup saved_group_2(u"Group 2", tab_groups::TabGroupColorId::kRed,
                              std::move(group_2_tabs), guid_2);
  service()->model()->Add(saved_group_1);
  service()->model()->Add(saved_group_2);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries({});

  // Retrieve the 2 saved groups from the model.
  SavedTabGroupModel* model = service()->model();
  const SavedTabGroup* retrieved_saved_group_1 = model->Get(guid_1);
  const SavedTabGroup* retrieved_saved_group_2 = model->Get(guid_2);

  // Verify saved group 1 and 2 have the correct tab group id.
  ASSERT_TRUE(retrieved_saved_group_1);
  ASSERT_TRUE(retrieved_saved_group_1->local_group_id().has_value());
  EXPECT_EQ(tab_group_id_1, retrieved_saved_group_1->local_group_id().value());

  ASSERT_TRUE(retrieved_saved_group_2);
  ASSERT_TRUE(retrieved_saved_group_2->local_group_id().has_value());
  EXPECT_EQ(tab_group_id_2, retrieved_saved_group_2->local_group_id().value());

  // Expect the model can locate tab group ids for group 1 and 2 but not
  // group 3.
  EXPECT_TRUE(model->Contains(tab_group_id_1));
  EXPECT_TRUE(model->Contains(tab_group_id_2));
  EXPECT_FALSE(model->Contains(tab_group_id_3));

  // StoreLocalToSavedId should only be called before the
  // SavedTabGroupModel is loaded to temporarily preserve id associations before
  // it is emptied an never used again. Calling StoreLocalToSavedId after the
  // SavedTabGroupModel is loaded will never use the data taking up space until
  // the browser is restarted. To prevent this we crash.
  EXPECT_DEATH(service()->StoreLocalToSavedId(guid_1, tab_group_id_1), "");
}

TEST_F(SavedTabGroupKeyedServiceUnitTest,
       KeyedServiceUpdatesOpenTabGroupOnSyncUpdates) {
  Browser* browser = AddBrowser();
  ASSERT_EQ(0, browser->tab_strip_model()->count());

  // Add 1 tab to the browser.
  AddTabToBrowser(browser, 0);
  ASSERT_EQ(1, browser->tab_strip_model()->count());

  const tab_groups::TabGroupId tab_group_id =
      browser->tab_strip_model()->AddToNewGroup({0});
  const base::Uuid guid = base::Uuid::GenerateRandomV4();

  // Store the guid to tab_group_id association in the keyed service.
  service()->StoreLocalToSavedId(guid, tab_group_id);

  // Populate the SavedTabGroupModel with some test data to simulate the browser
  // loading persisted data on startup.
  std::vector<SavedTabGroupTab> group_tabs = {
      SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab", guid)};

  SavedTabGroup saved_group(u"Group", tab_groups::TabGroupColorId::kGrey,
                            std::move(group_tabs), guid);
  service()->model()->Add(saved_group);

  // Notify the KeyedService that the SavedTabGroupModel has loaded all local
  // data triggered by the completion of SavedTabGroupModel::LoadStoredEntries.
  service()->model()->LoadStoredEntries({});

  // Retrieve the saved group from the SavedTabGroupModel.
  SavedTabGroupModel* model = service()->model();
  const SavedTabGroup* retrieved_saved_group = model->Get(guid);

  // Retrieve the tab group from the TabStripModel.
  const TabStripModel* tab_strip_model = browser->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  const TabGroup* tab_group =
      tab_strip_model->group_model()->GetTabGroup(tab_group_id);
  ASSERT_TRUE(tab_group);

  // Verify the visual data of the groups are the same.
  EXPECT_EQ(tab_group->visual_data()->title(), retrieved_saved_group->title());
  EXPECT_EQ(tab_group->visual_data()->color(), retrieved_saved_group->color());

  const std::u16string new_title = u"First new title";
  const tab_groups::TabGroupColorId new_color =
      tab_groups::TabGroupColorId::kOrange;

  tab_groups::TabGroupVisualData visual_data_1(new_title, new_color);

  // Simulate an update on saved groups 1 and 2 from the sync service.
  service()->model()->UpdatedVisualDataFromSync(guid, &visual_data_1);

  // Verify the groups still have the same visual data and that they have
  // updated to the new values.
  EXPECT_EQ(new_title, tab_group->visual_data()->title());
  EXPECT_EQ(new_color, tab_group->visual_data()->color());

  EXPECT_EQ(tab_group->visual_data()->title(), retrieved_saved_group->title());
  EXPECT_EQ(tab_group->visual_data()->color(), retrieved_saved_group->color());
}
