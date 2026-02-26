// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_view.h"

#include <memory>
#include <vector>

#include "base/pickle.h"
#include "base/test/mock_callback.h"
#include "base/uuid.h"
#include "chrome/browser/ui/views/bookmarks/saved_tab_groups/saved_tab_group_drag_data.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_controller.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_no_tab_groups_view.h"
#include "chrome/browser/ui/views/tabs/projects/projects_panel_tab_groups_item_view.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/saved_tab_groups/public/saved_tab_group.h"
#include "components/saved_tab_groups/test_support/mock_tab_group_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/actions/actions.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace {

tab_groups::SavedTabGroup CreateGroup(const std::u16string& title) {
  return tab_groups::SavedTabGroup(
      title, tab_groups::TabGroupColorId::kGrey, /*urls=*/{},
      /*position=*/std::nullopt, /*saved_guid=*/base::Uuid::GenerateRandomV4(),
      /*local_group_id=*/std::nullopt, /*creator_cache_guid=*/std::nullopt,
      /*last_updater_cache_guid=*/std::nullopt,
      /*created_before_syncing_tab_groups=*/false);
}

void PopulateExchangeDataForGroup(ui::OSExchangeData& data,
                                  tab_groups::SavedTabGroup& group) {
  base::Pickle pickle;
  pickle.WriteString(group.saved_guid().AsLowercaseString());
  data.SetPickledData(tab_groups::SavedTabGroupDragData::GetFormatType(),
                      pickle);
}

MATCHER_P(IsForTabGroup, expected_tab_group, "") {
  auto* label =
      static_cast<ProjectsPanelTabGroupsItemView*>(arg)->title_for_testing();
  return expected_tab_group.title() == label->GetText();
}

}  // namespace

class ProjectsPanelTabGroupsViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    root_action_item_ = actions::ActionItem::Builder().Build();
    action_view_controller_ = std::make_unique<views::ActionViewController>();

    widget_ = CreateTestWidget(
        CreateParams(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW_FRAMELESS));
    tab_groups_view_ =
        widget_->SetContentsView(std::make_unique<ProjectsPanelTabGroupsView>(
            root_action_item_.get(), action_view_controller_.get(),
            /*tab_group_button_callback=*/base::DoNothing(),
            /*more_button_callback=*/base::DoNothing(),
            tab_group_moved_callback_.Get(),
            create_new_tab_group_callback_.Get()));
  }

  void TearDown() override {
    // Since the widget owns the tab groups view, we need to reset our pointer
    // to it before the widget is destructed.
    tab_groups_view_ = nullptr;
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  testing::NiceMock<tab_groups::MockTabGroupSyncService>
      mock_tab_group_sync_service_;
  base::MockCallback<base::RepeatingClosure> create_new_tab_group_callback_;
  std::unique_ptr<actions::ActionItem> root_action_item_;
  std::unique_ptr<views::ActionViewController> action_view_controller_;
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<ProjectsPanelTabGroupsView> tab_groups_view_;
  base::MockCallback<ProjectsPanelTabGroupsView::TabGroupMovedCallback>
      tab_group_moved_callback_;

  void VerifyNoTabGroupsView() {
    // Should contain the "Create new tab group" button and the no tab groups
    // message.
    EXPECT_EQ(2u, tab_groups_view_->children().size());
    // Index 0 is the "Create new tab group" button.
    views::Label* no_tabs_label = static_cast<views::Label*>(
        tab_groups_view_->no_tab_groups_view_for_testing()->children()[1]);
    EXPECT_EQ(
        u"Organize your tabs by grouping them together and label them with a "
        u"custom name and color",
        no_tabs_label->GetText());
  }
};

TEST_F(ProjectsPanelTabGroupsViewTest, EmptyTabGroups) {
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
}

TEST_F(ProjectsPanelTabGroupsViewTest, EnsureNoTabGroupsRemovedCorrectly) {
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
  tab_groups_view_->SetTabGroups(
      {CreateGroup(u"Group 1"), CreateGroup(u"Group 2")});
  EXPECT_EQ(nullptr, tab_groups_view_->no_tab_groups_view_for_testing());
  tab_groups_view_->SetTabGroups({});
  VerifyNoTabGroupsView();
}

TEST_F(ProjectsPanelTabGroupsViewTest, PopulatesTabGroups) {
  std::vector<tab_groups::SavedTabGroup> groups = {CreateGroup(u"Group 1"),
                                                   CreateGroup(u"Group 2")};

  tab_groups_view_->SetTabGroups(groups);

  // Groups + "Create new tab group" button.
  ASSERT_EQ(groups.size() + 1, tab_groups_view_->children().size());

  // Index 0 is the "Create new tab group" button.
  for (size_t i = 0; i < groups.size(); ++i) {
    EXPECT_THAT(tab_groups_view_->children()[i + 1], IsForTabGroup(groups[i]));
  }
}

TEST_F(ProjectsPanelTabGroupsViewTest, CreateNewTabGroupButtonPressed) {
  auto* create_match_button =
      tab_groups_view_->create_new_tab_group_button_for_testing();

  EXPECT_CALL(create_new_tab_group_callback_, Run());
  ui::MouseEvent event(ui::EventType::kMousePressed, gfx::Point(), gfx::Point(),
                       base::TimeTicks::Now(), 0, 0);
  views::test::ButtonTestApi(create_match_button).NotifyClick(event);
}

TEST_F(ProjectsPanelTabGroupsViewTest, CanDrop) {
  ui::OSExchangeData data;
  EXPECT_FALSE(tab_groups_view_->CanDrop(data));

  base::Pickle pickle;
  pickle.WriteString(base::Uuid::GenerateRandomV4().AsLowercaseString());
  data.SetPickledData(tab_groups::SavedTabGroupDragData::GetFormatType(),
                      pickle);
  EXPECT_TRUE(tab_groups_view_->CanDrop(data));
}

TEST_F(ProjectsPanelTabGroupsViewTest, WriteDragData) {
  std::vector<tab_groups::SavedTabGroup> groups = {CreateGroup(u"Group 1")};
  tab_groups_view_->SetTabGroups(groups);

  // Set the size of the view so the drag image is not empty.
  tab_groups_view_->SetSize(gfx::Size(100, 100));

  auto* item_view = tab_groups_view_->item_views_for_testing()[0];

  ui::OSExchangeData data;
  tab_groups_view_->WriteDragDataForView(item_view, gfx::Point(), &data);

  EXPECT_TRUE(item_view->is_dragging());
  EXPECT_TRUE(
      data.HasCustomFormat(tab_groups::SavedTabGroupDragData::GetFormatType()));

  std::optional<tab_groups::SavedTabGroupDragData> drag_data =
      tab_groups::SavedTabGroupDragData::ReadFromOSExchangeData(&data);
  ASSERT_TRUE(drag_data.has_value());
  EXPECT_EQ(groups[0].saved_guid(), drag_data->guid());
}

TEST_F(ProjectsPanelTabGroupsViewTest, DragExitedResetsState) {
  std::vector<tab_groups::SavedTabGroup> groups = {CreateGroup(u"Group 1"),
                                                   CreateGroup(u"Group 2")};
  tab_groups_view_->SetTabGroups(groups);
  tab_groups_view_->SetSize(gfx::Size(100, 200));

  ui::OSExchangeData data;
  PopulateExchangeDataForGroup(data, groups[0]);

  ui::DropTargetEvent event(data, gfx::PointF(50, 75), gfx::PointF(50, 75),
                            ui::DragDropTypes::DRAG_MOVE);

  tab_groups_view_->OnDragEntered(event);
  tab_groups_view_->OnDragUpdated(event);
  tab_groups_view_->OnDragExited();

  // Drop callback should be null after drag exit.
  auto drop_callback = tab_groups_view_->GetDropCallback(event);
  EXPECT_TRUE(drop_callback.is_null());
}

TEST_F(ProjectsPanelTabGroupsViewTest, DragAndDropReorder) {
  std::vector<tab_groups::SavedTabGroup> groups = {CreateGroup(u"Group 1"),
                                                   CreateGroup(u"Group 2"),
                                                   CreateGroup(u"Group 3")};

  tab_groups_view_->SetTabGroups(groups);
  tab_groups_view_->SetSize(gfx::Size(100, 300));

  // Give each item a fixed position, width and height so we can easily test
  // drag and drop.
  auto item_views = tab_groups_view_->item_views_for_testing();
  ASSERT_EQ(groups.size(), item_views.size());
  for (size_t i = 0; i < item_views.size(); ++i) {
    item_views[i]->SetBounds(0, i * 50, 100, 50);
  }

  // Start dragging the first item.
  item_views[0]->SetIsDragging(true);

  ui::OSExchangeData data;
  PopulateExchangeDataForGroup(data, groups[0]);

  // Drag over the middle of the second item.
  // Group 1 is at 0-50, Group 2 is at 50-100, Group 3 is at 100-150.
  // 75 is in the middle of Group 2.
  auto location = gfx::PointF(50, 75);
  ui::DropTargetEvent event(data, location, /*root_location=*/location,
                            ui::DragDropTypes::DRAG_MOVE);

  tab_groups_view_->OnDragEntered(event);
  tab_groups_view_->OnDragUpdated(event);

  // Dropping at 75 should move Group 1 to index 1 (after Group 2).
  // The indicator should be between Group 2 and Group 3 (at y=100).
  auto indicator_bounds = tab_groups_view_->GetDropIndicatorBoundsForTesting();
  EXPECT_TRUE(indicator_bounds.has_value());
  // Check that the indicator bounds are between Group 1 and Group 2, with some
  // potential error in case the indicator sizing changes.
  EXPECT_THAT(indicator_bounds->y() + indicator_bounds->height() / 2.0,
              testing::DoubleNear(100.0, 5.0));

  EXPECT_CALL(tab_group_moved_callback_, Run(groups[0].saved_guid(), 1));

  auto drop_callback = tab_groups_view_->GetDropCallback(event);
  ASSERT_FALSE(drop_callback.is_null());

  ui::mojom::DragOperation output_op = ui::mojom::DragOperation::kNone;
  std::move(drop_callback).Run(event, output_op, nullptr);
  EXPECT_EQ(ui::mojom::DragOperation::kMove, output_op);
}
