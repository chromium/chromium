// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_web_contents_listener.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kFirstURL[] = "https://url1.com";
constexpr char kSecondURL[] = "https://url2.com";
constexpr char kThirdURL[] = "https://url3.com";

}  // anonymous namespace

class ListenerDeferredTest : public InProcessBrowserTest {
 public:
  ListenerDeferredTest() {
    features_.InitWithFeatures({data_sharing::features::kDataSharingFeature},
                               {});
  }

  void SetUpOnMainThread() override {
    other_tab_ = tab_strip_model()->GetTabForWebContents(
        chrome::AddAndReturnTabAt(browser(), GURL(kFirstURL), -1, true));

    test_tab_ = tab_strip_model()->GetTabForWebContents(
        chrome::AddAndReturnTabAt(browser(), GURL(kFirstURL), -1, true));
    WaitForNavigationCompleted();

    content::RunAllTasksUntilIdle();
  }

  void TearDownOnMainThread() override {
    other_tab_ = nullptr;
    test_tab_ = nullptr;
    tab_strip_model()->CloseAllTabs();
  }

  void Wait() {
    // Post a dummy task in the current thread and wait for its completion so
    // that any already posted tasks are completed.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  void ForegroundTestTab() {
    tab_strip_model()->ActivateTabAt(
        tab_strip_model()->GetIndexOfTab(test_tab_));

    // Ensure that the tab was foregrounded.
    ASSERT_TRUE(test_tab_->IsActivated()) << "The tab was not foregrounded.";
  }

  void BackgroundTestTab() {
    tab_strip_model()->ActivateTabAt(
        tab_strip_model()->GetIndexOfTab(other_tab_));

    // Ensure that the tab was backgrounded.
    ASSERT_FALSE(test_tab_->IsActivated()) << "The tab was not backgrounded.";
  }

  void WaitForNavigationCompleted() {
    if (test_tab_->GetContents()->IsLoading()) {
      content::TestNavigationObserver navigation_observer(
          test_tab_->GetContents());
      navigation_observer.Wait();
    } else {
    }
  }

  void SaveGroup() {
    local_group_id_ = tab_strip_model()->AddToNewGroup(
        {tab_strip_model()->GetIndexOfTab(test_tab_)});

    tab_groups::SavedTabGroupModel* model = saved_tab_group_model();

    ASSERT_TRUE(model);
    ASSERT_TRUE(local_group_id_.has_value());

    const tab_groups::SavedTabGroup* group =
        model->Get(local_group_id_.value());
    ASSERT_TRUE(group);
    ASSERT_EQ(1u, group->saved_tabs().size());

    // Since we have just one tab in the group, we can retrieve it
    const tab_groups::SavedTabGroupTab& tab = group->saved_tabs()[0];
    saved_tab_guid_ = tab.saved_tab_guid();
  }

  void AttemptNavigationFromSync(const GURL& url) {
    ASSERT_FALSE(test_tab_->GetContents()->IsLoading());

    tab_groups::SavedTabGroupModel* stg_model = saved_tab_group_model();
    ASSERT_TRUE(stg_model);

    ASSERT_TRUE(local_group_id_.has_value());
    const tab_groups::SavedTabGroup* group =
        stg_model->Get(local_group_id_.value());
    ASSERT_TRUE(group);

    // Retrieve the existing tab using the saved_tab_guid_
    ASSERT_TRUE(saved_tab_guid_.has_value());
    const tab_groups::SavedTabGroupTab* existing_tab =
        group->GetTab(saved_tab_guid_.value());
    ASSERT_TRUE(existing_tab);

    // Create a new SavedTabGroupTab representing the remote update
    tab_groups::SavedTabGroupTab remote_tab = *existing_tab;

    // Update the URL to simulate the sync change
    remote_tab.SetURL(url);

    // Merge the remote tab into the model
    const tab_groups::SavedTabGroupTab* merged_tab =
        stg_model->MergeRemoteTab(remote_tab);
    ASSERT_TRUE(merged_tab);

    // The TabGroupSyncService may post tasks to perform actions on observers.
    Wait();

    // After the task is posted, also verify that the webcontents completed
    // loading.
    WaitForNavigationCompleted();
  }

  GURL CurrentTabURL() {
    return test_tab_->GetContents()->GetLastCommittedURL();
  }

 private:
  base::test::ScopedFeatureList features_;

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }
  Profile* profile() { return browser()->profile(); }
  tab_groups::SavedTabGroupModel* saved_tab_group_model() {
    tab_groups::TabGroupSyncService* service =
        tab_groups::TabGroupSyncServiceFactory::GetForProfile(profile());
    EXPECT_TRUE(service);

    auto* service_impl =
        static_cast<tab_groups::TabGroupSyncServiceImpl*>(service);
    return service_impl->GetModel();
  }

  raw_ptr<tabs::TabInterface> other_tab_;
  raw_ptr<tabs::TabInterface> test_tab_;
  std::optional<tab_groups::TabGroupId> local_group_id_;
  std::optional<base::Uuid> saved_tab_guid_;
};

IN_PROC_BROWSER_TEST_F(ListenerDeferredTest, NavigatesWhenStartsForegrounded) {
  ForegroundTestTab();
  SaveGroup();

  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());
  AttemptNavigationFromSync(GURL(kSecondURL));
  EXPECT_EQ(GURL(kSecondURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kThirdURL));
  EXPECT_EQ(GURL(kThirdURL), CurrentTabURL());
}

IN_PROC_BROWSER_TEST_F(ListenerDeferredTest,
                       DoesntNavigateWhenStartsBackgrounded) {
  BackgroundTestTab();
  SaveGroup();
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kSecondURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kThirdURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());
}

IN_PROC_BROWSER_TEST_F(ListenerDeferredTest, ForegroundingAllowsNavigtation) {
  BackgroundTestTab();
  SaveGroup();
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  ForegroundTestTab();

  AttemptNavigationFromSync(GURL(kSecondURL));
  EXPECT_EQ(GURL(kSecondURL), CurrentTabURL());
}

IN_PROC_BROWSER_TEST_F(ListenerDeferredTest,
                       ForegroundingAfterANavigationNavigates) {
  BackgroundTestTab();
  SaveGroup();
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kSecondURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());
  ForegroundTestTab();
  WaitForNavigationCompleted();

  EXPECT_EQ(GURL(kSecondURL), CurrentTabURL());
}

IN_PROC_BROWSER_TEST_F(
    ListenerDeferredTest,
    DoesntNavigateWhenBackgroundedAfterStartingForegrounded) {
  ForegroundTestTab();
  SaveGroup();
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  BackgroundTestTab();

  AttemptNavigationFromSync(GURL(kSecondURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kThirdURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());
}

IN_PROC_BROWSER_TEST_F(ListenerDeferredTest,
                       OnlyNavigatesMostRecentNavigationURLOnForeground) {
  BackgroundTestTab();
  SaveGroup();
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  AttemptNavigationFromSync(GURL(kSecondURL));
  AttemptNavigationFromSync(GURL(kThirdURL));
  EXPECT_EQ(GURL(kFirstURL), CurrentTabURL());

  ForegroundTestTab();
  WaitForNavigationCompleted();
  EXPECT_EQ(GURL(kThirdURL), CurrentTabURL());
}
