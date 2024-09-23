// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <optional>

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/tab_group_sync/tab_group_sync_service_factory.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/tab_group_action_context_desktop.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/features.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/saved_tab_groups/tab_group_sync_service.h"
#include "components/saved_tab_groups/types.h"
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
           tab_groups::kTabGroupsSaveV2},
          {});
    } else {
      features_.InitWithFeatures(
          {}, {tab_groups::kTabGroupSyncServiceDesktopMigration,
               tab_groups::kTabGroupsSaveV2});
    }
  }

 private:
  base::test::ScopedFeatureList features_;
};

// Verifies that a saved group can be only be opened in the tabstrip once. If
// it is already open, we will find that group and focus it.
IN_PROC_BROWSER_TEST_P(SavedTabGroupBarBrowserTest,
                       ValidGroupIsOpenedInTabstripOnce) {
  if (GetParam()) {
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
  if (GetParam()) {
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

INSTANTIATE_TEST_SUITE_P(SavedTabGroupBar,
                         SavedTabGroupBarBrowserTest,
                         testing::Bool());

}  // namespace tab_groups
