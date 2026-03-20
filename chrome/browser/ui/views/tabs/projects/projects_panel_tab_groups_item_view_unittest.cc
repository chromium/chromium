// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "base/test/mock_callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/saved_tab_groups/public/saved_tab_group_tab.h"
#include "components/sync/base/collaboration_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/events/test/event_generator.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/widget/widget.h"

namespace {

class ScopedAnimationDisabler {
 public:
  ScopedAnimationDisabler() {
    ProjectsPanelTabGroupsItemView::disable_animations_for_testing();
  }
  ~ScopedAnimationDisabler() {
    ProjectsPanelTabGroupsItemView::enable_animations_for_testing();
  }
};

}  // namespace

class ProjectsPanelTabGroupsItemViewTest : public ChromeViewsTestBase {};

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

TEST_F(ProjectsPanelTabGroupsItemViewTest, HoverStateChanges) {
  ScopedAnimationDisabler animation_disabler;

  tab_groups::SavedTabGroup group(std::u16string(u"my_group"),
                                  tab_groups::TabGroupColorId::kGrey, {},
                                  std::nullopt);

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view = widget->SetContentsView(
      std::make_unique<ProjectsPanelTabGroupsItemView>(group));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());

  auto move_mouse_to = [&](bool inside_view) {
    if (inside_view) {
      generator.MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());
    } else {
      generator.MoveMouseTo(item_view->GetBoundsInScreen().bottom_right() +
                            gfx::Vector2d(10, 10));
    }
  };

  auto check_more_button_visible = [&](bool expected_visibility) {
    EXPECT_EQ(expected_visibility,
              item_view->more_button_for_testing()->GetVisible());
  };

  // Move mouse outside the view.
  move_mouse_to(false);
  check_more_button_visible(false);

  // Move mouse over the view.
  move_mouse_to(true);
  check_more_button_visible(true);

  // Move mouse outside the view again.
  move_mouse_to(false);
  check_more_button_visible(false);
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, RightClickTriggersContextMenu) {
  tab_groups::SavedTabGroup group(std::u16string(u"my_group"),
                                  tab_groups::TabGroupColorId::kGrey, {});

  base::MockCallback<ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback>
      more_button_callback;

  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  auto* item_view =
      widget->SetContentsView(std::make_unique<ProjectsPanelTabGroupsItemView>(
          group, /*pressed_callback=*/base::DoNothing(),
          more_button_callback.Get()));
  widget->Show();

  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  generator.MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());

  // Left click should not trigger context menu callback.
  EXPECT_CALL(more_button_callback, Run(testing::_, testing::_)).Times(0);
  generator.ClickLeftButton();

  // Right click should trigger context menu callback.
  EXPECT_CALL(more_button_callback, Run(group.saved_guid(), testing::_))
      .Times(1);
  generator.ClickRightButton();
}
