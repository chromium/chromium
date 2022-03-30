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

    if (group) {
      tab->set_group(group);
      tab_strip_controller_->AddTabToGroup(model_index, group.value());

      auto& group_views = tab_container_->group_views();
      if (group_views.find(group.value()) == group_views.end()) {
        tab_container_->OnGroupCreated(group.value());
      } else {
        tab_container_->OnGroupMoved(group.value());
      }
    }

    return tab;
  }

  // Returns all TabSlotViews in the order that they have as ViewChildren of
  // TabContainer. This should match the actual order that they appear in
  // visually.
  views::View::Views GetTabSlotViewsInFocusOrder() {
    views::View::Views all_children = tab_container_->children();

    const int num_tab_slot_views = tab_container_->GetTabCount();

    return views::View::Views(all_children.begin(),
                              all_children.begin() + num_tab_slot_views);
  }

  // Returns all TabSlotViews in the order that they appear visually. This is
  // the expected order of the ViewChildren of TabContainer.
  views::View::Views GetTabSlotViewsInVisualOrder() {
    views::View::Views ordered_views;

    for (int i = 0; i < tab_container_->GetTabCount(); ++i) {
      Tab* tab = tab_container_->GetTabAtModelIndex(i);

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

  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(0), 0, 1);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(1), 1, 2);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(1), 1, 0);
  EXPECT_EQ(GetTabSlotViewsInFocusOrder(), GetTabSlotViewsInVisualOrder());
  tab_container_->MoveTab(tab_container_->GetTabAtModelIndex(0), 0, 2);
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
