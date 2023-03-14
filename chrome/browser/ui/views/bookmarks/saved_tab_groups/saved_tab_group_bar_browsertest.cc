// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>

#include "base/test/bind.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/saved_tab_groups/saved_tab_group.h"
#include "components/saved_tab_groups/saved_tab_group_model.h"
#include "components/saved_tab_groups/saved_tab_group_tab.h"
#include "components/tab_groups/tab_group_id.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using SavedTabGroupBarBrowserTest = InProcessBrowserTest;

// Verifies that a saved group can be only be opened in the tabstrip once. If
// it is already open, we will find that group and focus it.
IN_PROC_BROWSER_TEST_F(SavedTabGroupBarBrowserTest,
                       ValidGroupIsOpenedInTabstripOnce) {
  SavedTabGroupKeyedService* saved_tab_group_service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
  SavedTabGroupModel* stg_model = saved_tab_group_service->model();
  TabStripModel* model = browser()->tab_strip_model();
  base::GUID guid = base::GUID::GenerateRandomV4();

  {  // Add the STG to the model and then open it from the current browser.
    const int original_model_count = model->GetTabCount();

    stg_model->Add(SavedTabGroup(
        std::u16string(u"test_title_1"), tab_groups::TabGroupColorId::kGrey,
        {SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab Title", guid)
             .SetTitle(u"Title")
             .SetFavicon(favicon::GetDefaultFavicon())},
        guid));
    saved_tab_group_service->OpenSavedTabGroupInBrowser(browser(), guid);
    const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
    EXPECT_NE(saved_tab_group, nullptr);
    EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
    EXPECT_TRUE(model->group_model()->ContainsTabGroup(
        saved_tab_group->local_group_id().value()));
    EXPECT_NE(model->GetTabCount(), original_model_count);
  }

  {  // The STG is already opened in the saved tab group
    const int original_model_count = model->GetTabCount();

    saved_tab_group_service->OpenSavedTabGroupInBrowser(browser(), guid);
    const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
    EXPECT_NE(saved_tab_group, nullptr);
    EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
    EXPECT_TRUE(model->group_model()->ContainsTabGroup(
        saved_tab_group->local_group_id().value()));
    EXPECT_EQ(model->count(), original_model_count);
  }
}

IN_PROC_BROWSER_TEST_F(SavedTabGroupBarBrowserTest,
                       DeletedSavedTabGroupDoesNotOpen) {
  SavedTabGroupKeyedService* saved_tab_group_service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
  SavedTabGroupModel* stg_model = saved_tab_group_service->model();
  TabStripModel* model = browser()->tab_strip_model();

  base::GUID guid = base::GUID::GenerateRandomV4();

  {  // Add an STG, open a group for it in the tabstrip, and delete the STG.
    stg_model->Add(SavedTabGroup(
        std::u16string(u"test_title_1"), tab_groups::TabGroupColorId::kGrey,
        {SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab Title", guid)
             .SetTitle(u"Title")
             .SetFavicon(favicon::GetDefaultFavicon())},
        guid));
    saved_tab_group_service->OpenSavedTabGroupInBrowser(browser(), guid);

    const SavedTabGroup* saved_tab_group = stg_model->Get(guid);

    EXPECT_NE(saved_tab_group, nullptr);
    EXPECT_TRUE(saved_tab_group->local_group_id().has_value());
    EXPECT_TRUE(model->group_model()->ContainsTabGroup(
        saved_tab_group->local_group_id().value()));
    saved_tab_group_service->UnsaveGroup(
        saved_tab_group->local_group_id().value());
  }

  {  // Attempt to reopen the STG, it should not open.
    const int original_tab_count = model->count();
    saved_tab_group_service->OpenSavedTabGroupInBrowser(browser(), guid);

    const SavedTabGroup* saved_tab_group = stg_model->Get(guid);
    EXPECT_EQ(saved_tab_group, nullptr);
    EXPECT_EQ(model->count(), original_tab_count);
  }
}

// Verify the saved status of a group is updated when it is added and removed
// from the SavedTabGroupModel.
IN_PROC_BROWSER_TEST_F(SavedTabGroupBarBrowserTest,
                       GroupMarkedAsSavedIfInModel) {
  SavedTabGroupKeyedService* saved_tab_group_service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
  SavedTabGroupModel* stg_model = saved_tab_group_service->model();
  TabStripModel* model = browser()->tab_strip_model();
  base::GUID guid = base::GUID::GenerateRandomV4();

  // Add a tab to a new group and expect the new group is not saved.
  chrome::AddTabAt(browser(), GURL("chrome://newtab"), -1, true);
  tab_groups::TabGroupId group_id = model->AddToNewGroup({1});
  EXPECT_FALSE(saved_tab_group_service->model()->Contains(group_id));

  // Add the group to the SavedTabGroupModel and expect it is saved.
  stg_model->Add(SavedTabGroup(
      std::u16string(u"test_title_1"), tab_groups::TabGroupColorId::kGrey,
      {SavedTabGroupTab(GURL("chrome://newtab"), u"New Tab Title", guid)
           .SetFavicon(favicon::GetDefaultFavicon())},
      guid, absl::nullopt, group_id));
  EXPECT_TRUE(saved_tab_group_service->model()->Contains(group_id));

  // Remove the group from the SavedTabGroupModel and expect it is no longer
  // saved.
  stg_model->Remove(group_id);
  EXPECT_FALSE(saved_tab_group_service->model()->Contains(group_id));
}
