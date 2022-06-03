// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/existing_tab_group_sub_menu_model.h"

#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "url/gurl.h"

namespace {

class ExistingTabGroupSubMenuModelTest : public BrowserWithTestWindowTest {
 public:
  ExistingTabGroupSubMenuModelTest() : BrowserWithTestWindowTest() {}
};

// Ensure that add to group submenu only appears when there is another group to
// move the tab into.
TEST_F(ExistingTabGroupSubMenuModelTest, ShouldShowSubmenu) {
  AddTab(browser(), GURL("chrome://newtab"));
  AddTab(browser(), GURL("chrome://newtab"));

  TabStripModel* model = browser()->tab_strip_model();
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 0U);
  model->AddToNewGroup({0});
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 1U);
  ASSERT_TRUE(model->GetTabGroupForTab(0).has_value());
  ASSERT_FALSE(model->GetTabGroupForTab(1).has_value());
  ASSERT_EQ(model->count(), 2);

  EXPECT_FALSE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(model, 0));
  EXPECT_TRUE(ExistingTabGroupSubMenuModel::ShouldShowSubmenu(model, 1));
}

// Validate that the submenu has the correct items.
TEST_F(ExistingTabGroupSubMenuModelTest, BuildSubmenuItems) {
  AddTab(browser(), GURL("chrome://newtab"));
  AddTab(browser(), GURL("chrome://newtab"));
  AddTab(browser(), GURL("chrome://newtab"));

  TabStripModel* model = browser()->tab_strip_model();
  model->AddToNewGroup({0});
  model->AddToNewGroup({1});
  ASSERT_EQ(model->group_model()->ListTabGroups().size(), 2U);
  ASSERT_TRUE(model->GetTabGroupForTab(0).has_value());
  ASSERT_TRUE(model->GetTabGroupForTab(1).has_value());
  ASSERT_FALSE(model->GetTabGroupForTab(2).has_value());
  ASSERT_EQ(model->count(), 3);

  ExistingTabGroupSubMenuModel menu1(nullptr, model, 0);
  EXPECT_EQ(3, menu1.GetItemCount());

  ExistingTabGroupSubMenuModel menu2(nullptr, model, 1);
  EXPECT_EQ(3, menu2.GetItemCount());

  ExistingTabGroupSubMenuModel menu3(nullptr, model, 2);
  EXPECT_EQ(4, menu3.GetItemCount());
}

}  // namespace
