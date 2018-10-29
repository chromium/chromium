// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_style.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_test_views_delegate.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/path.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace {

// Walks up the views hierarchy until it finds a tab view. It returns the
// found tab view, on NULL if none is found.
views::View* FindTabView(views::View* view) {
  views::View* current = view;
  while (current && strcmp(current->GetClassName(), Tab::kViewClassName)) {
    current = current->parent();
  }
  return current;
}

class TabStripTestViewsDelegate : public ChromeTestViewsDelegate {
 public:
  TabStripTestViewsDelegate() = default;
  ~TabStripTestViewsDelegate() override = default;
  void NotifyAccessibilityEvent(views::View* view,
                                ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kSelectionRemove) {
      remove_count_++;
    }
    if (event_type == ax::mojom::Event::kSelectionAdd) {
      add_count_++;
    }
  }

  int add_count() { return add_count_; }
  int remove_count() { return remove_count_; }

 private:
  int add_count_ = 0;
  int remove_count_ = 0;
  DISALLOW_COPY_AND_ASSIGN(TabStripTestViewsDelegate);
};

class AnimationWaiter {
 public:
  AnimationWaiter(TabStrip* tab_strip, base::TimeDelta duration)
      : tab_strip_(tab_strip), duration_(duration) {}

  ~AnimationWaiter() = default;

  // Blocks until |tab_strip_| is not animating.
  void Wait() {
    interval_timer_.Start(
        FROM_HERE, duration_,
        base::BindRepeating(&AnimationWaiter::CheckAnimationEnds,
                            base::Unretained(this)));

    run_loop_.Run();
  }

 private:
  void CheckAnimationEnds() {
    if (tab_strip_->IsAnimating())
      return;

    interval_timer_.Stop();
    run_loop_.Quit();
  }

  TabStrip* tab_strip_;

  base::RepeatingTimer interval_timer_;
  base::TimeDelta duration_;

  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(AnimationWaiter);
};
}  // namespace

class TestTabStripObserver : public TabStripObserver {
 public:
  explicit TestTabStripObserver(TabStrip* tab_strip) : tab_strip_(tab_strip) {
    tab_strip_->AddObserver(this);
  }

  ~TestTabStripObserver() override { tab_strip_->RemoveObserver(this); }

  int last_tab_added() const { return last_tab_added_; }
  int last_tab_removed() const { return last_tab_removed_; }
  int last_tab_moved_from() const { return last_tab_moved_from_; }
  int last_tab_moved_to() const { return last_tab_moved_to_; }

 private:
  // TabStripObserver overrides.
  void OnTabAdded(int index) override { last_tab_added_ = index; }

  void OnTabMoved(int from_index, int to_index) override {
    last_tab_moved_from_ = from_index;
    last_tab_moved_to_ = to_index;
  }

  void OnTabRemoved(int index) override { last_tab_removed_ = index; }

  TabStrip* tab_strip_;
  int last_tab_added_ = -1;
  int last_tab_removed_ = -1;
  int last_tab_moved_from_ = -1;
  int last_tab_moved_to_ = -1;

  DISALLOW_COPY_AND_ASSIGN(TestTabStripObserver);
};

class TabStripTest : public ChromeViewsTestBase,
                     public testing::WithParamInterface<bool> {
 public:
  TabStripTest() : test_api_(GetParam()) {}

  ~TabStripTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ = new TabStrip(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);
    // Do this to force TabStrip to create the buttons.
    parent_.AddChildView(tab_strip_);
    parent_.set_owned_by_client();

    widget_.reset(new views::Widget);
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(init_params);
    widget_->SetContentsView(&parent_);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  std::unique_ptr<views::TestViewsDelegate> CreateTestViewsDelegate() override {
    std::unique_ptr<TabStripTestViewsDelegate> delegate =
        std::make_unique<TabStripTestViewsDelegate>();
    test_views_delegate_ = delegate.get();
    return delegate;
  }

  bool IsShowingAttentionIndicator(Tab* tab) {
    return tab->icon_->ShowingAttentionIndicator();
  }

  // Checks whether |tab| contains |point_in_tabstrip_coords|, where the point
  // is in |tab_strip_| coordinates.
  bool IsPointInTab(Tab* tab, const gfx::Point& point_in_tabstrip_coords) {
    gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
    views::View::ConvertPointToTarget(tab_strip_, tab, &point_in_tab_coords);
    return tab->HitTestPoint(point_in_tab_coords);
  }

  Tab* FindTabForEvent(const gfx::Point& point) {
    return tab_strip_->FindTabForEvent(point);
  }

  void DoLayout() { tab_strip_->DoLayout(); }

  void AnimateToIdealBounds() { tab_strip_->AnimateToIdealBounds(); }

  int current_inactive_width() const {
    return tab_strip_->current_inactive_width_;
  }

  const StackedTabStripLayout* touch_layout() const {
    return tab_strip_->touch_layout_.get();
  }

  views::BoundsAnimator* bounds_animator() {
    return &tab_strip_->bounds_animator_;
  }

  // End any outstanding drag and animate tabs back to their ideal bounds.
  void StopDraggingTab(Tab* tab) {
    // Passing false for |is_first_tab| results in running the post-drag
    // animation unconditionally.
    bool is_first_tab = false;
    tab_strip_->StoppedDraggingTab(tab, &is_first_tab);
  }

  // Owned by TabStrip.
  FakeBaseTabStripController* controller_ = nullptr;
  // Owns |tab_strip_|.
  views::View parent_;
  TabStrip* tab_strip_ = nullptr;
  TabStripTestViewsDelegate* test_views_delegate_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

 private:
  ui::test::MaterialDesignControllerTestAPI test_api_;

  DISALLOW_COPY_AND_ASSIGN(TabStripTest);
};

TEST_P(TabStripTest, GetModelCount) {
  EXPECT_EQ(0, tab_strip_->GetModelCount());
}

TEST_P(TabStripTest, AccessibilityEvents) {
  // When adding tabs, SetSelection() is called after AddTabAt(), as
  // otherwise the index would not be meaningful.
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), true);
  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(1);
  tab_strip_->SetSelection(selection);
  EXPECT_EQ(1, test_views_delegate_->add_count());
  EXPECT_EQ(0, test_views_delegate_->remove_count());

  // When removing tabs, SetSelection() is called before RemoveTabAt(), as
  // otherwise the index would not be meaningful.
  selection.SetSelectedIndex(0);
  tab_strip_->SetSelection(selection);
  tab_strip_->RemoveTabAt(nullptr, 1, true);
  EXPECT_EQ(2, test_views_delegate_->add_count());
  EXPECT_EQ(1, test_views_delegate_->remove_count());
}

TEST_P(TabStripTest, IsValidModelIndex) {
  EXPECT_FALSE(tab_strip_->IsValidModelIndex(0));
}

TEST_P(TabStripTest, tab_count) {
  EXPECT_EQ(0, tab_strip_->tab_count());
}

TEST_P(TabStripTest, AddTabAt) {
  TestTabStripObserver observer(tab_strip_);
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  ASSERT_EQ(1, tab_strip_->tab_count());
  EXPECT_EQ(0, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  EXPECT_FALSE(tab == NULL);
}

TEST_P(TabStripTest, MoveTab) {
  TestTabStripObserver observer(tab_strip_);
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), false);
  tab_strip_->AddTabAt(2, TabRendererData(), false);
  ASSERT_EQ(3, tab_strip_->tab_count());
  EXPECT_EQ(2, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  tab_strip_->MoveTab(0, 1, TabRendererData());
  EXPECT_EQ(0, observer.last_tab_moved_from());
  EXPECT_EQ(1, observer.last_tab_moved_to());
  EXPECT_EQ(tab, tab_strip_->tab_at(1));
}

// Verifies child views are deleted after an animation completes.
TEST_P(TabStripTest, RemoveTab) {
  TestTabStripObserver observer(tab_strip_);
  controller_->AddTab(0, false);
  controller_->AddTab(1, false);
  const int child_view_count = tab_strip_->child_count();
  EXPECT_EQ(2, tab_strip_->tab_count());
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
  // When removing a tab the tabcount should immediately decrement.
  EXPECT_EQ(1, tab_strip_->tab_count());
  // But the number of views should remain the same (it's animatining closed).
  EXPECT_EQ(child_view_count, tab_strip_->child_count());
  tab_strip_->SetBounds(0, 0, 200, 20);
  // Layout at a different size should force the animation to end and delete
  // the tab that was removed.
  tab_strip_->Layout();
  EXPECT_EQ(child_view_count - 1, tab_strip_->child_count());

  // Remove the last tab to make sure things are cleaned up correctly when
  // the TabStrip is destroyed and an animation is ongoing.
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
}

TEST_P(TabStripTest, VisibilityInOverflow) {
  constexpr int kInitialWidth = 250;
  tab_strip_->SetBounds(0, 0, kInitialWidth, 20);

  // The first tab added to a reasonable-width strip should be visible.  If we
  // add enough additional tabs, eventually one should be invisible due to
  // overflow.
  int invisible_tab_index = 0;
  for (; invisible_tab_index < 100; ++invisible_tab_index) {
    controller_->AddTab(invisible_tab_index, false);
    if (!tab_strip_->tab_at(invisible_tab_index)->visible())
      break;
  }
  EXPECT_GT(invisible_tab_index, 0);
  EXPECT_LT(invisible_tab_index, 100);

  // The tabs before the invisible tab should still be visible.
  for (int i = 0; i < invisible_tab_index; ++i)
    EXPECT_TRUE(tab_strip_->tab_at(i)->visible());

  // Enlarging the strip should result in the last tab becoming visible.
  tab_strip_->SetBounds(0, 0, kInitialWidth * 2, 20);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // Shrinking it again should re-hide the last tab.
  tab_strip_->SetBounds(0, 0, kInitialWidth, 20);
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // Shrinking it still more should make more tabs invisible, though not all.
  // All the invisible tabs should be at the end of the strip.
  tab_strip_->SetBounds(0, 0, kInitialWidth / 2, 20);
  int i = 0;
  for (; i < invisible_tab_index; ++i) {
    if (!tab_strip_->tab_at(i)->visible())
      break;
  }
  ASSERT_GT(i, 0);
  EXPECT_LT(i, invisible_tab_index);
  invisible_tab_index = i;
  for (int i = invisible_tab_index + 1; i < tab_strip_->tab_count(); ++i)
    EXPECT_FALSE(tab_strip_->tab_at(i)->visible());

  // When we're already in overflow, adding tabs at the beginning or end of
  // the strip should not change how many tabs are visible.
  controller_->AddTab(tab_strip_->tab_count(), false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->visible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());
  controller_->AddTab(0, false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->visible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->visible());

  // If we remove enough tabs, all the tabs should be visible.
  for (int i = tab_strip_->tab_count() - 1; i >= invisible_tab_index; --i)
    controller_->RemoveTab(i);
  EXPECT_TRUE(tab_strip_->tab_at(tab_strip_->tab_count() - 1)->visible());
}

// Creates a tab strip in stacked layout mode and verifies that as we move
// across the strip at the top, middle, and bottom, events will target each tab
// in order.
TEST_P(TabStripTest, TabForEventWhenStacked) {
  tab_strip_->SetBounds(0, 0, 250, GetLayoutConstant(TAB_HEIGHT));

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Switch to stacked layout mode and force a layout to ensure tabs stack.
  tab_strip_->SetStackedLayout(true);
  DoLayout();

  gfx::Point p;
  for (int y : {0, tab_strip_->height() / 2, tab_strip_->height() - 1}) {
    p.set_y(y);
    int previous_tab = -1;
    for (int x = 0; x < tab_strip_->width(); ++x) {
      p.set_x(x);
      int tab = tab_strip_->GetModelIndexOfTab(FindTabForEvent(p));
      if (tab == previous_tab)
        continue;
      if ((tab != -1) || (previous_tab != tab_strip_->tab_count() - 1))
        EXPECT_EQ(previous_tab + 1, tab) << "p = " << p.ToString();
      previous_tab = tab;
    }
  }
}

// Tests that the tab close buttons of non-active tabs are hidden when
// the tabstrip is in stacked tab mode.
TEST_P(TabStripTest, TabCloseButtonVisibilityWhenStacked) {
  // Touch-optimized UI requires a larger width for tabs to show close buttons.
  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  tab_strip_->SetBounds(0, 0, touch_ui ? 442 : 346, 20);
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  ASSERT_EQ(3, tab_strip_->tab_count());

  Tab* tab0 = tab_strip_->tab_at(0);
  Tab* tab1 = tab_strip_->tab_at(1);
  ASSERT_TRUE(tab1->IsActive());
  Tab* tab2 = tab_strip_->tab_at(2);

  // Ensure that all tab close buttons are initially visible.
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);

  // Enter stacked layout mode and verify this sets |touch_layout_|.
  ASSERT_FALSE(touch_layout());
  tab_strip_->SetStackedLayout(true);
  ASSERT_TRUE(touch_layout());

  // Only the close button of the active tab should be visible in stacked
  // layout mode.
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_FALSE(tab2->showing_close_button_);

  // An inactive tab added to the tabstrip should not show
  // its tab close button.
  controller_->AddTab(3, false);
  Tab* tab3 = tab_strip_->tab_at(3);
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_FALSE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // After switching tabs, the previously-active tab should have its
  // tab close button hidden and the newly-active tab should show
  // its tab close button.
  tab_strip_->SelectTab(tab2);
  ASSERT_FALSE(tab1->IsActive());
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // After closing the active tab, the tab which becomes active should
  // show its tab close button.
  tab_strip_->CloseTab(tab1, CLOSE_TAB_FROM_TOUCH);
  tab1 = nullptr;
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);

  // All tab close buttons should be shown when disengaging stacked tab mode.
  tab_strip_->SetStackedLayout(false);
  ASSERT_FALSE(touch_layout());
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_TRUE(tab3->showing_close_button_);
}

// Tests that the tab close buttons of non-active tabs are hidden when
// the tabstrip is not in stacked tab mode and the tab sizes are shrunk
// into small sizes.
TEST_P(TabStripTest, TabCloseButtonVisibilityWhenNotStacked) {
  // Set the tab strip width to be wide enough for three tabs to show all
  // three icons, but not enough for five tabs to show all three icons.
  // Touch-optimized UI requires a larger width for tabs to show close buttons.
  const bool touch_ui = ui::MaterialDesignController::touch_ui();
  tab_strip_->SetBounds(0, 0, touch_ui ? 442 : 346, 20);
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  ASSERT_EQ(3, tab_strip_->tab_count());

  Tab* tab0 = tab_strip_->tab_at(0);
  ASSERT_FALSE(tab0->IsActive());
  Tab* tab1 = tab_strip_->tab_at(1);
  ASSERT_TRUE(tab1->IsActive());
  Tab* tab2 = tab_strip_->tab_at(2);
  ASSERT_FALSE(tab2->IsActive());

  // Ensure this is not in stacked layout mode.
  ASSERT_FALSE(touch_layout());

  // Ensure that all tab close buttons are initially visible.
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);

  // Shrink the tab sizes by adding more tabs.
  // An inactive tab added to the tabstrip, now each tab size is not
  // big enough to accomodate 3 icons, so it should not show its
  // tab close button.
  controller_->AddTab(3, false);
  Tab* tab3 = tab_strip_->tab_at(3);
  EXPECT_FALSE(tab3->showing_close_button_);

  // This inactive tab doesn't have alert button, but its favicon and
  // title would be shown.
  EXPECT_TRUE(tab3->showing_icon_);
  EXPECT_FALSE(tab3->showing_alert_indicator_);
  EXPECT_TRUE(tab3->title_->visible());

  // The active tab's close button still shows.
  EXPECT_TRUE(tab1->showing_close_button_);

  // An active tab added to the tabstrip should show its tab close
  // button.
  controller_->AddTab(4, true);
  Tab* tab4 = tab_strip_->tab_at(4);
  ASSERT_TRUE(tab4->IsActive());
  EXPECT_TRUE(tab4->showing_close_button_);

  // The previous active button is now inactive so its close
  // button should not show.
  EXPECT_FALSE(tab1->showing_close_button_);

  // After switching tabs, the previously-active tab should have its
  // tab close button hidden and the newly-active tab should show
  // its tab close button.
  tab_strip_->SelectTab(tab2);
  ASSERT_FALSE(tab4->IsActive());
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);
  EXPECT_FALSE(tab3->showing_close_button_);
  EXPECT_FALSE(tab4->showing_close_button_);

  // After closing the active tab, the tab which becomes active should
  // show its tab close button.
  tab_strip_->CloseTab(tab2, CLOSE_TAB_FROM_TOUCH);
  tab2 = nullptr;
  ASSERT_TRUE(tab3->IsActive());
  DoLayout();
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
  EXPECT_TRUE(tab3->showing_close_button_);
  EXPECT_FALSE(tab4->showing_close_button_);
}

TEST_P(TabStripTest, GetEventHandlerForOverlappingArea) {
  tab_strip_->SetBounds(0, 0, 1000, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Verify that the active tab will be a tooltip handler for points that hit
  // it.
  Tab* left_tab = tab_strip_->tab_at(0);
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));

  Tab* active_tab = tab_strip_->tab_at(1);
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  Tab* right_tab = tab_strip_->tab_at(2);
  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));

  Tab* most_right_tab = tab_strip_->tab_at(3);
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
            FindTabView(tab_strip_->GetEventHandlerForPoint(left_overlap)));

  // Test that active tabs gets events from area in which it overlaps with its
  // right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetEventHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(right_tab,
            FindTabView(tab_strip_->GetEventHandlerForPoint(unactive_overlap)));
}

TEST_P(TabStripTest, GetTooltipHandler) {
  tab_strip_->SetBounds(0, 0, 1000, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);
  controller_->AddTab(3, false);
  ASSERT_EQ(4, tab_strip_->tab_count());

  // Verify that the active tab will be a tooltip handler for points that hit
  // it.
  Tab* left_tab = tab_strip_->tab_at(0);
  left_tab->SetBoundsRect(gfx::Rect(gfx::Point(0, 0), gfx::Size(200, 20)));

  Tab* active_tab = tab_strip_->tab_at(1);
  active_tab->SetBoundsRect(gfx::Rect(gfx::Point(150, 0), gfx::Size(200, 20)));
  ASSERT_TRUE(active_tab->IsActive());

  Tab* right_tab = tab_strip_->tab_at(2);
  right_tab->SetBoundsRect(gfx::Rect(gfx::Point(300, 0), gfx::Size(200, 20)));

  Tab* most_right_tab = tab_strip_->tab_at(3);
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

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetTooltipHandlerForPoint(left_overlap)));

  // Test that active_tab handles tooltips from area in which it overlaps with
  // its right neighbour.
  gfx::Point right_overlap((active_tab->bounds().right() + right_tab->x()) / 2,
                           active_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and right tab.
  ASSERT_TRUE(IsPointInTab(active_tab, right_overlap));
  ASSERT_TRUE(IsPointInTab(right_tab, right_overlap));

  EXPECT_EQ(active_tab,
            FindTabView(tab_strip_->GetTooltipHandlerForPoint(right_overlap)));

  // Test that if neither of tabs is active, the left one is selected.
  gfx::Point unactive_overlap(
      (right_tab->x() + most_right_tab->bounds().right() + 1) / 2,
      right_tab->bounds().bottom() - 1);

  // Sanity check that the point is in both active and left tab.
  ASSERT_TRUE(IsPointInTab(right_tab, unactive_overlap));
  ASSERT_TRUE(IsPointInTab(most_right_tab, unactive_overlap));

  EXPECT_EQ(
      right_tab,
      FindTabView(tab_strip_->GetTooltipHandlerForPoint(unactive_overlap)));

  // Confirm that tab strip doe not return tooltip handler for points that
  // don't hit it.
  EXPECT_FALSE(tab_strip_->GetTooltipHandlerForPoint(gfx::Point(-1, 2)));
}

TEST_P(TabStripTest, NewTabButtonStaysVisible) {
  const int kTabStripWidth = 500;
  tab_strip_->SetBounds(0, 0, kTabStripWidth, 20);

  for (int i = 0; i < 100; ++i)
    controller_->AddTab(i, (i == 0));

  DoLayout();

  EXPECT_LE(tab_strip_->new_tab_button_bounds().right(), kTabStripWidth);
}

// The active tab should always be at least as wide as its minimum width.
// http://crbug.com/587688
TEST_P(TabStripTest, ActiveTabWidthWhenTabsAreTiny) {
  // The bug was caused when it's animating. Therefore we should make widget
  // visible so that animation can be triggered.
  tab_strip_->GetWidget()->Show();
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyle::GetMinimumInactiveWidth();
  while (current_inactive_width() != min_inactive_width)
    controller_->CreateNewTab();

  int active_index = controller_->GetActiveIndex();
  EXPECT_GT(tab_strip_->tab_count(), 1);
  EXPECT_EQ(tab_strip_->tab_count() - 1, active_index);
  EXPECT_LT(tab_strip_->ideal_bounds(0).width(),
            tab_strip_->ideal_bounds(active_index).width());

  // During mouse-based tab closure, the active tab should remain at least as
  // wide as it's minium width.
  controller_->SelectTab(0);
  for (const int min_active_width = TabStyle::GetMinimumActiveWidth();
       tab_strip_->tab_count();) {
    const int active_index = controller_->GetActiveIndex();
    EXPECT_GE(tab_strip_->ideal_bounds(active_index).width(), min_active_width);
    tab_strip_->CloseTab(tab_strip_->tab_at(active_index),
                         CLOSE_TAB_FROM_MOUSE);
  }
}

// Inactive tabs shouldn't shrink during mouse-based tab closure.
// http://crbug.com/850190
TEST_P(TabStripTest, InactiveTabWidthWhenTabsAreTiny) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs smaller than active
  // tab but not the minimum.
  const int min_inactive_width = TabStyle::GetMinimumInactiveWidth();
  const int min_active_width = TabStyle::GetMinimumActiveWidth();
  while (current_inactive_width() >=
         (min_inactive_width + min_active_width) / 2)
    controller_->CreateNewTab();

  // During mouse-based tab closure, inactive tabs shouldn't shrink
  // so that users can close tabs continuously without moving mouse.
  controller_->SelectTab(0);
  for (int old_inactive_width = current_inactive_width();
       tab_strip_->tab_count(); old_inactive_width = current_inactive_width()) {
    tab_strip_->CloseTab(tab_strip_->tab_at(controller_->GetActiveIndex()),
                         CLOSE_TAB_FROM_MOUSE);
    EXPECT_GE(current_inactive_width(), old_inactive_width);
  }
}

// When dragged tabs are moving back to their position, changes to ideal bounds
// should be respected. http://crbug.com/848016
TEST_P(TabStripTest, ResetBoundsForDraggedTabs) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyle::GetMinimumInactiveWidth();
  while (current_inactive_width() != min_inactive_width)
    controller_->CreateNewTab();

  const int min_active_width = TabStyle::GetMinimumActiveWidth();

  int dragged_tab_index = controller_->GetActiveIndex();
  EXPECT_GE(tab_strip_->ideal_bounds(dragged_tab_index).width(),
            min_active_width);

  // Mark the active tab as being dragged.
  Tab* dragged_tab = tab_strip_->tab_at(dragged_tab_index);
  dragged_tab->set_dragging(true);

  // Ending the drag triggers the tabstrip to begin animating this tab back
  // to its ideal bounds.
  StopDraggingTab(dragged_tab);
  EXPECT_TRUE(bounds_animator()->IsAnimating(dragged_tab));

  // Change the ideal bounds of the tabs mid-animation by selecting a
  // different tab.
  controller_->SelectTab(0);

  // Once the animation completes, the dragged tab should have animated to
  // the new ideal bounds (computed with this as an inactive tab) rather
  // than the original ones (where it's an active tab).
  const auto duration = base::TimeDelta::FromMilliseconds(
      bounds_animator()->GetAnimationDuration());
  AnimationWaiter waiter(tab_strip_, duration);
  waiter.Wait();

  EXPECT_FALSE(dragged_tab->dragging());
  EXPECT_LT(dragged_tab->bounds().width(), min_active_width);
}

// The "blocked" attention indicator should only show for background tabs.
TEST_P(TabStripTest, TabNeedsAttentionBlocked) {
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);

  Tab* tab1 = tab_strip_->tab_at(1);

  // Block tab1.
  TabRendererData data;
  data.blocked = true;
  tab1->SetData(data);

  EXPECT_FALSE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(0);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(1);
  EXPECT_FALSE(IsShowingAttentionIndicator(tab1));
}

// The generic "wants attention" version should always show.
TEST_P(TabStripTest, TabNeedsAttentionGeneric) {
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);

  Tab* tab1 = tab_strip_->tab_at(1);

  tab1->SetTabNeedsAttention(true);

  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(0);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(1);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
}

TEST_P(TabStripTest, NewTabButtonInkDrop) {
  constexpr int kTabStripWidth = 500;
  tab_strip_->SetBounds(0, 0, kTabStripWidth, GetLayoutConstant(TAB_HEIGHT));

  // Add a few tabs and simulate the new tab button's ink drop animation. This
  // should not cause any crashes since the ink drop layer size as well as the
  // ink drop container size should remain equal to the new tab button visible
  // bounds size. https://crbug.com/814105.
  for (int i = 0; i < 10; ++i) {
    tab_strip_->new_tab_button()->AnimateInkDropToStateForTesting(
        views::InkDropState::ACTION_TRIGGERED);
    controller_->AddTab(i, true /* is_active */);
    DoLayout();
    tab_strip_->new_tab_button()->AnimateInkDropToStateForTesting(
        views::InkDropState::HIDDEN);
  }
}

INSTANTIATE_TEST_CASE_P(, TabStripTest, ::testing::Values(false, true));
