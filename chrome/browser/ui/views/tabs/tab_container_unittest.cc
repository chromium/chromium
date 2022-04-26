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
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

// Walks up the views hierarchy until it finds a tab view. It returns the
// found tab view, or nullptr if none is found.
views::View* FindTabView(views::View* view) {
  views::View* current = view;
  while (current && !views::IsViewClass<Tab>(current)) {
    current = current->parent();
  }
  return current;
}
}  // namespace

class TabContainerTest : public ChromeViewsTestBase {
 public:
  TabContainerTest() = default;
  TabContainerTest(const TabContainerTest&) = delete;
  TabContainerTest& operator=(const TabContainerTest&) = delete;
  ~TabContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_slot_controller_ =
        std::make_unique<FakeTabSlotController>(tab_strip_controller_.get());

    std::unique_ptr<TabContainer> tab_container =
        std::make_unique<TabContainer>(
            tab_strip_controller_.get(), nullptr /*hover_card_controller*/,
            nullptr /*drag_context*/, tab_slot_controller_.get(),
            nullptr /*scroll_contents_view*/);
    tab_container->SetAvailableWidthCallback(base::BindRepeating(
        [](TabContainerTest* test) { return test->tab_container_width_; },
        this));

    widget_ = CreateTestWidget();
    SetTabContainerWidth(1000);
    tab_container_ = widget_->SetContentsView(std::move(tab_container));

    tab_slot_controller_->set_tab_container(tab_container_);
  }

  void TearDown() override {
    tab_container_ = nullptr;
    widget_.reset();
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
        std::make_unique<Tab>(tab_slot_controller_.get()), model_index, pinned);
    tab_strip_controller_->AddTab(model_index, active == TabActive::kActive);

    if (active == TabActive::kActive)
      tab_slot_controller_->set_active_tab(tab);

    if (group)
      AddTabToGroup(model_index, group.value());

    return tab;
  }

  void MoveTab(int from_model_index, int to_model_index) {
    tab_strip_controller_->MoveTab(from_model_index, to_model_index);
    tab_container_->MoveTab(from_model_index, to_model_index);
  }

  // Removes the tab from the viewmodel, but leaves the Tab view itself around
  // so it can animate closed.
  void RemoveTab(int model_index) {
    bool was_active =
        tab_container_->GetTabAtModelIndex(model_index)->IsActive();
    tab_strip_controller_->RemoveTab(model_index);
    tab_container_->RemoveTab(model_index, was_active);
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

  std::vector<TabGroupViews*> ListGroupViews() const {
    std::vector<TabGroupViews*> result;
    for (auto const& group_view_pair : tab_container_->group_views())
      result.push_back(group_view_pair.second.get());
    return result;
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

  // Makes sure that all tabs have the correct AX indices.
  void VerifyTabIndices() {
    for (int i = 0; i < tab_container_->GetTabCount(); ++i) {
      ui::AXNodeData ax_node_data;
      tab_container_->GetTabAtModelIndex(i)
          ->GetViewAccessibility()
          .GetAccessibleNodeData(&ax_node_data);
      EXPECT_EQ(i + 1, ax_node_data.GetIntAttribute(
                           ax::mojom::IntAttribute::kPosInSet));
      EXPECT_EQ(
          tab_container_->GetTabCount(),
          ax_node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
    }
  }

  // Checks whether |tab| contains |point_in_tab_container_coords|, where the
  // point is in |tab_container_| coordinates.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tab_container_coords) {
    gfx::Point point_in_tab_coords(point_in_tab_container_coords);
    views::View::ConvertPointToTarget(tab_container_.get(), tab,
                                      &point_in_tab_coords);
    return tab->HitTestPoint(point_in_tab_coords);
  }

  void SetTabContainerWidth(int width) {
    tab_container_width_ = width;
    widget_->SetSize(
        gfx::Size(tab_container_width_, GetLayoutConstant(TAB_HEIGHT)));
  }

  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  raw_ptr<TabContainer> tab_container_;
  std::unique_ptr<views::Widget> widget_;

  int tab_container_width_ = 0;
};

TEST_F(TabContainerTest, ExitsClosingModeAtStandardWidth) {
  AddTab(0, absl::nullopt, TabActive::kActive);

  // Create just enough tabs so tabs are not full size.
  const int standard_width = TabStyleViews::GetStandardWidth();
  while (tab_container_->layout_helper()->active_tab_width() ==
         standard_width) {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
  }

  // The test closes two tabs, we need at least one left over after that.
  ASSERT_GE(tab_container_->GetTabCount(), 3);

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(absl::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  // Close the second-to-last tab; tab closing mode should remain active,
  // constraining tab widths to below full size.
  tab_container_->RemoveTab(tab_container_->GetTabCount() - 2, false);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->layout_helper()->active_tab_width(),
            standard_width);

  // Close the last tab; tab closing mode should allow tabs to resize to full
  // size.
  tab_container_->RemoveTab(tab_container_->GetTabCount() - 1, false);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_EQ(tab_container_->layout_helper()->active_tab_width(),
            standard_width);
}

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

TEST_F(TabContainerTest, AccessibilityData) {
  // When adding tabs, indices should be set.
  AddTab(0);
  AddTab(1, absl::nullopt, TabActive::kActive);
  VerifyTabIndices();

  AddTab(0);
  VerifyTabIndices();

  RemoveTab(1);
  VerifyTabIndices();

  MoveTab(1, 0);
  VerifyTabIndices();
}

TEST_F(TabContainerTest, GetEventHandlerForOverlappingArea) {
  Tab* left_tab = AddTab(0);
  Tab* active_tab = AddTab(1, absl::nullopt, TabActive::kActive);
  Tab* right_tab = AddTab(2);
  Tab* most_right_tab = AddTab(3);
  tab_container_->CompleteAnimationAndLayout();

  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));
  most_right_tab->SetBoundsRect(
      gfx::Rect(gfx::Point(450, 0), gfx::Size(200, 20)));

  // Test that active tabs gets events from area in which it overlaps with its
  // left neighbour.
  gfx::Point left_overlap(
      (active_tab->x() + left_tab->bounds().right() + 1) / 2,
      active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(active_tab, left_overlap));
  ASSERT_TRUE(IsPointInTab(left_tab, left_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_container_->GetEventHandlerForPoint(left_overlap)));

  // Test that active tabs gets events from area in which it overlaps with its
  // right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(
      active_tab,
      FindTabView(tab_container_->GetEventHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(
      right_tab,
      FindTabView(tab_container_->GetEventHandlerForPoint(unactive_overlap)));
}

TEST_F(TabContainerTest, GetTooltipHandler) {
  Tab* left_tab = AddTab(0);
  Tab* active_tab = AddTab(1, absl::nullopt, TabActive::kActive);
  Tab* right_tab = AddTab(2);
  Tab* most_right_tab = AddTab(3);
  tab_container_->CompleteAnimationAndLayout();

  // Verify that the active tab will be a tooltip handler for points that hit
  // it.
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));
  most_right_tab->SetBoundsRect(
      gfx::Rect(gfx::Point(450, 0), gfx::Size(200, 20)));

  // Test that active_tab handles tooltips from area in which it overlaps with
  // its left neighbour.
  gfx::Point left_overlap(
      (active_tab->x() + left_tab->bounds().right() + 1) / 2,
      active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(active_tab, left_overlap));
  ASSERT_TRUE(IsPointInTab(left_tab, left_overlap));

  EXPECT_EQ(
      active_tab,
      FindTabView(tab_container_->GetTooltipHandlerForPoint(left_overlap)));

  // Test that active_tab handles tooltips from area in which it overlaps with
  // its right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(
      active_tab,
      FindTabView(tab_container_->GetTooltipHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both tabs.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(
      right_tab,
      FindTabView(tab_container_->GetTooltipHandlerForPoint(unactive_overlap)));

  // Confirm that tab strip doe not return tooltip handler for points that
  // don't hit it.
  EXPECT_FALSE(tab_container_->GetTooltipHandlerForPoint(gfx::Point(-1, 2)));
}

TEST_F(TabContainerTest, GroupHeaderBasics) {
  AddTab(0);

  Tab* tab = tab_container_->GetTabAtModelIndex(0);
  const int first_slot_x = tab->x();

  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  tab_container_->CompleteAnimationAndLayout();

  std::vector<TabGroupViews*> views = ListGroupViews();
  EXPECT_EQ(1u, views.size());
  TabGroupHeader* header = views[0]->header();
  EXPECT_EQ(first_slot_x, header->x());
  EXPECT_GT(header->width(), 0);
  EXPECT_EQ(header->bounds().right() - TabStyle::GetTabOverlap(), tab->x());
  EXPECT_EQ(tab->height(), header->height());
}

TEST_F(TabContainerTest, GroupHeaderBetweenTabs) {
  AddTab(0);
  AddTab(1);
  tab_container_->CompleteAnimationAndLayout();

  const int second_slot_x = tab_container_->GetTabAtModelIndex(1)->x();

  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(1, group);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupViews()[0]->header();
  EXPECT_EQ(header->x(), second_slot_x);
}

TEST_F(TabContainerTest, GroupHeaderMovesRightWithTab) {
  for (int i = 0; i < 4; i++)
    AddTab(i);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(1, group);
  tab_container_->CompleteAnimationAndLayout();

  MoveTab(1, 2);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupViews()[0]->header();
  // Header is now left of tab 2.
  EXPECT_LT(tab_container_->GetTabAtModelIndex(1)->x(), header->x());
  EXPECT_LT(header->x(), tab_container_->GetTabAtModelIndex(2)->x());
}

TEST_F(TabContainerTest, GroupHeaderMovesLeftWithTab) {
  for (int i = 0; i < 4; i++)
    AddTab(i);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(2, group);
  tab_container_->CompleteAnimationAndLayout();

  MoveTab(2, 1);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupViews()[0]->header();
  // Header is now left of tab 1.
  EXPECT_LT(tab_container_->GetTabAtModelIndex(0)->x(), header->x());
  EXPECT_LT(header->x(), tab_container_->GetTabAtModelIndex(1)->x());
}

TEST_F(TabContainerTest, GroupHeaderDoesntMoveReorderingTabsInGroup) {
  for (int i = 0; i < 4; i++)
    AddTab(i);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(1, group);
  AddTabToGroup(2, group);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupViews()[0]->header();
  const int initial_header_x = header->x();
  Tab* tab1 = tab_container_->GetTabAtModelIndex(1);
  const int initial_tab_1_x = tab1->x();
  Tab* tab2 = tab_container_->GetTabAtModelIndex(2);
  const int initial_tab_2_x = tab2->x();

  MoveTab(1, 2);
  tab_container_->CompleteAnimationAndLayout();

  // Header has not moved.
  EXPECT_EQ(initial_header_x, header->x());
  EXPECT_EQ(initial_tab_1_x, tab2->x());
  EXPECT_EQ(initial_tab_2_x, tab1->x());
}

TEST_F(TabContainerTest, GroupHeaderMovesOnRegrouping) {
  for (int i = 0; i < 3; i++)
    AddTab(i);
  tab_groups::TabGroupId group0 = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group0);
  tab_groups::TabGroupId group1 = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(1, group1);
  AddTabToGroup(2, group1);
  tab_container_->CompleteAnimationAndLayout();

  std::vector<TabGroupViews*> views = ListGroupViews();
  auto views_it =
      std::find_if(views.begin(), views.end(), [&group1](TabGroupViews* view) {
        return view->header()->group() == group1;
      });
  ASSERT_TRUE(views_it != views.end());
  TabGroupViews* group1_views = *views_it;

  // Change groups in a way so that the header should swap with the tab, without
  // an explicit MoveTab call.
  MoveTabIntoGroup(1, group0);
  tab_container_->CompleteAnimationAndLayout();

  // Header is now right of tab 1.
  EXPECT_LT(tab_container_->GetTabAtModelIndex(1)->x(),
            group1_views->header()->x());
  EXPECT_LT(group1_views->header()->x(),
            tab_container_->GetTabAtModelIndex(2)->x());
}

TEST_F(TabContainerTest, UngroupedTabMovesLeftOfHeader) {
  for (int i = 0; i < 2; i++)
    AddTab(i);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  tab_container_->CompleteAnimationAndLayout();

  MoveTab(1, 0);
  tab_container_->CompleteAnimationAndLayout();

  // Header is right of tab 0.
  TabGroupHeader* header = ListGroupViews()[0]->header();
  EXPECT_LT(tab_container_->GetTabAtModelIndex(0)->x(), header->x());
  EXPECT_LT(header->x(), tab_container_->GetTabAtModelIndex(1)->x());
}

TEST_F(TabContainerTest, DeleteTabGroupViewsWhenEmpty) {
  AddTab(0);
  AddTab(1);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  AddTabToGroup(1, group);
  RemoveTabFromGroup(0);

  EXPECT_EQ(1u, ListGroupViews().size());
  RemoveTabFromGroup(1);
  EXPECT_EQ(0u, ListGroupViews().size());
}

TEST_F(TabContainerTest, GroupUnderlineBasics) {
  AddTab(0);
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  tab_container_->CompleteAnimationAndLayout();

  std::vector<TabGroupViews*> views = ListGroupViews();
  EXPECT_EQ(1u, views.size());
  // Update underline manually in the absence of a real Paint cycle.
  views[0]->UpdateBounds();

  const TabGroupUnderline* underline = views[0]->underline();
  EXPECT_EQ(underline->x(), TabGroupUnderline::GetStrokeInset());
  EXPECT_GT(underline->width(), 0);
  EXPECT_EQ(underline->bounds().right(),
            tab_container_->GetTabAtModelIndex(0)->bounds().right() -
                TabGroupUnderline::GetStrokeInset());
  EXPECT_EQ(underline->height(), TabGroupUnderline::kStrokeThickness);

  // Endpoints are different if the last grouped tab is active.
  AddTab(1, absl::nullopt, TabActive::kActive);
  MoveTabIntoGroup(1, group);
  tab_container_->CompleteAnimationAndLayout();
  views[0]->UpdateBounds();

  EXPECT_EQ(underline->x(), TabGroupUnderline::GetStrokeInset());
  EXPECT_EQ(underline->bounds().right(),
            tab_container_->GetTabAtModelIndex(1)->bounds().right() +
                TabGroupUnderline::kStrokeThickness);
}

TEST_F(TabContainerTest, GroupHighlightBasics) {
  AddTab(0);

  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  tab_container_->CompleteAnimationAndLayout();

  std::vector<TabGroupViews*> views = ListGroupViews();
  EXPECT_EQ(1u, views.size());

  // The highlight bounds match the group view bounds. Grab this manually
  // here, since there isn't a real paint cycle to trigger OnPaint().
  gfx::Rect bounds = views[0]->GetBounds();
  EXPECT_EQ(bounds.x(), 0);
  EXPECT_GT(bounds.width(), 0);
  EXPECT_EQ(bounds.right(),
            tab_container_->GetTabAtModelIndex(0)->bounds().right());
  EXPECT_EQ(bounds.height(),
            tab_container_->GetTabAtModelIndex(0)->bounds().height());
}
