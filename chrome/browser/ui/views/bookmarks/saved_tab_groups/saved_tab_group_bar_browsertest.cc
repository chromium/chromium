// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include "base/test/bind.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_keyed_service.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_model.h"
#include "chrome/browser/ui/tabs/saved_tab_groups/saved_tab_group_service_factory.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using SavedTabGroupBarBrowserTest = InProcessBrowserTest;

// Verifies that a saved group can be only be opened in the tabstrip once. If
// it is already open, we will find that group and focus it.
IN_PROC_BROWSER_TEST_F(SavedTabGroupBarBrowserTest,
                       ValidGroupIsOpenedInTabstripOnce) {
  SavedTabGroupKeyedService* saved_tab_group_service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(saved_tab_group_service, nullptr);

  SavedTabGroupModel* stg_model = saved_tab_group_service->model();
  ASSERT_NE(stg_model, nullptr);

  SavedTabGroupTab saved_tab_group_tab(GURL("chrome://newtab"), u"Title",
                                       favicon::GetDefaultFavicon());
  SavedTabGroup saved_tab_group(
      tab_groups::TabGroupId::GenerateNew(), std::u16string(u"test_title_1"),
      tab_groups::TabGroupColorId::kGrey, {saved_tab_group_tab});
  stg_model->Add(saved_tab_group);
  ASSERT_TRUE(stg_model->Contains(saved_tab_group.group_id));
  ASSERT_EQ(stg_model->Count(), 1);

  TabStripModel* model = browser()->tab_strip_model();
  int current_tabs = model->GetTabCount();

  chrome::OpenSavedTabGroup(
      browser(), base::BindLambdaForTesting([=]() {
        return static_cast<content::PageNavigator*>(browser());
      }),
      saved_tab_group.group_id, saved_tab_group.saved_tabs.size());

  EXPECT_TRUE(model->group_model()->ContainsTabGroup(saved_tab_group.group_id));
  EXPECT_EQ(current_tabs + int(saved_tab_group.saved_tabs.size()),
            model->count());

  current_tabs = model->count();
  chrome::OpenSavedTabGroup(
      browser(), base::BindLambdaForTesting([=]() {
        return static_cast<content::PageNavigator*>(browser());
      }),
      saved_tab_group.group_id, saved_tab_group.saved_tabs.size());

  EXPECT_TRUE(model->group_model()->ContainsTabGroup(saved_tab_group.group_id));
  EXPECT_EQ(current_tabs, model->count());
}

// Verifies that attempting to open a deleted group will do nothing.
IN_PROC_BROWSER_TEST_F(SavedTabGroupBarBrowserTest, AttemptToOpenDeletedGroup) {
  SavedTabGroupKeyedService* saved_tab_group_service =
      SavedTabGroupServiceFactory::GetForProfile(browser()->profile());
  ASSERT_NE(saved_tab_group_service, nullptr);

  SavedTabGroupModel* stg_model = saved_tab_group_service->model();
  ASSERT_NE(stg_model, nullptr);

  SavedTabGroupTab saved_tab_group_tab(GURL("chrome://newtab"), u"Title",
                                       favicon::GetDefaultFavicon());
  SavedTabGroup saved_tab_group(
      tab_groups::TabGroupId::GenerateNew(), std::u16string(u"test_title_1"),
      tab_groups::TabGroupColorId::kGrey, {saved_tab_group_tab});
  stg_model->Add(saved_tab_group);
  ASSERT_TRUE(stg_model->Contains(saved_tab_group.group_id));
  ASSERT_EQ(stg_model->Count(), 1);

  TabStripModel* model = browser()->tab_strip_model();
  int current_tabs = model->GetTabCount();

  chrome::OpenSavedTabGroup(
      browser(), base::BindLambdaForTesting([=]() {
        return static_cast<content::PageNavigator*>(browser());
      }),
      saved_tab_group.group_id, saved_tab_group.saved_tabs.size());

  EXPECT_TRUE(model->group_model()->ContainsTabGroup(saved_tab_group.group_id));
  EXPECT_EQ(current_tabs + int(saved_tab_group.saved_tabs.size()),
            model->count());

  stg_model->Remove(saved_tab_group.group_id);
  model->CloseAllTabsInGroup(saved_tab_group.group_id);
  current_tabs = model->count();

  chrome::OpenSavedTabGroup(
      browser(), base::BindLambdaForTesting([=]() {
        return static_cast<content::PageNavigator*>(browser());
      }),
      saved_tab_group.group_id, saved_tab_group.saved_tabs.size());

  EXPECT_FALSE(
      model->group_model()->ContainsTabGroup(saved_tab_group.group_id));
  EXPECT_EQ(current_tabs, model->count());
}
