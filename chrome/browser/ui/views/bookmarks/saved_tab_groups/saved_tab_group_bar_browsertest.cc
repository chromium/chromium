// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/data_sharing/public/features.h"
#include "components/saved_tab_groups/internal/saved_tab_group_model.h"
#include "components/saved_tab_groups/internal/tab_group_sync_service_impl.h"
#include "components/saved_tab_groups/public/features.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/saved_tab_groups/public/tab_group_sync_service.h"
#include "components/saved_tab_groups/public/types.h"
#include "components/tab_groups/tab_group_color.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_groups {

class SavedTabGroupBarBrowserTest : public InProcessBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  SavedTabGroupBarBrowserTest() {
    if (GetParam()) {
      features_.InitWithFeatures(
          {data_sharing::features::kDataSharingFeature},
          {data_sharing::features::kDataSharingJoinOnly});
    } else {
      features_.InitWithFeatures(
          {}, {data_sharing::features::kDataSharingFeature,
               data_sharing::features::kDataSharingJoinOnly});
    }
  }
  void Wait() {
    // Post a dummy task in the current thread and wait for its completion so
    // that any already posted tasks are completed.
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies that a saved group can be only be opened in the tabstrip once. If
// it is already open, we will find that group and focus it.
IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       ValidGroupIsOpenedInTabstripOnce) {
  TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());

  TabStripModel* model = browser()->tab_strip_model();
  const TabGroupId group_id = model->AddToNewGroup({0});
  // Adding a new tab group posts a task. Wait for it to resolve.
  Wait();

  std::optional<SavedTabGroup> saved_group = service->GetGroup(group_id);
  ASSERT_TRUE(saved_group);
  EXPECT_EQ(group_id, saved_group->local_group_id());

  const int original_model_count = model->GetTabCount();
  const base::Uuid guid = saved_group->saved_guid();
  std::optional<LocalTabGroupID> opened_group_id = service->OpenTabGroup(
      guid, std::make_unique<TabGroupActionContextDesktop>(
                browser(), OpeningSource::kOpenedFromRevisitUi));

  EXPECT_TRUE(opened_group_id.has_value());
  EXPECT_TRUE(model->group_model()->ContainsTabGroup(opened_group_id.value()));
  EXPECT_EQ(model->count(), original_model_count);
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       DeletedSavedTabGroupDoesNotOpen) {
  TabGroupSyncService* service =
      tab_groups::TabGroupSyncServiceFactory::GetForProfile(
          browser()->profile());
  TabStripModel* model = browser()->tab_strip_model();
  const TabGroupId group_id = model->AddToNewGroup({0});
  Wait();

  std::optional<SavedTabGroup> saved_group = service->GetGroup(group_id);
  ASSERT_TRUE(saved_group);
  EXPECT_EQ(group_id, saved_group->local_group_id());

  const base::Uuid guid = saved_group->saved_guid();
  // Close and delete the group.
  model->CloseAllTabsInGroup(group_id);
  service->RemoveGroup(guid);

  // Attempt to reopen, it should not open.
  std::optional<LocalTabGroupID> opened_group_id = service->OpenTabGroup(
      guid, std::make_unique<TabGroupActionContextDesktop>(
                browser(), OpeningSource::kOpenedFromRevisitUi));

  EXPECT_FALSE(opened_group_id);
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       SavedTabGroupLoadStoredEntries) {
  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());

  // Create 1 pinned group
  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroup group{
      u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};
  SavedTabGroupTab tab{GURL("https://www.zombo.com"), u"tab_title", group_guid,
                       0};
  group.AddTabFromSync(std::move(tab));

  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddGroup(std::move(group));
  // Wait until the add group task resolves.
  Wait();

  EXPECT_EQ(1, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest, SavedTabGroupAdded) {
  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());

  // Create 1 pinned group
  base::Uuid pinned_group_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroup pinned_group{
      u"group_title", TabGroupColorId::kGrey, {}, 1, pinned_group_guid};
  SavedTabGroupTab pinned_tab{GURL("https://www.zombo.com"), u"tab_title",
                              pinned_group_guid, 0};
  pinned_group.AddTabFromSync(std::move(pinned_tab));
  service->AddGroup(std::move(pinned_group));

  // Create 1 unpinned group.
  base::Uuid unpinned_group_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroup unpinned_group{u"group_title",
                               TabGroupColorId::kGrey,
                               {},
                               std::nullopt,
                               unpinned_group_guid};
  SavedTabGroupTab unpinned_tab{GURL("https://www.zombo.com"), u"tab_title",
                                unpinned_group_guid, 0};
  unpinned_group.AddTabFromSync(std::move(unpinned_tab));
  service->AddGroup(std::move(unpinned_group));

  // Wait until the add group tasks have resolved.
  Wait();

  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(1, saved_tab_group_bar->GetNumberOfVisibleGroups());
  EXPECT_EQ(service->GetAllGroups().size(), 2u);
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       EmptySavedTabGroupDoesntDisplay) {
  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());

  // Create an empty group.
  base::Uuid group_guid = base::Uuid::GenerateRandomV4();
  SavedTabGroup group{
      u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};

  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  service->AddGroup(std::move(group));
  Wait();

  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

// Disabled since it does not work for trybots but helps test performance
// issues.
IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       DISABLED_LargeNumberOfSavedTabGroups) {
  constexpr int kLargeGroupCount = 1000;
  constexpr int kLargeTabInGroupCount = 1000;

  TabGroupSyncService* service =
      TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
  auto* service_impl = static_cast<TabGroupSyncServiceImpl*>(service);
  SavedTabGroupModel* model = service_impl->GetModel();
  for (int i = 0; i < kLargeGroupCount; i++) {
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group(u"Group Title", TabGroupColorId::kGrey, {}, 0,
                        group_guid);
    for (int j = 0; j < kLargeTabInGroupCount; j++) {
      GURL url("https://www.example.com");
      SavedTabGroupTab tab(url, u"Tab Title", group_guid, j);
      tab.SetFavicon(favicon::GetDefaultFavicon());
      group.AddTabFromSync(std::move(tab));
    }
    model->AddedFromSync(std::move(group));
  }

  Wait();

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  auto* bookmark_bar = browser_view->bookmark_bar();
  const SavedTabGroupBar* saved_tab_group_bar =
      bookmark_bar->saved_tab_group_bar();
  EXPECT_EQ(100, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupBarBrowserTest,
                         testing::Bool());

}  // namespace tab_groups
