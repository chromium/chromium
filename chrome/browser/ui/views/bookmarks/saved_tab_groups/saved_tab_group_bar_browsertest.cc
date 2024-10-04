// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_utils.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_bar_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
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
          {tab_groups::kTabGroupSyncServiceDesktopMigration,
           tab_groups::kTabGroupsSaveUIUpdate, tab_groups::kTabGroupsSaveV2},
          {});
    } else {
      features_.InitWithFeatures(
          {}, {tab_groups::kTabGroupSyncServiceDesktopMigration,
               tab_groups::kTabGroupsSaveV2});
    }
  }

  bool IsV2UIMigrationEnabled() const { return GetParam(); }

 private:
  base::test::ScopedFeatureList features_;
};

struct ScopedAddObservation : public TabGroupSyncService::Observer {
  explicit ScopedAddObservation(TabGroupSyncService* service_)
      : service(service_) {
    service->AddObserver(this);
  }

  ~ScopedAddObservation() override { service->RemoveObserver(this); }

  void OnTabGroupAdded(const SavedTabGroup& group,
                       TriggerSource source) override {
    last_group_tab_count = group.saved_tabs().size();
    last_trigger_source = source;
  }

  std::optional<int> last_group_tab_count = std::nullopt;
  std::optional<TriggerSource> last_trigger_source = std::nullopt;
  raw_ptr<TabGroupSyncService> service;
};

// Verifies that a saved group can be only be opened in the tabstrip once. If
// it is already open, we will find that group and focus it.
IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       ValidGroupIsOpenedInTabstripOnce) {
  if (IsV2UIMigrationEnabled()) {
    TabGroupSyncService* service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    TabStripModel* model = browser()->tab_strip_model();
    const TabGroupId id = model->AddToNewGroup({0});

    std::optional<SavedTabGroup> group = service->GetGroup(id);
    EXPECT_EQ(id, group->local_group_id());
    EXPECT_TRUE(group);

    const int original_model_count = model->GetTabCount();
    const base::Uuid guid = group->saved_guid();
    service->OpenTabGroup(guid,
                          std::make_unique<TabGroupActionContextDesktop>(
                              browser(), OpeningSource::kOpenedFromRevisitUi));

    group = service->GetGroup(guid);
    EXPECT_TRUE(group->local_group_id());
    EXPECT_TRUE(model->group_model()->ContainsTabGroup(
        group->local_group_id().value()));
    EXPECT_EQ(model->count(), original_model_count);
  } else {
    SavedTabGroupKeyedService* saved_tab_group_service =
        SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
    SavedTabGroupModel* stg_model = saved_tab_group_service->model();
    TabStripModel* model = browser()->tab_strip_model();
    base::Uuid guid = base::Uuid::GenerateRandomV4();

    {  // Add the STG to the model and then open it from the current browser.
      const int original_model_count = model->GetTabCount();

      stg_model->Add(SavedTabGroup(
          std::u16string(u"test_title_1"), tab_groups::TabGroupColorId::kGrey,
          {SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab Title", guid,
                            /*position=*/0)
               .SetTitle(u"Title")
               .SetFavicon(favicon::GetDefaultFavicon())},
          /*position=*/std::nullopt, guid));
      saved_tab_group_service->OpenSavedTabGroupInBrowser(
          browser(), guid, OpeningSource::kOpenedFromRevisitUi);
      const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
      EXPECT_NE(saved_tab_group, nullptr);
      EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
      EXPECT_TRUE(model->group_model()->ContainsTabGroup(
          saved_tab_group->local_group_id().value()));
      EXPECT_NE(model->GetTabCount(), original_model_count);
    }

    {  // The STG is already opened in the saved tab group
      const int original_model_count = model->GetTabCount();

      saved_tab_group_service->OpenSavedTabGroupInBrowser(
          browser(), guid, OpeningSource::kOpenedFromRevisitUi);
      const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
      EXPECT_NE(saved_tab_group, nullptr);
      EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
      EXPECT_TRUE(model->group_model()->ContainsTabGroup(
          saved_tab_group->local_group_id().value()));
      EXPECT_EQ(model->count(), original_model_count);
    }
  }
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       DeletedSavedTabGroupDoesNotOpen) {
  if (IsV2UIMigrationEnabled()) {
    TabGroupSyncService* service =
        TabGroupSyncServiceFactory::GetForProfile(browser()->profile());
    TabStripModel* model = browser()->tab_strip_model();
    const TabGroupId id = model->AddToNewGroup({0});

    std::optional<SavedTabGroup> group = service->GetGroup(id);
    EXPECT_EQ(id, group->local_group_id());
    EXPECT_TRUE(group);

    const base::Uuid guid = group->saved_guid();

    // Close and delete the group.
    model->CloseAllTabsInGroup(id);
    service->RemoveGroup(guid);

    // Attempt to reopen, it should not open.
    service->OpenTabGroup(guid,
                          std::make_unique<TabGroupActionContextDesktop>(
                              browser(), OpeningSource::kOpenedFromRevisitUi));

    group = service->GetGroup(guid);
    EXPECT_FALSE(group);
    EXPECT_FALSE(model->group_model()->ContainsTabGroup(id));
  } else {
    SavedTabGroupKeyedService* saved_tab_group_service =
        SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
    SavedTabGroupModel* stg_model = saved_tab_group_service->model();
    TabStripModel* model = browser()->tab_strip_model();

    base::Uuid guid = base::Uuid::GenerateRandomV4();

    {  // Add an STG, open a group for it in the tabstrip, and delete the STG.
      stg_model->Add(SavedTabGroup(
          std::u16string(u"test_title_1"), tab_groups::TabGroupColorId::kGrey,
          {SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab Title", guid,
                            /*position=*/0)
               .SetTitle(u"Title")
               .SetFavicon(favicon::GetDefaultFavicon())},
          /*position=*/std::nullopt, guid));
      saved_tab_group_service->OpenSavedTabGroupInBrowser(
          browser(), guid, OpeningSource::kOpenedFromRevisitUi);

      const SavedTabGroup* saved_tab_group = stg_model->Get(guid);

      EXPECT_NE(saved_tab_group, nullptr);
      EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
      EXPECT_TRUE(model->group_model()->ContainsTabGroup(
          saved_tab_group->local_group_id().value()));
      saved_tab_group_service->UnsaveGroup(
          saved_tab_group->local_group_id().value(),
          ClosingSource::kDeletedByUser);
    }

    {  // Attempt to reopen the STG, it should not open.
      const int original_tab_count = model->count();
      saved_tab_group_service->OpenSavedTabGroupInBrowser(
          browser(), guid, OpeningSource::kOpenedFromRevisitUi);

      const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
      EXPECT_EQ(saved_tab_group, nullptr);
      EXPECT_EQ(model->count(), original_tab_count);
    }
  }
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       SavedTabGroupLoadStoredEntriesV1) {
  if (IsV2UIMigrationEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());

  {  // Create 1 pinned group
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group{
        u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};
    SavedTabGroupTab tab{GURL("https://www.zombo.com"), u"tab_title",
                         group_guid, 0};

    SavedTabGroupKeyedService* old_service =
        SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
    SavedTabGroupModel* model = old_service->model();
    model->LoadStoredEntries({std::move(group)}, {std::move(tab)});
  }

  EXPECT_EQ(1, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       SavedTabGroupLoadStoredEntriesV2) {
  if (!IsV2UIMigrationEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  TabGroupSyncService* service =
      SavedTabGroupUtils::GetServiceForProfile(browser()->profile());

  {  // Create 1 pinned group
    ScopedAddObservation observer(service);
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group{
        u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};
    SavedTabGroupTab tab{GURL("https://www.zombo.com"), u"tab_title",
                         group_guid, 0};

    TabGroupSyncServiceImpl* service_impl =
        static_cast<TabGroupSyncServiceImpl*>(service);
    SavedTabGroupModel* model = service_impl->GetModelForTesting();
    model->LoadStoredEntries({std::move(group)}, {std::move(tab)});

    // Expect that a remote group was created.
    EXPECT_NE(0, observer.last_group_tab_count);
    EXPECT_EQ(TriggerSource::REMOTE, observer.last_trigger_source);
  }

  {  // Create one unpinned group.
    ScopedAddObservation observer(service);
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group{
        u"group_title", TabGroupColorId::kGrey, {}, std::nullopt, group_guid};
    SavedTabGroupTab tab{GURL("https://www.zombo.com"), u"tab_title",
                         group_guid, 0};

    TabGroupSyncServiceImpl* service_impl =
        static_cast<TabGroupSyncServiceImpl*>(service);
    SavedTabGroupModel* model = service_impl->GetModelForTesting();
    model->AddedFromSync({std::move(group)});
    model->AddTabToGroupFromSync(group_guid, {std::move(tab)});

    // Expect that a remote group was created.
    EXPECT_NE(0, observer.last_group_tab_count);
    EXPECT_EQ(TriggerSource::REMOTE, observer.last_trigger_source);
  }

  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(1, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       EmptySavedTabGroupDoesntDisplayV1) {
  if (IsV2UIMigrationEnabled()) {
    GTEST_SKIP() << "N/A for V2";
  }

  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());

  {  // Create an empty group.
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group{
        u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};

    SavedTabGroupKeyedService* old_service =
        SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
    SavedTabGroupModel* model = old_service->model();
    model->LoadStoredEntries({std::move(group)}, {});
  }

  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       EmptySavedTabGroupDoesntDisplayV2) {
  if (!IsV2UIMigrationEnabled()) {
    GTEST_SKIP() << "N/A for V1";
  }
  ASSERT_TRUE(SavedTabGroupUtils::IsEnabledForProfile(browser()->profile()));
  TabGroupSyncService* service =
      SavedTabGroupUtils::GetServiceForProfile(browser()->profile());

  {  // Create an empty group.
    ScopedAddObservation observer(service);
    base::Uuid group_guid = base::Uuid::GenerateRandomV4();
    SavedTabGroup group{
        u"group_title", TabGroupColorId::kGrey, {}, 0, group_guid};

    TabGroupSyncServiceImpl* service_impl =
        static_cast<TabGroupSyncServiceImpl*>(service);
    SavedTabGroupModel* model = service_impl->GetModelForTesting();
    model->LoadStoredEntries({std::move(group)}, {});

    // Expect that a remote group was created.
    EXPECT_EQ(std::nullopt, observer.last_group_tab_count);
    EXPECT_EQ(std::nullopt, observer.last_trigger_source);
  }

  const SavedTabGroupBar* saved_tab_group_bar =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->bookmark_bar()
          ->saved_tab_group_bar();
  EXPECT_EQ(0, saved_tab_group_bar->GetNumberOfVisibleGroups());
}

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupBarBrowserTest,
                         testing::Bool());

}  // namespace tab_groups
