// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/compound_tab_container.h"

#include <memory>
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/fake_tab_slot_controller.h"
#include "chrome/browser/ui/views/tabs/tab_drag_context.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "tab_style_views.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class FakeTabDragContext : public TabDragContextBase {
 public:
  FakeTabDragContext() = default;
  ~FakeTabDragContext() override = default;

  void UpdateAnimationTarget(TabSlotView* tab_slot_view,
                             const gfx::Rect& target_bounds) override {}
  bool IsDragSessionActive() const override { return false; }
  bool IsAnimatingDragEnd() const override { return false; }
  void CompleteEndDragAnimations() override {}
  int GetTabDragAreaWidth() const override { return width(); }
};

class FakeTabContainerController final : public TabContainerController {
 public:
  explicit FakeTabContainerController(TabStripController& tab_strip_controller)
      : tab_strip_controller_(tab_strip_controller) {}
  ~FakeTabContainerController() override = default;

  bool IsValidModelIndex(int index) const override {
    return tab_strip_controller_->IsValidIndex(index);
  }

  int GetActiveIndex() const override {
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

  void OnDropIndexUpdate(int index, bool drop_before) override {
    tab_strip_controller_->OnDropIndexUpdate(index, drop_before);
  }

  bool IsGroupCollapsed(const tab_groups::TabGroupId& group) const override {
    return tab_strip_controller_->IsGroupCollapsed(group);
  }

  absl::optional<int> GetFirstTabInGroup(
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

 private:
  const raw_ref<TabStripController> tab_strip_controller_;
};
}  // namespace

class CompoundTabContainerTest : public ChromeViewsTestBase {
 public:
  CompoundTabContainerTest() = default;
  CompoundTabContainerTest(const CompoundTabContainerTest&) = delete;
  CompoundTabContainerTest& operator=(const CompoundTabContainerTest&) = delete;
  ~CompoundTabContainerTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    tab_strip_controller_ = std::make_unique<FakeBaseTabStripController>();
    tab_container_controller_ = std::make_unique<FakeTabContainerController>(
        *(tab_strip_controller_.get()));
    tab_slot_controller_ =
        std::make_unique<FakeTabSlotController>(tab_strip_controller_.get());

    std::unique_ptr<TabDragContextBase> drag_context =
        std::make_unique<FakeTabDragContext>();
    std::unique_ptr<TabContainer> tab_container =
        std::make_unique<CompoundTabContainer>(
            raw_ref<TabContainerController>(*(tab_container_controller_.get())),
            nullptr /*hover_card_controller*/, drag_context.get(),
            *(tab_slot_controller_.get()), nullptr /*scroll_contents_view*/);
    tab_container->SetAvailableWidthCallback(base::BindRepeating(
        [](CompoundTabContainerTest* test) {
          return test->tab_container_width_;
        },
        this));

    widget_ = CreateTestWidget();
    tab_container_ =
        widget_->GetRootView()->AddChildView(std::move(tab_container));
    drag_context_ =
        widget_->GetRootView()->AddChildView(std::move(drag_context));
    SetTabContainerWidth(1000);

    tab_slot_controller_->set_tab_container(tab_container_);
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
              TabPinned pinned,
              absl::optional<tab_groups::TabGroupId> group = absl::nullopt,
              TabActive active = TabActive::kInactive) {
    Tab* tab = tab_container_->AddTab(
        std::make_unique<Tab>(tab_slot_controller_.get()), model_index, pinned);
    tab_strip_controller_->AddTab(model_index, active, pinned);

    if (active == TabActive::kActive)
      tab_slot_controller_->set_active_tab(tab);

    if (group) {
      NOTREACHED();  // TODO(crbug.com/1346017): copy/reuse more stuff from
                     // TabContainerTest
      AddTabToGroup(model_index, group.value());
    }

    TabRendererData tab_data = tab->data();
    tab_data.pinned = pinned == TabPinned::kPinned;
    tab->SetData(tab_data);

    return tab;
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

  void SetTabContainerWidth(int width) {
    tab_container_width_ = width;
    gfx::Size size(tab_container_width_, GetLayoutConstant(TAB_HEIGHT));
    widget_->SetSize(size);
    drag_context_->SetSize(size);
    tab_container_->SetSize(size);
  }

  std::unique_ptr<FakeBaseTabStripController> tab_strip_controller_;
  std::unique_ptr<FakeTabContainerController> tab_container_controller_;
  std::unique_ptr<FakeTabSlotController> tab_slot_controller_;
  raw_ptr<TabDragContextBase> drag_context_;
  raw_ptr<TabContainer> tab_container_;
  std::unique_ptr<views::Widget> widget_;

  int tab_container_width_ = 0;
};

TEST_F(CompoundTabContainerTest, PinnedTabReparents) {
  // Start with one tab, initially pinned.
  Tab* const tab = AddTab(0, TabPinned::kPinned);
  TabContainer* const pinned_container =
      views::AsViewClass<TabContainer>(tab->parent());
  ASSERT_NE(pinned_container, nullptr);

  // Unpin the tab and it should move to a new TabContainer.
  tab_container_->SetTabPinned(0, TabPinned::kUnpinned);
  TabContainer* const unpinned_container =
      views::AsViewClass<TabContainer>(tab->parent());
  ASSERT_NE(unpinned_container, nullptr);
  EXPECT_NE(pinned_container, unpinned_container);

  // Re-pin the tab and it should move back.
  tab_container_->SetTabPinned(0, TabPinned::kPinned);
  EXPECT_EQ(tab->parent(), pinned_container);
}

TEST_F(CompoundTabContainerTest, MoveTabsWithinContainers) {
  // Start with two tabs each pinned and unpinned.
  const Tab* const tab0 = AddTab(0, TabPinned::kPinned);
  const Tab* const tab1 = AddTab(1, TabPinned::kPinned);
  const Tab* const tab2 = AddTab(2, TabPinned::kUnpinned);
  const Tab* const tab3 = AddTab(3, TabPinned::kUnpinned);

  // Swap each pair.
  tab_container_->MoveTab(0, 1);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(0), tab1);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(1), tab0);

  tab_container_->MoveTab(2, 3);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(2), tab3);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(3), tab2);

  // And back again.
  tab_container_->MoveTab(1, 0);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(0), tab0);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(1), tab1);

  tab_container_->MoveTab(3, 2);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(2), tab2);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(3), tab3);
}

TEST_F(CompoundTabContainerTest, MoveTabBetweenContainers) {
  // Start with one pinned tab and two unpinned tabs.
  const views::View* const pinned_container =
      AddTab(0, TabPinned::kPinned)->parent();
  const views::View* const unpinned_container =
      AddTab(1, TabPinned::kUnpinned)->parent();
  Tab* const moving_tab = AddTab(2, TabPinned::kUnpinned);
  TabRendererData moving_tab_data = moving_tab->data();

  // Pin `moving_tab` as part of a move.
  moving_tab_data.pinned = true;
  moving_tab->SetData(moving_tab_data);
  tab_container_->MoveTab(2, 1);
  // It should be pinned and at index 1.
  EXPECT_EQ(moving_tab->parent(), pinned_container);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(1), moving_tab);

  // Move it to index 0, then unpin it as part of another move.
  tab_container_->MoveTab(1, 0);
  moving_tab_data.pinned = false;
  moving_tab->SetData(moving_tab_data);
  tab_container_->MoveTab(0, 1);
  // It should be unpinned and at index 1.
  EXPECT_EQ(moving_tab->parent(), unpinned_container);
  EXPECT_EQ(tab_container_->GetTabAtModelIndex(1), moving_tab);
}

TEST_F(CompoundTabContainerTest, RemoveTab) {
  // Start with two pinned tabs and two unpinned tabs.
  AddTab(0, TabPinned::kPinned);
  AddTab(1, TabPinned::kPinned);
  AddTab(2, TabPinned::kUnpinned);
  AddTab(3, TabPinned::kUnpinned);

  // Remove the last tab.
  RemoveTab(3);
  EXPECT_EQ(tab_container_->GetTabCount(), 3);
  // Remove the middle tab.
  RemoveTab(1);
  EXPECT_EQ(tab_container_->GetTabCount(), 2);
  // Remove the first tab.
  RemoveTab(0);
  EXPECT_EQ(tab_container_->GetTabCount(), 1);
  // Remove the only remaining tab.
  RemoveTab(0);
  EXPECT_EQ(tab_container_->GetTabCount(), 0);
}

TEST_F(CompoundTabContainerTest, GetIndexOfFirstNonClosingTab) {
  // Test that CompoundTabContainer can identify the tab events should be
  // forwarded to in case one is closing.

  // Create a tabstrip with four tabs.
  Tab* first_pinned = AddTab(0, TabPinned::kPinned);
  AddTab(1, TabPinned::kPinned);
  Tab* first_unpinned = AddTab(2, TabPinned::kUnpinned);
  AddTab(3, TabPinned::kUnpinned);

  // RemoveTab below *starts* the tab removal process, but leaves the view
  // around to be animated closed.

  // Remove `first_unpinned`, so the next non-closing tab is the other unpinned
  // tab, i.e. both tabs are in `unpinned_tab_container_`.
  RemoveTab(2);
  EXPECT_EQ(tab_container_->GetModelIndexOfFirstNonClosingTab(first_unpinned),
            2);

  // Both tabs are in `pinned_tab_container_`.
  RemoveTab(0);
  EXPECT_EQ(tab_container_->GetModelIndexOfFirstNonClosingTab(first_pinned), 0);

  // One tab is in each container.
  RemoveTab(0);
  EXPECT_EQ(tab_container_->GetModelIndexOfFirstNonClosingTab(first_pinned), 0);

  // There is no next tab, and this one is unpinned.
  RemoveTab(0);
  EXPECT_EQ(tab_container_->GetModelIndexOfFirstNonClosingTab(first_unpinned),
            -1);

  // There is no next tab, and this one is pinned.
  EXPECT_EQ(tab_container_->GetModelIndexOfFirstNonClosingTab(first_pinned),
            -1);
}

TEST_F(CompoundTabContainerTest, ExitsClosingModeAtStandardWidth) {
  AddTab(0, TabPinned::kUnpinned, absl::nullopt, TabActive::kActive);

  // Create just enough tabs so tabs are not full size.
  const int standard_width = TabStyleViews::GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0, TabPinned::kUnpinned);
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
  RemoveTab(tab_container_->GetTabCount() - 2);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->GetActiveTabWidth(), standard_width);

  // Close the last tab; tab closing mode should allow tabs to resize to full
  // size.
  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_EQ(tab_container_->GetActiveTabWidth(), standard_width);
}

TEST_F(CompoundTabContainerTest, ClosingPinnedTabsEngagesClosingMode) {
  // This test verifies that tab closing mode engages correctly when closing a
  // pinned tab.

  // Add two unpinned tabs to be governed by closing mode.
  AddTab(0, TabPinned::kUnpinned, absl::nullopt, TabActive::kActive);
  AddTab(1, TabPinned::kUnpinned, absl::nullopt, TabActive::kInactive);

  // Create just enough (pinned) tabs so the active tab is not full size.
  const int standard_width = TabStyleViews::GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0, TabPinned::kPinned, absl::nullopt, TabActive::kInactive);
    tab_container_->CompleteAnimationAndLayout();
  }

  // The test closes two tabs, we need at least one left over after that.
  ASSERT_GE(tab_container_->GetTabCount(), 3);

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(absl::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  // Close the third-to-last tab, which is the last pinned tab; tab closing mode
  // should constrain tab widths to below full size.
  RemoveTab(tab_container_->GetTabCount() - 3);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->GetActiveTabWidth(), standard_width);

  // Close the last tab, which is the inactive unpinned tab; tab closing mode
  // should allow tabs to resize to full size.
  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_EQ(tab_container_->GetActiveTabWidth(), standard_width);
}

TEST_F(CompoundTabContainerTest, ExitsClosingModeWhenClosingLastUnpinnedTab) {
  // Add two unpinned tabs to be governed by closing mode.
  AddTab(0, TabPinned::kUnpinned, absl::nullopt, TabActive::kInactive);
  AddTab(1, TabPinned::kUnpinned, absl::nullopt, TabActive::kActive);

  // Create just enough (pinned) tabs so the active tab is not full size.
  const int standard_width = TabStyleViews::GetStandardWidth();
  while (tab_container_->GetActiveTabWidth() == standard_width) {
    AddTab(0, TabPinned::kPinned);
    tab_container_->CompleteAnimationAndLayout();
  }

  // The test closes two tabs, we need at least one left over after that.
  ASSERT_GE(tab_container_->GetTabCount(), 3);

  // Enter tab closing mode manually; this would normally happen as the result
  // of a mouse/touch-based tab closure action.
  tab_container_->EnterTabClosingMode(absl::nullopt,
                                      CloseTabSource::CLOSE_TAB_FROM_MOUSE);

  // Close the second-to-last tab, which is the inactive unpinned tab; tab
  // closing mode should remain active, constraining tab widths to below full
  // size.
  RemoveTab(tab_container_->GetTabCount() - 2);
  tab_container_->CompleteAnimationAndLayout();
  ASSERT_LT(tab_container_->GetActiveTabWidth(), standard_width);

  // Close the last tab, which is the active unpinned tab; tab closing mode
  // should exit.
  RemoveTab(tab_container_->GetTabCount() - 1);
  tab_container_->CompleteAnimationAndLayout();
  EXPECT_FALSE(tab_container_->InTabClose());
}
