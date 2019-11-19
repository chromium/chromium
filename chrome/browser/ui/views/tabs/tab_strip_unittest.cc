// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <string>

#include "base/bind.h"
#include "base/macros.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_group_id.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_animation.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/base/test/material_design_controller_test_api.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/accessibility/ax_event_manager.h"
#include "ui/views/accessibility/ax_event_observer.h"
#include "ui/views/accessibility/view_accessibility.h"
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

class TestAXEventObserver : public views::AXEventObserver {
 public:
  TestAXEventObserver() { views::AXEventManager::Get()->AddObserver(this); }

  ~TestAXEventObserver() override {
    views::AXEventManager::Get()->RemoveObserver(this);
  }

  // views::AXEventObserver:
  void OnViewEvent(views::View* view, ax::mojom::Event event_type) override {
    if (event_type == ax::mojom::Event::kSelectionRemove) {
      remove_count_++;
    }
    if (event_type == ax::mojom::Event::kSelection) {
      change_count_++;
    }
    if (event_type == ax::mojom::Event::kSelectionAdd) {
      add_count_++;
    }
  }

  int add_count() { return add_count_; }
  int change_count() { return change_count_; }
  int remove_count() { return remove_count_; }

 private:
  int add_count_ = 0;
  int change_count_ = 0;
  int remove_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(TestAXEventObserver);
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
  TabStripTest()
      : test_api_(GetParam()),
        animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {}

  ~TabStripTest() override {}

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ = new TabStrip(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);
    // Do this to force TabStrip to create the buttons.
    auto* parent = new views::View;
    parent->AddChildView(tab_strip_);

    widget_ = std::make_unique<views::Widget>();
    views::Widget::InitParams init_params =
        CreateParams(views::Widget::InitParams::TYPE_POPUP);
    init_params.ownership =
        views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;
    init_params.bounds = gfx::Rect(0, 0, 200, 200);
    widget_->Init(std::move(init_params));
    widget_->SetContentsView(parent);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
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

  void CompleteAnimationAndLayout() {
    tab_strip_->CompleteAnimationAndLayout();
  }

  int TabToNewTabButtonSpacing() {
    return tab_strip_->TabToNewTabButtonSpacing();
  }

  void AnimateToIdealBounds() { tab_strip_->AnimateToIdealBounds(); }

  const StackedTabStripLayout* touch_layout() const {
    return tab_strip_->touch_layout_.get();
  }

  views::BoundsAnimator* bounds_animator() {
    return &tab_strip_->bounds_animator_;
  }

  int GetActiveTabWidth() { return tab_strip_->GetActiveTabWidth(); }
  int GetInactiveTabWidth() { return tab_strip_->GetInactiveTabWidth(); }

  // End any outstanding drag and animate tabs back to their ideal bounds.
  void StopDragging(TabSlotView* view) {
    // Passing false for |is_first_view| results in running the post-drag
    // animation unconditionally.
    bool is_first_view = false;
    tab_strip_->StoppedDraggingView(view, &is_first_view);
  }

  // Makes sure that all tabs have the correct AX indices.
  void VerifyTabIndices() {
    for (int i = 0; i < tab_strip_->tab_count(); ++i) {
      ui::AXNodeData ax_node_data;
      tab_strip_->tab_at(i)->GetViewAccessibility().GetAccessibleNodeData(
          &ax_node_data);
      EXPECT_EQ(i + 1, ax_node_data.GetIntAttribute(
                           ax::mojom::IntAttribute::kPosInSet));
      EXPECT_EQ(
          tab_strip_->tab_count(),
          ax_node_data.GetIntAttribute(ax::mojom::IntAttribute::kSetSize));
    }
  }

  std::vector<TabGroupHeader*> ListGroupHeaders() const {
    std::vector<TabGroupHeader*> result;
    for (auto const& header_pair : tab_strip_->group_headers_)
      result.push_back(header_pair.second.get());
    return result;
  }

  std::vector<TabGroupUnderline*> ListGroupUnderlines() const {
    std::vector<TabGroupUnderline*> result;
    for (auto const& underline_pair : tab_strip_->group_underlines_)
      result.push_back(underline_pair.second.get());
    return result;
  }

  // Owned by TabStrip.
  FakeBaseTabStripController* controller_ = nullptr;
  TabStrip* tab_strip_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

  ui::MouseEvent dummy_event_ = ui::MouseEvent(ui::ET_MOUSE_PRESSED,
                                               gfx::PointF(),
                                               gfx::PointF(),
                                               base::TimeTicks::Now(),
                                               0,
                                               0);

 private:
  ui::test::MaterialDesignControllerTestAPI test_api_;
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;

  DISALLOW_COPY_AND_ASSIGN(TabStripTest);
};

TEST_P(TabStripTest, GetModelCount) {
  EXPECT_EQ(0, tab_strip_->GetModelCount());
}

TEST_P(TabStripTest, AccessibilityEvents) {
  TestAXEventObserver observer;

  // When adding tabs, SetSelection() is called after AddTabAt(), as
  // otherwise the index would not be meaningful.
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), true);
  ui::ListSelectionModel selection;
  selection.SetSelectedIndex(1);
  tab_strip_->SetSelection(selection);
  EXPECT_EQ(0, observer.add_count());
  EXPECT_EQ(1, observer.change_count());
  EXPECT_EQ(0, observer.remove_count());

  // When removing tabs, SetSelection() is called before RemoveTabAt(), as
  // otherwise the index would not be meaningful.
  selection.SetSelectedIndex(0);
  tab_strip_->SetSelection(selection);
  tab_strip_->RemoveTabAt(nullptr, 1, true);
  EXPECT_EQ(0, observer.add_count());
  EXPECT_EQ(2, observer.change_count());
  EXPECT_EQ(0, observer.remove_count());

  // When activating widget, refire selection event on tab.
  widget_->OnNativeWidgetActivationChanged(true);
  EXPECT_EQ(0, observer.add_count());
  EXPECT_EQ(3, observer.change_count());
  EXPECT_EQ(0, observer.remove_count());
}

TEST_P(TabStripTest, AccessibilityData) {
  // When adding tabs, indexes should be set.
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), true);
  VerifyTabIndices();

  tab_strip_->AddTabAt(0, TabRendererData(), false);
  VerifyTabIndices();

  tab_strip_->RemoveTabAt(nullptr, 1, false);
  VerifyTabIndices();

  tab_strip_->MoveTab(1, 0, TabRendererData());
  VerifyTabIndices();
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
  const size_t num_children = tab_strip_->children().size();
  EXPECT_EQ(2, tab_strip_->tab_count());
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
  // When removing a tab the tabcount should immediately decrement.
  EXPECT_EQ(1, tab_strip_->tab_count());
  // But the number of views should remain the same (it's animatining closed).
  EXPECT_EQ(num_children, tab_strip_->children().size());
  tab_strip_->SetBounds(0, 0, 200, 20);
  // Layout at a different size should force the animation to end and delete
  // the tab that was removed.
  tab_strip_->Layout();
  EXPECT_EQ(num_children - 1, tab_strip_->children().size());

  // Remove the last tab to make sure things are cleaned up correctly when
  // the TabStrip is destroyed and an animation is ongoing.
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
}

namespace {

bool TabViewsInOrder(TabStrip* tab_strip) {
  for (int i = 1; i < tab_strip->tab_count(); ++i) {
    Tab* left = tab_strip->tab_at(i - 1);
    Tab* right = tab_strip->tab_at(i);

    if (tab_strip->FindChild(right) < tab_strip->FindChild(left)) {
      return false;
    }
  }

  return true;
}

}  // namespace

// Verifies child view order matches model order.
TEST_P(TabStripTest, TabViewOrder) {
  controller_->AddTab(0, false);
  controller_->AddTab(1, false);
  controller_->AddTab(2, false);
  EXPECT_TRUE(TabViewsInOrder(tab_strip_));

  tab_strip_->MoveTab(0, 1, TabRendererData());
  EXPECT_TRUE(TabViewsInOrder(tab_strip_));
  tab_strip_->MoveTab(1, 2, TabRendererData());
  EXPECT_TRUE(TabViewsInOrder(tab_strip_));
  tab_strip_->MoveTab(1, 0, TabRendererData());
  EXPECT_TRUE(TabViewsInOrder(tab_strip_));
  tab_strip_->MoveTab(0, 2, TabRendererData());
  EXPECT_TRUE(TabViewsInOrder(tab_strip_));
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
    if (!tab_strip_->tab_at(invisible_tab_index)->GetVisible())
      break;
  }
  EXPECT_GT(invisible_tab_index, 0);
  EXPECT_LT(invisible_tab_index, 100);

  // The tabs before the invisible tab should still be visible.
  for (int i = 0; i < invisible_tab_index; ++i)
    EXPECT_TRUE(tab_strip_->tab_at(i)->GetVisible());

  // Enlarging the strip should result in the last tab becoming visible.
  tab_strip_->SetBounds(0, 0, kInitialWidth * 2, 20);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // Shrinking it again should re-hide the last tab.
  tab_strip_->SetBounds(0, 0, kInitialWidth, 20);
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // Shrinking it still more should make more tabs invisible, though not all.
  // All the invisible tabs should be at the end of the strip.
  tab_strip_->SetBounds(0, 0, kInitialWidth / 2, 20);
  int i = 0;
  for (; i < invisible_tab_index; ++i) {
    if (!tab_strip_->tab_at(i)->GetVisible())
      break;
  }
  ASSERT_GT(i, 0);
  EXPECT_LT(i, invisible_tab_index);
  invisible_tab_index = i;
  for (int i = invisible_tab_index + 1; i < tab_strip_->tab_count(); ++i)
    EXPECT_FALSE(tab_strip_->tab_at(i)->GetVisible());

  // When we're already in overflow, adding tabs at the beginning or end of
  // the strip should not change how many tabs are visible.
  controller_->AddTab(tab_strip_->tab_count(), false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->GetVisible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());
  controller_->AddTab(0, false);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->GetVisible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // If we remove enough tabs, all the tabs should be visible.
  for (int i = tab_strip_->tab_count() - 1; i >= invisible_tab_index; --i)
    controller_->RemoveTab(i);
  EXPECT_TRUE(tab_strip_->tab_at(tab_strip_->tab_count() - 1)->GetVisible());
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
  CompleteAnimationAndLayout();

  gfx::Point p;
  for (int y : {0, tab_strip_->height() / 2, tab_strip_->height() - 1}) {
    p.set_y(y);
    int previous_tab = -1;
    for (int x = 0; x < tab_strip_->width(); ++x) {
      p.set_x(x);
      int tab = tab_strip_->GetModelIndexOf(FindTabForEvent(p));
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
  tab_strip_->SelectTab(tab2, dummy_event_);
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
  EXPECT_TRUE(tab3->title_->GetVisible());

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
  tab_strip_->SelectTab(tab2, dummy_event_);
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
  CompleteAnimationAndLayout();
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

  CompleteAnimationAndLayout();

  EXPECT_LE(tab_strip_->new_tab_button_ideal_bounds().right(), kTabStripWidth);
}

TEST_P(TabStripTest, NewTabButtonRightOfTabs) {
  const int kTabStripWidth = 500;
  tab_strip_->SetBounds(0, 0, kTabStripWidth, 20);

  controller_->AddTab(0, true);

  AnimateToIdealBounds();

  EXPECT_EQ(tab_strip_->new_tab_button_ideal_bounds().x(),
            tab_strip_->ideal_bounds(0).right() + TabToNewTabButtonSpacing());
}

// The cached widths are private, but if they give incorrect results it can
// cause subtle errors in other tests. Therefore it's prudent to test them.
TEST_P(TabStripTest, CachedWidthsReportCorrectSize) {
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);
  controller_->AddTab(2, false);

  const int standard_width = TabStyle::GetStandardWidth();

  tab_strip_->SetBounds(0, 0, 1000, 100);

  EXPECT_EQ(standard_width, GetActiveTabWidth());
  EXPECT_EQ(standard_width, GetInactiveTabWidth());

  tab_strip_->SetBounds(0, 0, 240, 100);

  EXPECT_LT(GetActiveTabWidth(), standard_width);
  EXPECT_EQ(GetInactiveTabWidth(), GetActiveTabWidth());

  tab_strip_->SetBounds(0, 0, 50, 100);

  EXPECT_EQ(TabStyleViews::GetMinimumActiveWidth(), GetActiveTabWidth());
  EXPECT_EQ(TabStyleViews::GetMinimumInactiveWidth(), GetInactiveTabWidth());
}

// The active tab should always be at least as wide as its minimum width.
// http://crbug.com/587688
TEST_P(TabStripTest, ActiveTabWidthWhenTabsAreTiny) {
  // The bug was caused when it's animating. Therefore we should make widget
  // visible so that animation can be triggered.
  tab_strip_->GetWidget()->Show();
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyleViews::GetMinimumInactiveWidth();
  while (GetInactiveTabWidth() != min_inactive_width) {
    controller_->CreateNewTab();
    CompleteAnimationAndLayout();
  }

  EXPECT_GT(tab_strip_->tab_count(), 1);

  const int active_index = controller_->GetActiveIndex();
  EXPECT_EQ(tab_strip_->tab_count() - 1, active_index);
  EXPECT_LT(tab_strip_->ideal_bounds(0).width(),
            tab_strip_->ideal_bounds(active_index).width());

  // During mouse-based tab closure, the active tab should remain at least as
  // wide as it's minimum width.
  controller_->SelectTab(0, dummy_event_);
  while (tab_strip_->tab_count() > 0) {
    const int active_index = controller_->GetActiveIndex();
    EXPECT_GE(tab_strip_->ideal_bounds(active_index).width(),
              TabStyleViews::GetMinimumActiveWidth());
    tab_strip_->CloseTab(tab_strip_->tab_at(active_index),
                         CLOSE_TAB_FROM_MOUSE);
    CompleteAnimationAndLayout();
  }
}

// Inactive tabs shouldn't shrink during mouse-based tab closure.
// http://crbug.com/850190
TEST_P(TabStripTest, InactiveTabWidthWhenTabsAreTiny) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs smaller than active
  // tab but not the minimum.
  const int min_inactive_width = TabStyleViews::GetMinimumInactiveWidth();
  const int min_active_width = TabStyleViews::GetMinimumActiveWidth();
  while (GetInactiveTabWidth() >= (min_inactive_width + min_active_width) / 2) {
    controller_->CreateNewTab();
    CompleteAnimationAndLayout();
  }

  // During mouse-based tab closure, inactive tabs shouldn't shrink
  // so that users can close tabs continuously without moving mouse.
  controller_->SelectTab(0, dummy_event_);
  // If there are only two tabs in the strip, then after closing one the
  // remaining one will be active and there will be no inactive tabs,
  // so we stop at 2.
  while (tab_strip_->tab_count() > 2) {
    const int last_inactive_width = GetInactiveTabWidth();
    tab_strip_->CloseTab(tab_strip_->tab_at(controller_->GetActiveIndex()),
                         CLOSE_TAB_FROM_MOUSE);
    CompleteAnimationAndLayout();
    EXPECT_GE(GetInactiveTabWidth(), last_inactive_width);
  }
}

// When dragged tabs are moving back to their position, changes to ideal bounds
// should be respected. http://crbug.com/848016
TEST_P(TabStripTest, ResetBoundsForDraggedTabs) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyleViews::GetMinimumInactiveWidth();
  while (GetInactiveTabWidth() != min_inactive_width)
    controller_->CreateNewTab();

  const int min_active_width = TabStyleViews::GetMinimumActiveWidth();

  int dragged_tab_index = controller_->GetActiveIndex();
  EXPECT_GE(tab_strip_->ideal_bounds(dragged_tab_index).width(),
            min_active_width);

  // Mark the active tab as being dragged.
  Tab* dragged_tab = tab_strip_->tab_at(dragged_tab_index);
  dragged_tab->set_dragging(true);

  gfx::AnimationContainerTestApi test_api(bounds_animator()->container());

  // Ending the drag triggers the tabstrip to begin animating this tab back
  // to its ideal bounds.
  StopDragging(dragged_tab);
  EXPECT_TRUE(bounds_animator()->IsAnimating(dragged_tab));

  // Change the ideal bounds of the tabs mid-animation by selecting a
  // different tab.
  controller_->SelectTab(0, dummy_event_);

  // Once the animation completes, the dragged tab should have animated to
  // the new ideal bounds (computed with this as an inactive tab) rather
  // than the original ones (where it's an active tab).
  const base::TimeDelta duration = bounds_animator()->GetAnimationDuration();
  test_api.IncrementTime(duration);

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
  controller_->SelectTab(0, dummy_event_);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(1, dummy_event_);
  EXPECT_FALSE(IsShowingAttentionIndicator(tab1));
}

// The generic "wants attention" version should always show.
TEST_P(TabStripTest, TabNeedsAttentionGeneric) {
  controller_->AddTab(0, false);
  controller_->AddTab(1, true);

  Tab* tab1 = tab_strip_->tab_at(1);

  tab1->SetTabNeedsAttention(true);

  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(0, dummy_event_);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(1, dummy_event_);
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
    CompleteAnimationAndLayout();
    tab_strip_->new_tab_button()->AnimateInkDropToStateForTesting(
        views::InkDropState::HIDDEN);
  }
}

// Closing tab should be targeted during event dispatching.
TEST_P(TabStripTest, EventsOnClosingTab) {
  tab_strip_->SetBounds(0, 0, 200, 20);

  controller_->AddTab(0, false);
  controller_->AddTab(1, true);

  Tab* first_tab = tab_strip_->tab_at(0);
  gfx::Point tab_center = first_tab->bounds().CenterPoint();

  EXPECT_EQ(first_tab, tab_strip_->GetEventHandlerForPoint(tab_center));
  tab_strip_->CloseTab(first_tab, CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(first_tab, tab_strip_->GetEventHandlerForPoint(tab_center));
}

TEST_P(TabStripTest, AnimationOnTabAdd) {
  tab_strip_->SetBounds(0, 0, 1000, 100);
  // Show widget to enable animations.
  widget_->Show();

  // First tab does not animate.
  controller_->AddTab(0, true);
  controller_->AddTab(1, false);

  const int initial_width = tab_strip_->tab_at(1)->width();
  EXPECT_LT(initial_width, tab_strip_->tab_at(0)->width());

  task_environment_.FastForwardBy(TabAnimation::kAnimationDuration / 2);

  EXPECT_GT(tab_strip_->tab_at(1)->width(), initial_width);
  EXPECT_LT(tab_strip_->tab_at(1)->width(), tab_strip_->tab_at(0)->width());

  // Fast-forward by more than enough to ensure the animation finishes.
  task_environment_.FastForwardBy(TabAnimation::kAnimationDuration);

  EXPECT_EQ(tab_strip_->tab_at(0)->width(), tab_strip_->tab_at(1)->width());
}

TEST_P(TabStripTest, GroupHeaderBasics) {
  tab_strip_->SetBounds(0, 0, 1000, 100);
  bounds_animator()->SetAnimationDuration(base::TimeDelta());
  tab_strip_->AddTabAt(0, TabRendererData(), false);

  Tab* tab = tab_strip_->tab_at(0);
  const int first_slot_x = tab->x();

  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  CompleteAnimationAndLayout();

  std::vector<TabGroupHeader*> headers = ListGroupHeaders();
  EXPECT_EQ(1u, headers.size());
  TabGroupHeader* header = headers[0];
  EXPECT_EQ(first_slot_x, header->x());
  EXPECT_GT(header->width(), 0);
  EXPECT_EQ(header->bounds().right() - TabStyle::GetTabOverlap(), tab->x());
  EXPECT_EQ(tab->height(), header->height());
}

TEST_P(TabStripTest, GroupHeaderBetweenTabs) {
  tab_strip_->SetBounds(0, 0, 1000, 100);
  bounds_animator()->SetAnimationDuration(base::TimeDelta());

  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), false);

  const int second_slot_x = tab_strip_->tab_at(1)->x();

  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(1, group);

  TabGroupHeader* header = ListGroupHeaders()[0];
  EXPECT_EQ(header->x(), second_slot_x);
}

TEST_P(TabStripTest, GroupHeaderMovesRightWithTab) {
  tab_strip_->SetBounds(0, 0, 2000, 100);
  for (int i = 0; i < 4; i++)
    tab_strip_->AddTabAt(i, TabRendererData(), false);
  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(1, group);
  CompleteAnimationAndLayout();

  controller_->MoveTab(1, 2);
  CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupHeaders()[0];
  // Header is now left of tab 2.
  EXPECT_LT(tab_strip_->tab_at(1)->x(), header->x());
  EXPECT_LT(header->x(), tab_strip_->tab_at(2)->x());
}

TEST_P(TabStripTest, GroupHeaderMovesLeftWithTab) {
  tab_strip_->SetBounds(0, 0, 2000, 100);
  for (int i = 0; i < 4; i++)
    tab_strip_->AddTabAt(i, TabRendererData(), false);
  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(2, group);
  CompleteAnimationAndLayout();

  controller_->MoveTab(2, 1);
  CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupHeaders()[0];
  // Header is now left of tab 1.
  EXPECT_LT(tab_strip_->tab_at(0)->x(), header->x());
  EXPECT_LT(header->x(), tab_strip_->tab_at(1)->x());
}

TEST_P(TabStripTest, GroupHeaderDoesntMoveReorderingTabsInGroup) {
  tab_strip_->SetBounds(0, 0, 2000, 100);
  for (int i = 0; i < 4; i++)
    tab_strip_->AddTabAt(i, TabRendererData(), false);
  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(1, group);
  controller_->MoveTabIntoGroup(2, group);
  CompleteAnimationAndLayout();

  TabGroupHeader* header = ListGroupHeaders()[0];
  const int initial_header_x = header->x();
  Tab* tab1 = tab_strip_->tab_at(1);
  const int initial_tab_1_x = tab1->x();
  Tab* tab2 = tab_strip_->tab_at(2);
  const int initial_tab_2_x = tab2->x();

  controller_->MoveTab(1, 2);
  CompleteAnimationAndLayout();

  // Header has not moved.
  EXPECT_EQ(initial_header_x, header->x());
  EXPECT_EQ(initial_tab_1_x, tab2->x());
  EXPECT_EQ(initial_tab_2_x, tab1->x());
}

TEST_P(TabStripTest, GroupHeaderMovesOnRegrouping) {
  tab_strip_->SetBounds(0, 0, 2000, 100);
  for (int i = 0; i < 3; i++)
    tab_strip_->AddTabAt(i, TabRendererData(), false);
  TabGroupId group0 = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group0);
  TabGroupId group1 = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(1, group1);
  controller_->MoveTabIntoGroup(2, group1);
  CompleteAnimationAndLayout();

  std::vector<TabGroupHeader*> headers = ListGroupHeaders();
  auto header1_it = std::find_if(
      headers.begin(), headers.end(),
      [&group1](TabGroupHeader* header) { return header->group() == group1; });
  ASSERT_TRUE(header1_it != headers.end());
  TabGroupHeader* header1 = *header1_it;

  // Change groups in a way so that the header should swap with the tab, without
  // an explicit MoveTab call.
  controller_->MoveTabIntoGroup(1, group0);

  // Header is now right of tab 1.
  EXPECT_LT(tab_strip_->tab_at(1)->x(), header1->x());
  EXPECT_LT(header1->x(), tab_strip_->tab_at(2)->x());
}

TEST_P(TabStripTest, UngroupedTabMovesLeftOfHeader) {
  tab_strip_->SetBounds(0, 0, 2000, 100);
  for (int i = 0; i < 2; i++)
    tab_strip_->AddTabAt(i, TabRendererData(), false);
  TabGroupId group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  CompleteAnimationAndLayout();

  controller_->MoveTab(1, 0);
  CompleteAnimationAndLayout();

  // Header is right of tab 0.
  TabGroupHeader* header = ListGroupHeaders()[0];
  EXPECT_LT(tab_strip_->tab_at(0)->x(), header->x());
  EXPECT_LT(header->x(), tab_strip_->tab_at(1)->x());
}

// This can happen when a tab in the middle of a group starts to close.
TEST_P(TabStripTest, DiscontinuousGroup) {
  tab_strip_->SetBounds(0, 0, 1000, 100);
  bounds_animator()->SetAnimationDuration(base::TimeDelta());

  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), false);
  tab_strip_->AddTabAt(2, TabRendererData(), false);

  const int first_slot_x = tab_strip_->tab_at(0)->x();

  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  controller_->MoveTabIntoGroup(2, group);

  std::vector<TabGroupHeader*> headers = ListGroupHeaders();
  EXPECT_EQ(1u, headers.size());
  EXPECT_EQ(first_slot_x, headers[0]->x());
}

TEST_P(TabStripTest, DeleteTabGroupHeaderAndUnderlineWhenEmpty) {
  tab_strip_->AddTabAt(0, TabRendererData(), false);
  tab_strip_->AddTabAt(1, TabRendererData(), false);
  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  controller_->MoveTabIntoGroup(1, group);
  controller_->MoveTabIntoGroup(0, base::nullopt);

  EXPECT_EQ(1u, ListGroupHeaders().size());
  EXPECT_EQ(1u, ListGroupUnderlines().size());
  controller_->MoveTabIntoGroup(1, base::nullopt);
  EXPECT_EQ(0u, ListGroupHeaders().size());
  EXPECT_EQ(0u, ListGroupUnderlines().size());
}

TEST_P(TabStripTest, GroupUnderlineBasics) {
  tab_strip_->SetBounds(0, 0, 1000, 100);
  bounds_animator()->SetAnimationDuration(base::TimeDelta());
  controller_->AddTab(0, false);

  base::Optional<TabGroupId> group = TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  CompleteAnimationAndLayout();

  std::vector<TabGroupUnderline*> underlines = ListGroupUnderlines();
  EXPECT_EQ(1u, underlines.size());
  TabGroupUnderline* underline = underlines[0];
  // Update underline manually in the absence of a real Paint cycle.
  underline->UpdateBounds();

  EXPECT_EQ(underline->x(), TabGroupUnderline::GetStrokeInset());
  EXPECT_GT(underline->width(), 0);
  EXPECT_EQ(underline->bounds().right(),
            tab_strip_->tab_at(0)->bounds().right() -
                TabGroupUnderline::GetStrokeInset());
  EXPECT_EQ(underline->height(), TabGroupUnderline::kStrokeThickness);

  // Endpoints are different if the last grouped tab is active.
  controller_->AddTab(1, true);
  controller_->MoveTabIntoGroup(1, group);
  underline->UpdateBounds();

  EXPECT_EQ(underline->x(), TabGroupUnderline::GetStrokeInset());
  EXPECT_EQ(underline->bounds().right(),
            tab_strip_->tab_at(1)->bounds().right() +
                TabGroupUnderline::kStrokeThickness);
}

TEST_P(TabStripTest, ChangingLayoutTypeResizesTabs) {
  tab_strip_->SetBounds(0, 0, 1000, 100);

  tab_strip_->AddTabAt(0, TabRendererData(), false);
  Tab* tab = tab_strip_->tab_at(0);
  const int initial_height = tab->height();

  ui::test::MaterialDesignControllerTestAPI other_layout(!GetParam());

  CompleteAnimationAndLayout();
  if (GetParam()) {
    // Touch -> normal.
    EXPECT_LT(tab->height(), initial_height);
  } else {
    // Normal -> touch.
    EXPECT_GT(tab->height(), initial_height);
  }
}

INSTANTIATE_TEST_SUITE_P(, TabStripTest, ::testing::Values(false, true));
