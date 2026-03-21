// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"

#include "base/run_loop.h"
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
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

constexpr char16_t kTestGroupName[] = u"my_group";

class ScopedAnimationDisabler {
 public:
  ScopedAnimationDisabler() {
    ProjectsPanelTabGroupsItemView::disable_animations_for_testing();
  }
  ~ScopedAnimationDisabler() {
    ProjectsPanelTabGroupsItemView::enable_animations_for_testing();
  }
};

tab_groups::SavedTabGroup& GetTestSavedTabGroup() {
  static base::NoDestructor<tab_groups::SavedTabGroup> group(
      kTestGroupName, tab_groups::TabGroupColorId::kGrey,
      std::vector<tab_groups::SavedTabGroupTab>(), std::nullopt);
  return *group;
}

}  // namespace

class ProjectsPanelTabGroupsItemViewTest : public ChromeViewsTestBase {};

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestDisplay) {
  auto tab_groups_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(GetTestSavedTabGroup());
  EXPECT_EQ(kTestGroupName, tab_groups_item->title_for_testing()->GetText());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestTabGroupClosed) {
  auto tab_groups_closed_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(GetTestSavedTabGroup());
  EXPECT_EQ(&kTabGroupClosedIcon,
            &tab_groups_closed_item->tab_group_vector_icon_for_testing());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestTabGroupOpen) {
  tab_groups::SavedTabGroup open_group(
      kTestGroupName, tab_groups::TabGroupColorId::kGrey, {}, std::nullopt,
      std::nullopt,
      tab_groups::TabGroupId::FromRawToken(
          base::Token{0x12345678, 0x9ABCDEF0}));
  auto tab_groups_open_item =
      std::make_unique<ProjectsPanelTabGroupsItemView>(open_group);
  EXPECT_EQ(&kTabGroupIcon,
            &tab_groups_open_item->tab_group_vector_icon_for_testing());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestChildrenNonSharedTabGroup) {
  auto non_collaboration_group_view =
      std::make_unique<ProjectsPanelTabGroupsItemView>(GetTestSavedTabGroup());
  // Title, icon, more button, and focus ring.
  EXPECT_EQ(4u, non_collaboration_group_view->children().size());
}

TEST_F(ProjectsPanelTabGroupsItemViewTest, TestChildrenSharedTabGroup) {
  tab_groups::SavedTabGroup collaboration_group(
      kTestGroupName, tab_groups::TabGroupColorId::kGrey, {});
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

class ProjectsPanelTabGroupsItemViewWidgetTest
    : public ProjectsPanelTabGroupsItemViewTest {
 public:
  void SetUp() override {
    ProjectsPanelTabGroupsItemViewTest::SetUp();
    widget_ = CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    widget_->Show();
  }

  void TearDown() override {
    widget_.reset();
    ProjectsPanelTabGroupsItemViewTest::TearDown();
  }

  views::Widget* widget() { return widget_.get(); }

  ProjectsPanelTabGroupsItemView* CreateItemView(
      ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback
          more_button_callback = base::DoNothing()) {
    return widget_->SetContentsView(
        std::make_unique<ProjectsPanelTabGroupsItemView>(
            GetTestSavedTabGroup(), base::DoNothing(),
            std::move(more_button_callback)));
  }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(ProjectsPanelTabGroupsItemViewWidgetTest, HoverStateChanges) {
  ScopedAnimationDisabler animation_disabler;

  auto* item_view = CreateItemView();

  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());

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

TEST_F(ProjectsPanelTabGroupsItemViewWidgetTest,
       RightClickTriggersContextMenu) {
  base::MockCallback<ProjectsPanelTabGroupsItemView::MoreButtonPressedCallback>
      more_button_callback;

  auto* item_view = CreateItemView(more_button_callback.Get());

  ui::test::EventGenerator generator(GetContext(), widget()->GetNativeWindow());
  generator.MoveMouseTo(item_view->GetBoundsInScreen().CenterPoint());

  // Left click should not trigger context menu callback.
  EXPECT_CALL(more_button_callback, Run(testing::_, testing::_)).Times(0);
  generator.ClickLeftButton();

  // Right click should trigger context menu callback.
  EXPECT_CALL(more_button_callback, Run(item_view->guid(), testing::_))
      .Times(1);
  generator.ClickRightButton();
}

TEST_F(ProjectsPanelTabGroupsItemViewWidgetTest,
       ReverseFocusTraversalFocusesMoreButtonFirst) {
  ScopedAnimationDisabler animation_disabler;

  // Create a view for us to layout like
  // [ Item view  ]
  // [ Lower view ]
  // so we can validate the behavior when performing reverse focus traversal.
  views::View* root_view =
      widget()->SetContentsView(std::make_unique<views::View>());
  root_view->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  auto* item_view = root_view->AddChildView(
      std::make_unique<ProjectsPanelTabGroupsItemView>(GetTestSavedTabGroup()));
  auto* lower_view = root_view->AddChildView(std::make_unique<views::View>());
  lower_view->SetFocusBehavior(views::View::FocusBehavior::ALWAYS);

  // Set explicit bounds to ensure lower_view is physically below item_view.
  item_view->SetPreferredSize(gfx::Size(100, 20));
  lower_view->SetPreferredSize(gfx::Size(100, 20));
  widget()->LayoutRootViewIfNecessary();

  // Initially, the more button is hidden.
  EXPECT_FALSE(item_view->more_button_for_testing()->GetVisible());

  // Focus lower_view first.
  widget()->GetFocusManager()->SetFocusedView(lower_view);
  EXPECT_TRUE(lower_view->HasFocus());

  // Simulate Shift+Tab from lower_view, which should focus the more button.
  widget()->GetFocusManager()->AdvanceFocus(true);

  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(item_view->more_button_for_testing()->HasFocus());
  EXPECT_TRUE(item_view->more_button_for_testing()->GetVisible());
}

TEST_F(ProjectsPanelTabGroupsItemViewWidgetTest, ShiftTabFromMoreButton) {
  ScopedAnimationDisabler animation_disabler;

  auto* item_view = CreateItemView();

  // Focus the item view (makes more button visible).
  widget()->GetFocusManager()->SetFocusedView(item_view);
  EXPECT_TRUE(item_view->more_button_for_testing()->GetVisible());

  // Focus the more button.
  widget()->GetFocusManager()->SetFocusedView(
      item_view->more_button_for_testing());
  EXPECT_TRUE(item_view->more_button_for_testing()->HasFocus());

  // Simulate Shift+Tab to traverse from more button to the parent item view.
  widget()->GetFocusManager()->AdvanceFocus(/*reverse=*/true);
  base::RunLoop().RunUntilIdle();

  // Focus should now be on the item view.
  EXPECT_TRUE(item_view->HasFocus());
  EXPECT_TRUE(item_view->more_button_for_testing()->GetVisible());
}
