// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/memory/raw_ref.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_close_button.h"
#include "chrome/browser/ui/views/tabs/tab_container_impl.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout_helper.h"
#include "chrome/browser/ui/views/tabs/tab_strip_types.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/animation/animation_test_api.h"
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

class FakeTabDragContext : public TabDragContextBase {
  METADATA_HEADER(FakeTabDragContext, TabDragContextBase)

 public:
  FakeTabDragContext() = default;
  ~FakeTabDragContext() override = default;

  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             const gfx::Rect& target_bounds) override {}
  bool IsDragSessionActive() const override { return drag_session_active_; }
  bool IsAnimatingDragEnd() const override { return false; }
  void CompleteEndDragAnimations() override {}
  int GetTabDragAreaWidth() const override { return width(); }

  void set_drag_session_active(bool active) { drag_session_active_ = active; }

 private:
  bool drag_session_active_ = false;
};

BEGIN_METADATA(FakeTabDragContext)
END_METADATA

class FakeTabContainerController final : public TabContainerController {
 public:
  explicit FakeTabContainerController(TabStripController& tab_strip_controller)
      : tab_strip_controller_(tab_strip_controller) {}
  ~FakeTabContainerController() override = default;

  void set_tab_container(TabContainer* tab_container) {
    tab_container_ = tab_container;
  }

  void set_is_animating_outside_container(bool is_animating_outside_container) {
    is_animating_outside_container_ = is_animating_outside_container;
  }

  bool IsValidModelIndex(int index) const override {
    return tab_strip_controller_->IsValidIndex(index);
  }

  std::optional<int> GetActiveIndex() const override {
    return tab_strip_controller_->GetActiveIndex();
  }

  int NumPinnedTabsInModel() const override {
    for (size_t i = 0;
         i < static_cast<size_t>(tab_strip_controller_->GetCount()); ++i) {
      if (!tab_strip_controller_->IsTabPinned(static_cast<int>(i)))
        return static_cast<int>(i);
    }

    // All tabs are pinned.
    return tab_strip_controller_->GetCount();
  }

  void OnDropIndexUpdate(std::optional<int> index, bool drop_before) override {
    tab_strip_controller_->OnDropIndexUpdate(index, drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    return tab_strip_controller_->IsGroupCollapsed(group);
  }

  std::optional<int> GetFirstTabInGroup(
      const tab_groups::TabGroupId& group) const override {
    return tab_strip_controller_->GetFirstTabInGroup(group);
  }

  gfx::Range ListTabsInGroup(
      const tab_groups::TabGroupId& group) const override {
    return tab_strip_controller_->ListTabsInGroup(group);
  }

  bool CanExtendDragHandle() const override {
    return !tab_strip_controller_->IsFrameCondensed() &&
           !tab_strip_controller_->EverHasVisibleBackgroundTabShapes();
  }

  const views::View* GetTabClosingModeMouseWatcherHostView() const override {
    return nullptr;
  }

  bool IsAnimatingInTabStrip() const override {
    return tab_container_->IsAnimating() || is_animating_outside_container_;
  }

  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             gfx::Rect target_bounds) override {}

 private:
  const raw_ref<TabStripController> tab_strip_controller_;
  raw_ptr<const TabContainer, DanglingUntriaged> tab_container_;

  // Set this to true to emulate a tab being animated outside `tab_container_`.
  bool is_animating_outside_container_ = false;
};
}  // namespace

class TabContainerTest : public ChromeViewsTestBase {
 public:
  TabContainerTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}
  TabContainerTest(const TabContainerTest&) = delete;
  TabContainerTest& operator=(const TabContainerTest&) = delete;
  ~TabContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_container_controller_ = std::make_unique<FakeTabContainerController>(
        *(tab_strip_controller_.get()));
    tab_slot_controller_ =
        std::make_unique<FakeTabSlotController>(tab_strip_controller_.get());

    std::unique_ptr<FakeTabDragContext> drag_context =
        std::make_unique<FakeTabDragContext>();
    std::unique_ptr<TabContainer> tab_container =
        std::make_unique<TabContainerImpl>(
            *(tab_container_controller_.get()),
            nullptr /*hover_card_controller*/, drag_context.get(),
            *(tab_slot_controller_.get()), nullptr /*scroll_contents_view*/);
    tab_container->SetAvailableWidthCallback(base::BindRepeating(
        [](TabContainerTest* test) { return test->tab_container_width_; },
        this));

    tab_container_controller_->set_tab_container(tab_container.get());
    tab_slot_controller_->set_tab_container(tab_container.get());

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    tab_container_ =
        widget_->GetRootView()->AddChildView(std::move(tab_container));
    drag_context_ =
        widget_->GetRootView()->AddChildView(std::move(drag_context));
    SetTabContainerWidth(1000);
  }

  void TearDown() override {
    drag_context_ = nullptr;
    tab_container_ = nullptr;
    widget_.reset();
    tab_slot_controller_.reset();
    tab_container_controller_.reset();
    tab_strip_controller_.reset();

    ChromeViewsTestBase::TearDown();
  }

 protected:
  Tab* AddTab(int model_index,
              std::optional<tab_groups::TabGroupId> group = std::nullopt,
              TabActive active = TabActive::kInactive,
              TabPinned pinned = TabPinned::kUnpinned) {
    Tab* tab = tab_container_->AddTab(
        std::make_unique<Tab>(tab_slot_controller_.get()), model_index, pinned);
    tab_strip_controller_->AddTab(model_index, active, pinned);

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

    const auto& group_views = tab_container_->get_group_views_for_testing();
    if (group_views.find(group) == group_views.end())
      tab_container_->OnGroupCreated(group);

    tab_container_->OnGroupMoved(group);
  }

  void RemoveTabFromGroup(int model_index) {
    Tab* tab = tab_container_->GetTabAtModelIndex(model_index);
    std::optional<tab_groups::TabGroupId> old_group = tab->group();
    DCHECK(old_group);

    tab->set_group(std::nullopt);
    tab_strip_controller_->RemoveTabFromGroup(model_index);

    bool group_is_empty = true;
    for (int i = 0; i < tab_container_->GetTabCount(); i++) {
      if (tab_container_->GetTabAtModelIndex(i)->group() == old_group)
        group_is_empty = false;
    }

    if (group_is_empty) {
      tab_container_->OnGroupClosed(old_group.value());
    } else {
      tab_container_->OnGroupMoved(old_group.value());
    }
  }

  void MoveTabIntoGroup(int index,
                        std::optional<tab_groups::TabGroupId> new_group) {
    std::optional<tab_groups::TabGroupId> old_group =
        tab_container_->GetTabAtModelIndex(index)->group();

    if (old_group.has_value())
      RemoveTabFromGroup(index);
    if (new_group.has_value())
      AddTabToGroup(index, new_group.value());
  }

  std::vector<TabGroupViews*> ListGroupViews() const {
    std::vector<TabGroupViews*> result;
    for (auto const& group_view_pair :
         tab_container_->get_group_views_for_testing())
      result.push_back(group_view_pair.second.get());
    return result;
  }

  // Returns all TabSlotViews in the order that they have as ViewChildren of
  // TabContainer. This should match the actual order that they appear in
  // visually.
  views::View::Views GetTabSlotViewsInFocusOrder() {
    views::View::Views all_children = tab_container_->children();

    const int num_tab_slot_views =
        tab_container_->GetTabCount() +
        tab_container_->get_group_views_for_testing().size();

    return views::View::Views(all_children.begin(),
                              all_children.begin() + num_tab_slot_views);
  }

  // Returns all TabSlotViews in the order that they appear visually. This is
  // the expected order of the ViewChildren of TabContainer.
  views::View::Views GetTabSlotViewsInVisualOrder() {
    views::View::Views ordered_views;

    std::optional<tab_groups::TabGroupId> prev_group = std::nullopt;

    for (int i = 0; i < tab_container_->GetTabCount(); ++i) {
      Tab* tab = tab_container_->GetTabAtModelIndex(i);

      // If the current Tab is the first one in a group, first add the
      // TabGroupHeader to the list of views.
      std::optional<tab_groups::TabGroupId> curr_group = tab->group();
      if (curr_group.has_value() && curr_group != prev_group) {
        ordered_views.push_back(
            tab_container_->GetGroupViews(curr_group.value())->header());
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
    gfx::Size size(tab_container_width_, GetLayoutConstant(TAB_STRIP_HEIGHT));
    widget_->SetSize(size);
    drag_context_->SetSize(size);
    tab_container_->SetSize(size);
  }

  // An abridged version of the above that avoids calls to TabContainer::Layout
  // from Widget::SetSize.
  void SetTabContainerWidthSingleLayout(int width) {
    tab_container_width_ = width;
    gfx::Size size(tab_container_width_, GetLayoutConstant(TAB_STRIP_HEIGHT));
    tab_container_->SetSize(size);
  }

  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabContainerController> tab_container_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  raw_ptr<FakeTabDragContext> drag_context_;
  raw_ptr<TabContainer> tab_container_;
  std::unique_ptr<views::Widget> widget_;

  // Used to force animation on, so that any tests that rely on animation pass
  // on machines where animation is turned off.
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;

  int tab_container_width_ = 0;
};

TEST_F(TabContainerTest, ExitsClosingModeAtStandardWidth) {
  AddTab(0, std::nullopt, TabActive::kActive);

  // Create just enough tabs so tabs are not full size.
  const int standard_width = TabStyle::Get()->GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
  }

  // The test closes two tabs, we need at least one left over after that.
  ASSERT_GE(tab_container_->GetTabCount(), 3);

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(std::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  // Close the second-to-last tab; tab closing mode should remain active,
  // constraining tab widths to below full size.
  RemoveTab(tab_container_->GetTabCount() - 2);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->GetActiveTabWidth(), standard_width);

  // Close the last tab; tab closing mode should allow tabs to resize to full
  // size.
  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_EQ(tab_container_->GetActiveTabWidth(), standard_width);
}

TEST_F(TabContainerTest, StaysInClosingModeBelowStandardWidth) {
  AddTab(0, std::nullopt, TabActive::kActive);

  // Create just enough tabs so tabs are not full size.
  const int standard_width = TabStyle::Get()->GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
  }

  // Add one more so removing a tab leaves things below full size.
  AddTab(0);
  tab_container_->CompleteAnimationAndLayout();

  // The test closes two tabs, we need at least one left over after that.
  ASSERT_GE(tab_container_->GetTabCount(), 3);

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(std::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  // Close the second-to-last tab; tab closing mode should remain active,
  // constraining tab widths to below full size.
  RemoveTab(tab_container_->GetTabCount() - 2);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->GetActiveTabWidth(), standard_width);

  // Close the last tab; tab closing mode should remain active, as there isn't
  // enough room for tabs to be standard width.
  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_LT(tab_container_->GetActiveTabWidth(), standard_width);
}

TEST_F(TabContainerTest, ClosingModeAffectsMinWidth) {
  AddTab(0, std::nullopt, TabActive::kActive);

  // Create just enough tabs so tabs are not full size.
  const int standard_width = TabStyle::Get()->GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
  }

  // Add one more so removing a tab leaves things below full size.
  AddTab(0);
  tab_container_->CompleteAnimationAndLayout();

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(std::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();

  // In closing mode, minimum width and preferred width should be equal.
  EXPECT_EQ(tab_container_->GetMinimumSize().width(),
            tab_container_->GetIdealBounds(tab_container_->GetTabCount() - 1)
                .right());
}

// After removing a tab followed by removing a tab in a tabgroup
// Should bring the subsequent tab to its place as expected in
// tab closing mode.
TEST_F(TabContainerTest, RemoveTabInGroupWithTabClosingMode) {
  AddTab(0, std::nullopt, TabActive::kActive);

  // Create enough tabs so tabs are not full size.
  const int standard_width = TabStyle::Get()->GetStandardWidth();

  // Set a tab_counter to avoid infinite loop
  int tab_counter = 0;
  while ((tab_counter < 100) &&
         (tab_container_->GetActiveTabWidth() == standard_width ||
          tab_container_->GetTabCount() < 10)) {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
    tab_counter += 1;
  }

  // add the first two tabs to a group
  tab_groups::TabGroupId group1 = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(1, group1);
  AddTabToGroup(2, group1);
  AddTabToGroup(3, group1);

  // Remove the second from last tab
  tab_container_->EnterTabClosingMode(std::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  RemoveTab(tab_container_->GetTabCount() - 2);
  tab_container_->CompleteAnimationAndLayout();

  // Get the group tab's close button center point
  Tab* tab = tab_container_->GetTabAtModelIndex(1);
  TabCloseButton* tab_close_button = tab->close_button();
  gfx::Point tab_center = tab_close_button->GetBoundsInScreen().CenterPoint();

  // Remove the tab
  tab_container_->EnterTabClosingMode(std::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);
  tab_container_->OnGroupContentsChanged(group1);
  RemoveTab(1);
  tab_container_->CompleteAnimationAndLayout();

  // Check if the next tab moves to its place
  Tab* tab_next = tab_container_->GetTabAtModelIndex(1);
  raw_ptr<TabCloseButton> tab_next_close_button = tab_next->close_button();
  EXPECT_TRUE(tab_next_close_button->GetBoundsInScreen().Contains(tab_center));
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
  Tab* tab1 = AddTab(0, std::nullopt, TabActive::kActive);
  Tab* tab2 = AddTab(1, group);
  Tab* tab3 = AddTab(2, group);
  tab_container_->CompleteAnimationAndLayout();

  TabGroupHeader* const group_header =
      tab_container_->GetGroupViews(group)->header();

  using DropIndex = BrowserRootView::DropIndex;
  using BrowserRootView::DropIndex::GroupInclusion::kDontIncludeInGroup;
  using BrowserRootView::DropIndex::GroupInclusion::kIncludeInGroup;
  using BrowserRootView::DropIndex::RelativeToIndex::kInsertBeforeIndex;
  using BrowserRootView::DropIndex::RelativeToIndex::kReplaceIndex;

  // Check dragging near the edge of each tab.
  EXPECT_EQ((DropIndex{.index = 0,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab1->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{.index = 1,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab1->bounds().right_center() + gfx::Vector2d(-1, 0))));
  EXPECT_EQ((DropIndex{.index = 1,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab2->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{.index = 2,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab2->bounds().right_center() + gfx::Vector2d(-1, 0))));
  EXPECT_EQ((DropIndex{.index = 2,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab3->bounds().left_center() + gfx::Vector2d(1, 0))));
  EXPECT_EQ((DropIndex{.index = 3,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                tab3->bounds().right_center() + gfx::Vector2d(-1, 0))));

  // Check dragging in the center of each tab.
  EXPECT_EQ((DropIndex{.index = 0,
                       .relative_to_index = kReplaceIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab1->bounds().CenterPoint())));
  EXPECT_EQ((DropIndex{.index = 1,
                       .relative_to_index = kReplaceIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab2->bounds().CenterPoint())));
  EXPECT_EQ((DropIndex{.index = 2,
                       .relative_to_index = kReplaceIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(
                MakeEventForDragLocation(tab3->bounds().CenterPoint())));

  // Check dragging over group header.
  // The left half of the header should drop outside the group.
  EXPECT_EQ((DropIndex{.index = 1,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kDontIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                group_header->bounds().CenterPoint() + gfx::Vector2d(-1, 0))));
  // The right half of the header should drop inside the group.
  EXPECT_EQ((DropIndex{.index = 1,
                       .relative_to_index = kInsertBeforeIndex,
                       .group_inclusion = kIncludeInGroup}),
            tab_container_->GetDropIndex(MakeEventForDragLocation(
                group_header->bounds().CenterPoint() + gfx::Vector2d(1, 0))));
}

TEST_F(TabContainerTest, AccessibilityData) {
  // When adding tabs, indices should be set.
  AddTab(0);
  AddTab(1, std::nullopt, TabActive::kActive);
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
  Tab* active_tab = AddTab(1, std::nullopt, TabActive::kActive);
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
  Tab* active_tab = AddTab(1, std::nullopt, TabActive::kActive);
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
  EXPECT_EQ(header->bounds().right() - TabStyle::Get()->GetTabOverlap(),
            tab->x());
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
  auto views_it = base::ranges::find(views, group1, [](TabGroupViews* view) {
    return view->header()->group();
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
  AddTab(1, std::nullopt, TabActive::kActive);
  MoveTabIntoGroup(1, group);
  tab_container_->CompleteAnimationAndLayout();
  views[0]->UpdateBounds();

  EXPECT_EQ(underline->x(), TabGroupUnderline::GetStrokeInset());
  EXPECT_EQ(underline->bounds().right(),
            tab_container_->GetTabAtModelIndex(1)->bounds().right() +
                TabGroupUnderline::kStrokeThickness);
}

TEST_F(TabContainerTest, UnderlineBoundsTabVisibilityChange) {
  // Validates that group underlines are updated correctly in a single Layout
  // call when the visibility of tabs in the group change. See crbug.com/1356177

  // This test is only valid with scrolling off, since it pertains to tab
  // visibility stuff that scrolling doesn't do.
  ASSERT_FALSE(base::FeatureList::IsEnabled(tabs::kScrollableTabStrip));

  SetTabContainerWidth(200);
  // Add tabs to a single group until the last one is not visible.
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  do {
    AddTab(0, group);
    tab_container_->CompleteAnimationAndLayout();
  } while (tab_container_->GetTabAtModelIndex(tab_container_->GetTabCount() - 1)
               ->GetVisible());

  const TabGroupUnderline* underline = ListGroupViews()[0]->underline();
  const gfx::Rect initial_bounds = underline->bounds();

  // Shrink the TabContainer and verify that the underline bounds changed. Use
  // the abridged version of the method to ensure TabContainer::Layout is called
  // exactly once.
  SetTabContainerWidthSingleLayout(100);
  const gfx::Rect shrunk_bounds = underline->bounds();
  EXPECT_NE(shrunk_bounds, initial_bounds);

  // Re-expand the TabContainer and verify that the underline bounds changed.
  // Use the abridged version of the method to ensure TabContainer::Layout is
  // called exactly once.
  SetTabContainerWidthSingleLayout(300);
  EXPECT_NE(underline->bounds(), initial_bounds);
  EXPECT_NE(underline->bounds(), shrunk_bounds);
}

TEST_F(TabContainerTest, UnderlineBoundsCollapsedGroupHeaderVisibilityChange) {
  // Validates that group underlines are updated correctly in a single Layout
  // call when the visibility of the group header changes, even if the group is
  // collapsed. See crbug.com/1374614

  // This test is only valid with scrolling off, since it pertains to tab
  // visibility stuff that scrolling doesn't do.
  ASSERT_FALSE(base::FeatureList::IsEnabled(tabs::kScrollableTabStrip));

  SetTabContainerWidth(200);
  // Create a tab group with one tab and collapse it.
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTab(0, std::nullopt, TabActive::kActive);
  AddTab(1, group);
  tab_strip_controller_->ToggleTabGroupCollapsedState(
      group, ToggleTabGroupCollapsedStateOrigin::kMouse);
  // Add tabs until the group header is not visible.
  do {
    AddTab(0);
    tab_container_->CompleteAnimationAndLayout();
  } while (ListGroupViews()[0]->header()->GetVisible());

  const TabGroupUnderline* underline = ListGroupViews()[0]->underline();
  const gfx::Rect initial_bounds = underline->bounds();

  // Expand the TabContainer and verify that the underline bounds changed.
  // Use the abridged version of the method to ensure TabContainer::Layout is
  // called exactly once.
  SetTabContainerWidthSingleLayout(300);
  EXPECT_NE(underline->bounds(), initial_bounds);
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

TEST_F(TabContainerTest, PreferredWidthDuringAnimation) {
  AddTab(0);
  AddTab(0);
  const int initial_pref_width = tab_container_->GetPreferredSize().width();

  // Trigger an animation.
  RemoveTab(0);
  ASSERT_TRUE(tab_container_->IsAnimating());

  // During animations, container preferred size should animate smoothly.
  EXPECT_EQ(initial_pref_width, tab_container_->GetPreferredSize().width());

  // Minimum size should match preferred width during animations.
  EXPECT_EQ(tab_container_->GetPreferredSize().width(),
            tab_container_->GetMinimumSize().width());

  // Complete the animation and the preferred width should match ideal bounds of
  // the trailingmost tab.
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_NE(initial_pref_width, tab_container_->GetPreferredSize().width());
  EXPECT_EQ(tab_container_->GetPreferredSize().width(),
            tab_container_->GetIdealBounds(tab_container_->GetTabCount() - 1)
                .right());
}

TEST_F(TabContainerTest, PreferredWidthNotAffectedByTransferTabTo) {
  // Start with two tabs.
  AddTab(0);
  AddTab(1);
  const int initial_pref_width = tab_container_->GetPreferredSize().width();

  // Transfer one out, then pretend to animate it.
  std::unique_ptr<views::View> hold_my_tab = std::make_unique<views::View>();
  hold_my_tab->AddChildView(tab_container_->RemoveTabFromViewModel(1));
  tab_container_controller_->set_is_animating_outside_container(true);
  // Preferred width should be unchanged, even though `owned_tab` is no longer
  // part of `tab_container_`.
  EXPECT_EQ(initial_pref_width, tab_container_->GetPreferredSize().width());
  // Minimum size should match preferred width during animations.
  EXPECT_EQ(tab_container_->GetPreferredSize().width(),
            tab_container_->GetMinimumSize().width());

  // Complete the animation and stop pretending.
  tab_container_->CompleteAnimationAndLayout();
  tab_container_controller_->set_is_animating_outside_container(false);
  // Preferred width should now be changed.
  EXPECT_NE(initial_pref_width, tab_container_->GetPreferredSize().width());
}

TEST_F(TabContainerTest, PreferredWidthAddTabToViewModel) {
  // Start with one tab, and one more that is not in the container.
  AddTab(0);
  const auto owned_tab = std::make_unique<Tab>(tab_slot_controller_.get());
  const int initial_pref_width = tab_container_->GetPreferredSize().width();

  // Add `owned_tab` to `tab_container_`'s viewmodel without giving it the
  // actual view, and pretend to animate it.
  tab_container_->AddTabToViewModel(owned_tab.get(), 1, TabPinned::kUnpinned);
  tab_container_controller_->set_is_animating_outside_container(true);
  // Preferred width should be unchanged.
  EXPECT_EQ(initial_pref_width, tab_container_->GetPreferredSize().width());
  // Minimum size should match preferred width during animations.
  EXPECT_EQ(tab_container_->GetPreferredSize().width(),
            tab_container_->GetMinimumSize().width());

  // Complete animation and stop pretending.
  tab_container_->CompleteAnimationAndLayout();
  tab_container_controller_->set_is_animating_outside_container(false);
  // Preferred width should be changed, even though we still haven't handed the
  // actual view over.
  EXPECT_NE(initial_pref_width, tab_container_->GetPreferredSize().width());
}

TEST_F(TabContainerTest, TabDestroyedWhileOutOfContainerDoesNotActuallyReturn) {
  // Add a tab, but take the view to simulate an outside-container animation.
  std::unique_ptr<views::View> tab_parent = std::make_unique<views::View>();
  Tab* tab_ptr =
      tab_parent->AddChildView(tab_container_->RemoveChildViewT(AddTab(0)));

  // Simulate destroying the tabstrip during this animation:
  // 1. Close the tab.
  RemoveTab(0);
  // 2. Remove it from the view hierarchy (this would happen as part of the
  // tab's destructor).
  std::unique_ptr<Tab> tab = tab_parent->RemoveChildViewT(tab_ptr);
  // 3. BoundsAnimator completes the animation, which returns the TabSlotView.
  tab_container_->ReturnTabSlotView(tab_ptr);

  // Validate that `tab_container_` did not actually take the tab view back.
  EXPECT_EQ(tab->parent(), nullptr);
}

TEST_F(TabContainerTest, GetLeadingTrailingElementsForZOrdering) {
  // An empty TabContainer has no leading/trailing views.
  EXPECT_EQ(tab_container_->GetLeadingElementForZOrdering(), std::nullopt);
  EXPECT_EQ(tab_container_->GetTrailingElementForZOrdering(), std::nullopt);

  // Leading/trailing views could be tabs.
  Tab* const first_tab = AddTab(0);
  Tab* const last_tab = AddTab(1);
  EXPECT_EQ(tab_container_->GetLeadingElementForZOrdering()->view(), first_tab);
  EXPECT_EQ(tab_container_->GetTrailingElementForZOrdering()->view(), last_tab);

  // Leading view could be a group header.
  tab_groups::TabGroupId group = tab_groups::TabGroupId::GenerateNew();
  AddTabToGroup(0, group);
  AddTabToGroup(1, group);
  TabGroupHeader* const group_header =
      tab_container_->GetGroupViews(group)->header();
  EXPECT_EQ(tab_container_->GetLeadingElementForZOrdering()->view(),
            group_header);
}

TEST_F(TabContainerTest, TabGroupHeaderAccessibleProperties) {
  auto group = tab_groups::TabGroupId::GenerateNew();
  AddTab(0, std::nullopt, TabActive::kActive);
  AddTab(1, group);

  TabGroupHeader* const group_header =
      tab_container_->GetGroupViews(group)->header();
  ui::AXNodeData data;

  group_header->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kTabList);
}
