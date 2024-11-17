// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/task/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {
namespace {

class SyncBridgeModelObserverForTest : public SavedTabGroupModelObserver {
 public:
  SyncBridgeModelObserverForTest() = default;
  ~SyncBridgeModelObserverForTest() override = default;

  void SavedTabGroupAddedLocally(const base::Uuid& guid) override {
    write_events_since_last_++;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void SavedTabGroupUpdatedLocally(
      const base::Uuid& group_guid,
      const std::optional<base::Uuid>& tab_guid) override {
    write_events_since_last_++;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void SavedTabGroupTabMovedLocally(const base::Uuid& group_guid,
                                    const base::Uuid& tab_guid) override {
    write_events_since_last_++;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void SavedTabGroupRemovedLocally(
      const SavedTabGroup& removed_group) override {
    write_events_since_last_++;
    if (quit_) {
      std::move(quit_).Run();
    }
  }

  void SavedTabGroupLocalIdChanged(const base::Uuid& saved_group_id) override {
    // TODO(shaktisahu): Should we separately check DB write and sync writes?
  }

  int write_events_since_last() { return write_events_since_last_; }

  void reset_counts() { write_events_since_last_ = 0; }

  void WaitForPostedTasks() {
    // Post a dummy task in the current thread and wait for its completion so
    // that any already posted tasks are completed.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::OnceClosure quit_;
  int write_events_since_last_ = 0;
};

}  // namespace

// Tests that are meant to perform user actions on browser and tab strip and
// verify at bridge layer (via a model observer API) that events are written as
// expected to the sync and database.
class TabGroupSyncNavigationIntegrationTest : public InProcessBrowserTest {
 public:
  TabGroupSyncNavigationIntegrationTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;

    enabled_features.push_back(tab_groups::kTabGroupsSaveV2);
    enabled_features.push_back(tab_groups::kTabGroupsSaveUIUpdate);
    enabled_features.push_back(
        tab_groups::kTabGroupSyncServiceDesktopMigration);

    feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

  ~TabGroupSyncNavigationIntegrationTest() override = default;
  TabGroupSyncNavigationIntegrationTest(
      const TabGroupSyncNavigationIntegrationTest&) = delete;
  TabGroupSyncNavigationIntegrationTest& operator=(
      const TabGroupSyncNavigationIntegrationTest&) = delete;

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("example.com", "127.0.0.1");
    ASSERT_TRUE(embedded_https_test_server().Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetupSyncBridgeModelObserver() {
    TabGroupSyncServiceImpl* service_impl =
        static_cast<TabGroupSyncServiceImpl*>(service());
    SavedTabGroupModel* model = service_impl->GetModelForTesting();
    model->AddObserver(&sync_bridge_model_observer_);
  }

  content::WebContents* AddTabToBrowser(int index) {
    std::unique_ptr<content::WebContents> web_contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));

    content::WebContents* web_contents_ptr = web_contents.get();

    browser()->tab_strip_model()->AddWebContents(
        std::move(web_contents), index,
        ui::PageTransition::PAGE_TRANSITION_TYPED, AddTabTypes::ADD_ACTIVE);

    return web_contents_ptr;
  }

  TabGroupSyncService* service() {
    return TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  }

  void Wait() { sync_bridge_model_observer_.WaitForPostedTasks(); }

  void VerifyWrittenToSync(int write_events_since_last) {
    // Verify that observers aren't notified of a local update. We use this to
    // as a proxy to verify that nothing was written to sync.
    Wait();
    EXPECT_EQ(write_events_since_last,
              sync_bridge_model_observer_.write_events_since_last());

    // Reset the counts to zero for the next verification call.
    sync_bridge_model_observer_.reset_counts();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  SyncBridgeModelObserverForTest sync_bridge_model_observer_;
};

IN_PROC_BROWSER_TEST_F(TabGroupSyncNavigationIntegrationTest,
                       OpenTabGroupFromRevisitUiDoesNotPropagateToSync) {
  SetupSyncBridgeModelObserver();

  EXPECT_EQ(0u, service()->GetAllGroups().size());
  TabStripModel* const tabstrip = browser()->tab_strip_model();
  ASSERT_EQ(1, tabstrip->count());

  // Create a local tab group with one tab.
  TabGroupId local_group_id = browser()->tab_strip_model()->AddToNewGroup({0});
  VerifyWrittenToSync(/*write_events_since_last=*/1);

  // Ensure the group was saved.
  EXPECT_EQ(1u, service()->GetAllGroups().size());
  std::optional<SavedTabGroup> retrieved_group =
      service()->GetGroup(local_group_id);
  base::Uuid saved_id = retrieved_group->saved_guid();

  // Close the group. The mapping should be dropped. Add an extra tab to prevent
  // the browser from closing due to having zero tabs.
  AddTabToBrowser(0);
  browser()->tab_strip_model()->CloseAllTabsInGroup(local_group_id);
  EXPECT_FALSE(browser()->tab_strip_model()->group_model()->ContainsTabGroup(
      local_group_id));

  // Reopen the group. The group shouldn't send a write event to sync.
  std::unique_ptr<TabGroupActionContextDesktop> desktop_context =
      std::make_unique<TabGroupActionContextDesktop>(browser(),
                                                     OpeningSource::kUnknown);
  service()->OpenTabGroup(saved_id, std::move(desktop_context));

  retrieved_group = service()->GetGroup(saved_id);
  EXPECT_TRUE(retrieved_group->local_group_id().has_value());
  EXPECT_NE(local_group_id, retrieved_group->local_group_id());
  EXPECT_TRUE(browser()->tab_strip_model()->group_model()->ContainsTabGroup(
      retrieved_group->local_group_id().value()));

  // There should be no write event back to sync.
  VerifyWrittenToSync(/*write_events_since_last=*/0);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncNavigationIntegrationTest,
                       NavigateTabFromSyncDoesNotPropagateBackToSync) {
  SetupSyncBridgeModelObserver();
  TabGroupSyncServiceImpl* service_impl =
      static_cast<TabGroupSyncServiceImpl*>(service());
  SavedTabGroupModel* model = service_impl->GetModelForTesting();

  TabStripModel* const tabstrip = browser()->tab_strip_model();

  // Create a local tab group with one tab.
  ASSERT_EQ(1, tabstrip->count());
  const tab_groups::TabGroupId local_id = tabstrip->AddToNewGroup({0});
  VerifyWrittenToSync(/*write_events_since_last=*/1);

  // Verify the saved tab group since the group will be autosaved.
  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->saved_tabs()[0].local_tab_id().has_value());

  // Simulate an incoming navigation from sync.
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/simple.html");
  SavedTabGroupTab incoming_tab = retrieved_group->saved_tabs()[0];
  incoming_tab.SetURL(url);
  incoming_tab.SetTitle(u"Example Page");

  model->MergeRemoteTab(incoming_tab);
  Wait();

  content::WaitForLoadStop(tabstrip->GetWebContentsAt(0));

  // There should be no write event back to sync.
  VerifyWrittenToSync(/*write_events_since_last=*/0);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncNavigationIntegrationTest,
                       AddTabFromSyncDoesNotPropagateBackToSync) {
  SetupSyncBridgeModelObserver();
  TabGroupSyncServiceImpl* service_impl =
      static_cast<TabGroupSyncServiceImpl*>(service());
  SavedTabGroupModel* model = service_impl->GetModelForTesting();

  TabStripModel* const tabstrip = browser()->tab_strip_model();

  // Create a local tab group with one tab.
  ASSERT_EQ(1, tabstrip->count());
  const tab_groups::TabGroupId local_id = tabstrip->AddToNewGroup({0});
  VerifyWrittenToSync(/*write_events_since_last=*/1);

  // Verify the saved tab group since the group will be autosaved.
  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->saved_tabs()[0].local_tab_id().has_value());
  base::Uuid group_guid = retrieved_group->saved_guid();

  // Simulate a tab added event from sync.
  const GURL url =
      embedded_https_test_server().GetURL("example.com", "/simple.html");
  SavedTabGroupTab incoming_tab(url, u"Example Page", group_guid,
                                std::make_optional(1));
  model->AddTabToGroupFromSync(group_guid, incoming_tab);
  Wait();

  retrieved_group = service()->GetGroup(local_id);
  EXPECT_EQ(2u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->saved_tabs()[1].local_tab_id().has_value());

  const auto& retrieved_tab = retrieved_group->saved_tabs()[1];
  EXPECT_EQ(retrieved_tab.saved_tab_guid(), incoming_tab.saved_tab_guid());

  // The incoming tab should have opened in local group.
  EXPECT_EQ(2, tabstrip->count());
  EXPECT_EQ(
      2u, tabstrip->group_model()->GetTabGroup(local_id)->ListTabs().length());

  content::WaitForLoadStop(tabstrip->GetWebContentsAt(1));

  // There should be no write event back to sync.
  VerifyWrittenToSync(/*write_events_since_last=*/0);
}

IN_PROC_BROWSER_TEST_F(TabGroupSyncNavigationIntegrationTest,
                       AddTabLocallyDoesPropagateToSync) {
  SetupSyncBridgeModelObserver();
  TabStripModel* const tabstrip = browser()->tab_strip_model();

  // Create a local tab group.
  ASSERT_EQ(1, tabstrip->count());
  const tab_groups::TabGroupId local_id = tabstrip->AddToNewGroup({0});
  VerifyWrittenToSync(/*write_events_since_last=*/1);

  // Verify the saved tab group since the group will be autosaved.
  std::optional<SavedTabGroup> retrieved_group = service()->GetGroup(local_id);
  EXPECT_TRUE(retrieved_group.has_value());
  EXPECT_EQ(1u, retrieved_group->saved_tabs().size());
  EXPECT_TRUE(retrieved_group->saved_tabs()[0].local_tab_id().has_value());

  // Create a tab locally and add it to the group. The tab should be written to
  // sync.
  AddTabToBrowser(1);
  tabstrip->AddToExistingGroup({1}, local_id,
                               /*add_to_end=*/true);
  VerifyWrittenToSync(/*write_events_since_last=*/1);
}

}  // namespace tab_groups
