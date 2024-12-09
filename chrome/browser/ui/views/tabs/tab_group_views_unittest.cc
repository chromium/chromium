// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_group_views.h"

#include <memory>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/widget/widget.h"

class TabGroupViewsTest : public ChromeViewsTestBase {
 public:
  TabGroupViewsTest() {}
  TabGroupViewsTest(const TabGroupViewsTest&) = delete;
  TabGroupViewsTest& operator=(const TabGroupViewsTest&) = delete;
  ~TabGroupViewsTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    tab_container_ = widget_->SetContentsView(std::make_unique<views::View>());
    tab_container_->SetBounds(0, 0, 1000, 100);
    drag_context_ =
        tab_container_->AddChildView(std::make_unique<views::View>());
    drag_context_->SetBounds(0, 0, 1000, 100);

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_slot_controller_ =
        std::make_unique<FakeTabSlotController>(tab_strip_controller_.get());
    group_views_ = std::make_unique<TabGroupViews>(
        tab_container_.get(), drag_context_.get(),
        *(tab_slot_controller_.get()), id_);
  }

  void TearDown() override {
    drag_context_ = nullptr;
    tab_container_ = nullptr;

    widget_->Close();

    group_views_.reset();
    widget_.reset();
    tab_slot_controller_.reset();
    tab_strip_controller_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::Widget> widget_;
  raw_ptr<views::View> tab_container_;
  raw_ptr<views::View> drag_context_;
  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  tab_groups::TabGroupId id_ = tab_groups::TabGroupId::GenerateNew();
  std::unique_ptr<TabGroupViews> group_views_;
};

TEST_F(TabGroupViewsTest, GroupViewsCreated) {
  EXPECT_NE(nullptr, group_views_->header());
  EXPECT_NE(nullptr, group_views_->underline());
  EXPECT_NE(nullptr, group_views_->drag_underline());
  EXPECT_NE(nullptr, group_views_->highlight());

  EXPECT_EQ(tab_container_.get(), group_views_->header()->parent());
  EXPECT_EQ(tab_container_.get(), group_views_->underline()->parent());
  EXPECT_EQ(drag_context_.get(), group_views_->drag_underline()->parent());
  EXPECT_EQ(drag_context_.get(), group_views_->highlight()->parent());
}

TEST_F(TabGroupViewsTest, HeaderInitialAccessibilityProperties) {
  TabGroupHeader* header = group_views_->header();
  ui::AXNodeData node_data;

  header->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kEditable));
  EXPECT_EQ(node_data.role, ax::mojom::Role::kTabList);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
}

// Underline should actually underline the group.
TEST_F(TabGroupViewsTest, UnderlineBoundsNoDrag) {
  TabGroupHeader* header = group_views_->header();
  Tab* tab_1 = tab_container_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_1->set_group(id_);
  Tab* tab_2 = tab_container_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_2->set_group(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);

  group_views_->UpdateBounds();

  EXPECT_TRUE(group_views_->underline()->GetVisible());
  const gfx::Rect underline_bounds = group_views_->underline()->bounds();

  // Underline should begin within the header.
  EXPECT_GT(underline_bounds.x(), header->bounds().x());
  EXPECT_LT(underline_bounds.x(), header->bounds().right());

  // Underline should end within the last tab.
  EXPECT_GT(underline_bounds.right(), tab_2->bounds().x());
  EXPECT_LT(underline_bounds.right(), tab_2->bounds().right());

  EXPECT_FALSE(group_views_->drag_underline()->GetVisible());
}

// Underline should not be visible with chrome refresh flag when only header is
// visible.
TEST_F(TabGroupViewsTest, UnderlineBoundsWhenTabsAreNotVisible) {
  TabGroupHeader* header = group_views_->header();
  Tab* tab_1 = tab_container_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_1->set_group(id_);
  Tab* tab_2 = tab_container_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_2->set_group(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);

  tab_1->SetVisible(false);
  tab_2->SetVisible(false);
  group_views_->UpdateBounds();

  EXPECT_FALSE(group_views_->underline()->GetVisible());
  EXPECT_GT(group_views_->underline()->width(), 0);
}

// Drag_underline should underline the group when the group is being dragged,
// and the highlight should highlight it.
TEST_F(TabGroupViewsTest, UnderlineBoundsHeaderDrag) {
  TabGroupHeader* header = group_views_->header();
  drag_context_->AddChildView(header);
  Tab* tab_1 = drag_context_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_1->set_group(id_);
  Tab* tab_2 = drag_context_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  tab_2->set_group(id_);

  header->SetBounds(0, 0, 100, 0);
  tab_1->SetBounds(50, 0, 100, 0);
  tab_2->SetBounds(100, 0, 100, 0);
  group_views_->highlight()->SetVisible(true);

  group_views_->UpdateBounds();

  // The underline and the drag underline should match exactly.
  EXPECT_TRUE(group_views_->underline()->GetVisible());
  EXPECT_EQ(group_views_->underline()->bounds(),
            group_views_->drag_underline()->bounds());

  EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
  const gfx::Rect drag_underline_bounds =
      group_views_->drag_underline()->bounds();

  // Drag underline should begin within the header.
  EXPECT_GT(drag_underline_bounds.x(), header->bounds().x());
  EXPECT_LT(drag_underline_bounds.x(), header->bounds().right());

  // Drag underline should end within the last tab.
  EXPECT_GT(drag_underline_bounds.right(), tab_2->bounds().x());
  EXPECT_LT(drag_underline_bounds.right(), tab_2->bounds().right());

  // Highlight should span the dragged views exactly.
  EXPECT_EQ(group_views_->highlight()->bounds().x(), header->bounds().x());
  EXPECT_EQ(group_views_->highlight()->bounds().right(),
            tab_2->bounds().right());
}

// Underline and drag_underline should align with one another correctly when
// dragging a tab within a group.
TEST_F(TabGroupViewsTest, UnderlineBoundsDragTabInGroup) {
  TabGroupHeader* header = group_views_->header();
  Tab* other_tab = tab_container_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  other_tab->set_group(id_);
  Tab* dragged_tab = drag_context_->AddChildView(
      std::make_unique<Tab>(tab_slot_controller_.get()));
  dragged_tab->set_group(id_);

  header->SetBounds(0, 0, 100, 0);
  other_tab->SetBounds(50, 0, 100, 0);
  dragged_tab->SetBounds(100, 0, 100, 0);
  group_views_->highlight()->SetVisible(true);

  /////////////// Case 1: `dragged_tab` is right of `other_tab`. ///////////////
  {
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab.
    EXPECT_GT(underline_bounds.right(), dragged_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), dragged_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline should begin right at the beginning of the dragged tab.
    EXPECT_EQ(drag_underline_bounds.x(), dragged_tab->x());

    // Drag underline end should match the other underline's.
    EXPECT_EQ(drag_underline_bounds.right(), underline_bounds.right());
  }

  //// Case 2: `dragged_tab` is a bit left of and overlapping `other_tab`. /////
  {
    dragged_tab->SetBounds(45, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline should begin right at the beginning of the dragged tab.
    EXPECT_EQ(drag_underline_bounds.x(), dragged_tab->x());

    // Drag underline end should match the other underline's. Note that this is
    // different from the case above because the drag underline must be extended
    // from its natural end to meet the other underline.
    EXPECT_EQ(drag_underline_bounds.right(), underline_bounds.right());
  }

  ///// Case 3: `dragged_tab` is a bit right of and overlapping `header`. //////
  {
    dragged_tab->SetBounds(5, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within the header.
    EXPECT_GT(underline_bounds.x(), header->bounds().x());
    EXPECT_LT(underline_bounds.x(), header->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline begin should match the other underline's. Unlike case 4,
    // the drag underline must be extended to match the underline.
    EXPECT_EQ(drag_underline_bounds.x(), underline_bounds.x());

    // Drag underline end should match the dragged tab's end.
    EXPECT_EQ(drag_underline_bounds.right(), dragged_tab->bounds().right());
  }

  ///////////////// Case 4: `dragged_tab` is left of `header`. /////////////////
  {
    dragged_tab->SetBounds(-50, 0, 100, 0);
    group_views_->UpdateBounds();

    EXPECT_TRUE(group_views_->underline()->GetVisible());
    const gfx::Rect underline_bounds = group_views_->underline()->bounds();

    // Underline should begin within `dragged_tab`, now that it's leftmost.
    EXPECT_GT(underline_bounds.x(), dragged_tab->bounds().x());
    EXPECT_LT(underline_bounds.x(), dragged_tab->bounds().right());

    // Underline should end within the last tab, now `other_tab`.
    EXPECT_GT(underline_bounds.right(), other_tab->bounds().x());
    EXPECT_LT(underline_bounds.right(), other_tab->bounds().right());

    EXPECT_TRUE(group_views_->drag_underline()->GetVisible());
    const gfx::Rect drag_underline_bounds =
        group_views_->drag_underline()->bounds();

    // Drag underline begin should match the other underline's. Unlike case 3,
    // this is the drag underline's natural start point.
    EXPECT_EQ(drag_underline_bounds.x(), underline_bounds.x());

    // Drag underline end should match the dragged tab's end.
    EXPECT_EQ(drag_underline_bounds.right(), dragged_tab->bounds().right());
  }
}
