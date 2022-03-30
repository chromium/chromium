// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_container.h"
#include <memory>

#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"

class TabContainerTest : public ChromeViewsTestBase {
 public:
  TabContainerTest() = default;
  TabContainerTest(const TabContainerTest&) = delete;
  TabContainerTest& operator=(const TabContainerTest&) = delete;
  ~TabContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_slot_controller_ = std::make_unique<FakeTabSlotController>();

    tab_container_ = std::make_unique<TabContainer>(
        tab_strip_controller_.get(), nullptr /*hover_card_controller*/,
        nullptr /*drag_context*/, tab_slot_controller_.get(),
        nullptr /*scroll_contents_view*/);
    tab_container_->SetAvailableWidthCallback(
        base::BindRepeating([]() { return 500; }));
    tab_container_->SetBounds(0, 0, 500, GetLayoutConstant(TAB_HEIGHT));
    tab_container_->Layout();
  }

  void TearDown() override {
    tab_container_.reset();
    tab_slot_controller_.reset();
    tab_strip_controller_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  Tab* AddTab(int model_index,
              absl::optional<tab_groups::TabGroupId> group = absl::nullopt,
              TabActive active = TabActive::kInactive,
              TabPinned pinned = TabPinned::kUnpinned) {
    Tab* tab = tab_container_->AddTab(
        std::make_unique<Tab>(tab_slot_controller_.get()), model_index,
        TabPinned::kUnpinned);
    tab_strip_controller_->AddTab(model_index, active == TabActive::kActive);

    if (group)
      AddTabToGroup(model_index, group.value());

    return tab;
  }

  void MoveTab(int from_model_index, int to_model_index) {
    tab_strip_controller_->MoveTab(from_model_index, to_model_index);
    tab_container_->MoveTab(from_model_index, to_model_index);
  }

  void AddTabToGroup(int model_index, tab_groups::TabGroupId group) {
    tab_container_->GetTabAtModelIndex(model_index)->set_group(group);
    tab_strip_controller_->AddTabToGroup(model_index, group);

    auto& group_views = tab_container_->group_views();
    if (group_views.find(group) == group_views.end())
      tab_container_->OnGroupCreated(group);

    tab_container_->OnGroupMoved(group);
  }

  void RemoveTabFromGroup(int model_index) {
    Tab* tab = tab_container_->GetTabAtModelIndex(model_index);
    absl::optional<tab_groups::TabGroupId> old_group = tab->group();
    DCHECK(old_group);

    tab->set_group(absl::nullopt);
    tab_strip_controller_->RemoveTabFromGroup(model_index);

    bool group_is_empty = true;
    for (Tab* tab : tab_container_->layout_helper()->GetTabs()) {
      if (tab->group() == old_group)
        group_is_empty = false;
    }

    if (group_is_empty) {
      tab_container_->OnGroupClosed(old_group.value());
    } else {
      tab_container_->OnGroupMoved(old_group.value());
    }
  }

  void MoveTabIntoGroup(int index,
                        absl::optional<tab_groups::TabGroupId> new_group) {
    absl::optional<tab_groups::TabGroupId> old_group =
        tab_container_->GetTabAtModelIndex(index)->group();

    if (old_group.has_value())
      RemoveTabFromGroup(index);
    if (new_group.has_value())
      AddTabToGroup(index, new_group.value());
  }

  // Returns all TabSlotViews in the order that they have as ViewChildren of
  // TabContainer. This should match the actual order that they appear in
  // visually.
  views::View::Views GetTabSlotViewsInFocusOrder() {
    views::View::Views all_children = tab_container_->children();

    const int num_tab_slot_views =
        tab_container_->GetTabCount() + tab_container_->group_views().size();

    return views::View::Views(all_children.begin(),
                              all_children.begin() + num_tab_slot_views);
  }

  // Returns all TabSlotViews in the order that they appear visually. This is
  // the expected order of the ViewChildren of TabContainer.
  views::View::Views GetTabSlotViewsInVisualOrder() {
    views::View::Views ordered_views;

    absl::optional<tab_groups::TabGroupId> prev_group = absl::nullopt;

    for (int i = 0; i < tab_container_->GetTabCount(); ++i) {
      Tab* tab = tab_container_->GetTabAtModelIndex(i);

      // If the current Tab is the first one in a group, first add the
      // TabGroupHeader to the list of views.
      absl::optional<tab_groups::TabGroupId> curr_group = tab->group();
      if (curr_group.has_value() && curr_group != prev_group) {
        ordered_views.push_back(
            tab_container_->group_views()[curr_group.value()]->header());
      }
      prev_group = curr_group;

      ordered_views.push_back(tab);
    }

    return ordered_views;
  }

  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  std::unique_ptr<TabContainer> tab_container_;
};

// Verifies child view order matches model order.
TEST_F(TabContainerTest, TabViewOrder) {
  AddTab(0);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  AddTab(1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  AddTab(2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  MoveTab(0, 1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  MoveTab(1, 2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  MoveTab(1, 0);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  MoveTab(0, 2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
}

// Verifies child view order matches slot order with group headers.
TEST_F(TabContainerTest, TabViewOrderWithGroups) {
  AddTab(0);
  AddTab(1);
  AddTab(2);
  AddTab(3);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  tab_groups::TabGroupId group1 = tab_groups::TabGroupId::GenerateNew();
  tab_groups::TabGroupId group2 = tab_groups::TabGroupId::GenerateNew();

  // Add multiple tabs to a group and verify view order.
  AddTabToGroup(0, group1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  AddTabToGroup(1, group1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  // Move tabs within a group and verify view order.
  MoveTab(1, 0);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  // Add a single tab to a group and verify view order.
  AddTabToGroup(2, group2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());

  // Move and add tabs near a group and verify view order.
  AddTab(2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  MoveTab(4, 3);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
}

namespace {
ui::DropTargetEvent MakeEventForDragLocation(const gfx::Point& p) {
  return ui::DropTargetEvent({}, gfx::PointF(p), {},
                             ui::DragDropTypes::DRAG_LINK);
}
}  // namespace

TEST_F(TabContainerTest, DropIndexForDragLocationIsCorrect) {
  auto group = tab_groups::TabGroupId::GenerateNew();
  Tab* tab1 = AddTab(0, absl::nullopt, TabActive::kActive);
  Tab* tab2 = AddTab(1, group);
  Tab* tab3 = AddTab(2, group);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* const group_header =
      tab_container_->group_views()[group]->header();

  using DropIndex = BrowserRootView::DropIndex;

  // Check dragging near the edge of each tab.
  EXPECT_EQ((DropIndex{0, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab1->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{1, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab1->bounds().right_center() + gfx::Vector2d(-1, 0))));
  EXPECT_EQ((DropIndex{1, true, true}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab2->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{2, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab2->bounds().right_center() + gfx::Vector2d(-1, 0))));
  EXPECT_EQ((DropIndex{2, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab3->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{3, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab3->bounds().right_center() + gfx::Vector2d(-1, 0))));

  // Check dragging in the center of each tab.
  EXPECT_EQ((DropIndex{0, false, false}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab1->bounds().CenterPoint())));
  EXPECT_EQ((DropIndex{1, false, false}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab2->bounds().CenterPoint())));
  EXPECT_EQ((DropIndex{2, false, false}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab3->bounds().CenterPoint())));

  // Check dragging over group header.
  EXPECT_EQ((DropIndex{1, true, false}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                group_header->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{1, true, true}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                group_header->bounds().right_center() + gfx::Vector2d(-1, 0))));
}
