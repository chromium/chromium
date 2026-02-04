// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sync/base/collaboration_id.h"
#include "ui/views/controls/image_view.h"
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

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestTabGroupClosed) {
  tab_groups::SavedTabGroup closed_group(std::u16string(u"my_group"),
                                         tab_groups::TabGroupColorId::kGrey, {},
                                         std::nullopt);
  auto tab_groups_closed_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(closed_group);
  EXPECT_EQ(&kTabGroupClosedIcon,
            &tab_groups_closed_item->tab_group_vector_icon_for_testing());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestTabGroupOpen) {
  tab_groups::SavedTabGroup open_group(
      std::u16string(u"my_group"), tab_groups::TabGroupColorId::kGrey, {},
      std::nullopt, std::nullopt,
      tab_groups::TabGroupId::FromRawToken(
          base::Token{0x12345678, 0x9ABCDEF0}));
  auto tab_groups_open_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(open_group);
  EXPECT_EQ(&kTabGroupIcon,
            &tab_groups_open_item->tab_group_vector_icon_for_testing());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestChildrenNonSharedTabGroup) {
  tab_groups::SavedTabGroup non_collaboration_group(
      std::u16string(u"my_group"), tab_groups::TabGroupColorId::kGrey, {});

  auto non_collaboration_group_view =
      std::make_unique<ProjectsPanelTabGroupsItemView>(non_collaboration_group);
  // Title, icon, more button, and focus ring.
  EXPECT_EQ(4u, non_collaboration_group_view->children().size());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestChildrenSharedTabGroup) {
  tab_groups::SavedTabGroup collaboration_group(
      std::u16string(u"my_group"), tab_groups::TabGroupColorId::kGrey, {});
  collaboration_group.SetCollaborationId(
      syncer::CollaborationId("collaboration_id"));

  auto collaboration_group_view =
      std::make_unique<ProjectsPanelTabGroupsItemView>(collaboration_group);
  // Title, icon, shared icon, more button, and focus ring.
  EXPECT_EQ(5u, collaboration_group_view->children().size());
  views::ImageView* collaboration_view =
      collaboration_group_view->shared_icon_for_testing();
  EXPECT_NE(nullptr, collaboration_view);
  EXPECT_TRUE(collaboration_view->GetImageModel().IsVectorIcon());
  EXPECT_EQ(&kPeopleGroupIcon,
            collaboration_view->GetImageModel().GetVectorIcon().vector_icon());
}
