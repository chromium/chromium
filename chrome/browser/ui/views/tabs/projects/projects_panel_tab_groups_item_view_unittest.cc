// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/views_test_base.h"

class ProjectsPanelTabGroupsItemViewTest : public views::ViewsTestBase {};

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestDisplay) {
  tab_groups::SavedTabGroup group(std::u16string(u"my_group"),
                                  tab_groups::TabGroupColorId::kGrey, {},
                                  std::nullopt);
  auto tab_groups_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(group);
  EXPECT_EQ(u"my_group", tab_groups_item->title_for_testing()->GetText());
}
