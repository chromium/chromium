// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/timer/timer.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_group_header.h"
#include "chrome/browser/ui/views/tabs/tab_group_highlight.h"
#include "chrome/browser/ui/views/tabs/tab_group_underline.h"
#include "chrome/browser/ui/views/tabs/tab_group_views.h"
#include "chrome/browser/ui/views/tabs/tab_icon.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/tab_groups/tab_group_id.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/ax_event_counter.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_targeter.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

struct TabStripUnittestParams {
  const bool touch_ui;
  const bool scrolling_enabled;
};

constexpr TabStripUnittestParams kTabStripUnittestParams[] = {
    {false, true},
    {false, false},
    {true, false},
    {true, true},
};
}  // namespace

class TestTabStripObserver : public TabStripObserver {
 public:
  explicit TestTabStripObserver(TabStrip* tab_strip) : tab_strip_(tab_strip) {
    tab_strip_->SetTabStripObserver(this);
  }
  TestTabStripObserver(const TestTabStripObserver&) = delete;
  TestTabStripObserver& operator=(const TestTabStripObserver&) = delete;
  ~TestTabStripObserver() override { tab_strip_->SetTabStripObserver(nullptr); }

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

  raw_ptr<TabStrip> tab_strip_;
  int last_tab_added_ = -1;
  int last_tab_removed_ = -1;
  int last_tab_moved_from_ = -1;
  int last_tab_moved_to_ = -1;
};

// TabStripTestBase contains no test cases.
class TabStripTestBase : public ChromeViewsTestBase {
 public:
  TabStripTestBase(bool touch_ui, bool scrolling_enabled)
      : touch_ui_scoper_(touch_ui),
        animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    if (scrolling_enabled) {
      scoped_feature_list_.InitWithFeatures({tabs::kScrollableTabStrip}, {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {tabs::kScrollableTabStrip});
    }
  }
  TabStripTestBase(const TabStripTestBase&) = delete;
  TabStripTestBase& operator=(const TabStripTestBase&) = delete;
  ~TabStripTestBase() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    controller_ = new FakeBaseTabStripController;
    tab_strip_ = new TabStrip(std::unique_ptr<TabStripController>(controller_));
    controller_->set_tab_strip(tab_strip_);
    // Do this to force TabStrip to create the buttons.
    auto tab_strip_parent = std::make_unique<views::View>();
    views::FlexLayout* layout_manager = tab_strip_parent->SetLayoutManager(
        std::make_unique<views::FlexLayout>());
    // Scale the tabstrip between zero and its preferred width to match the
    // context it operates in in TabStripRegionView (with tab scrolling off).
    layout_manager->SetOrientation(views::LayoutOrientation::kHorizontal)
        .SetDefault(
            views::kFlexBehaviorKey,
            views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                     views::MaximumFlexSizeRule::kPreferred));
    tab_strip_parent->AddChildView(tab_strip_.get());
    // The tab strip is free to use all of the space in its parent view, since
    // there are no sibling controls such as the NTB in the test context.
    tab_strip_->SetAvailableWidthCallback(base::BindRepeating(
        [](views::View* tab_strip_parent) {
          return tab_strip_parent->size().width();
        },
        tab_strip_parent.get()));

    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
    tab_strip_parent_ = widget_->SetContentsView(std::move(tab_strip_parent));

    // Prevent hover cards from appearing when the mouse is over the tab. Tests
    // don't typically account for this possibly, so it can cause unrelated
    // tests to fail due to tab data not being set. See crbug.com/1050012.
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  void SetMaxTabStripWidth(int max_width) {
    tab_strip_parent_->SetBounds(0, 0, max_width,
                                 GetLayoutConstant(TAB_STRIP_HEIGHT));
    // Layout is handled from the Widget, so make sure it is also the correct
    // size.
    widget_->SetSize(tab_strip_parent_->bounds().size());
  }

  bool IsShowingAttentionIndicator(Tab* tab) {
    return tab->icon_->GetShowingAttentionIndicator();
  }

  void CompleteAnimationAndLayout() {
    // Complete animations and lay out *within the current tabstrip width*.
    tab_strip_->StopAnimating(true);
    // Resize the tabstrip based on the current tab states.
    views::test::RunScheduledLayout(tab_strip_parent_.get());
  }

  int GetActiveTabWidth() { return tab_strip_->GetActiveTabWidth(); }
  int GetInactiveTabWidth() { return tab_strip_->GetInactiveTabWidth(); }

  // End any outstanding drag and animate tabs back to their ideal bounds.
  void StopDragging() { tab_strip_->GetDragContext()->StoppedDragging(); }

  size_t NumTabSlotViews() {
    base::RepeatingCallback<size_t(views::View*)> num_tab_slot_views =
        base::BindLambdaForTesting([&num_tab_slot_views](views::View* parent) {
          if (views::IsViewClass<TabSlotView>(parent)) {
            return size_t(1);
          } else {
            size_t sum = 0;
            for (views::View* child : parent->children()) {
              sum += num_tab_slot_views.Run(child);
            }
            return sum;
          }
        });

    return num_tab_slot_views.Run(tab_strip_);
  }

  std::vector<TabGroupViews*> ListGroupViews() const {
    std::vector<TabGroupViews*> result;
    for (auto const& group_view_pair :
         tab_strip_->tab_container_->get_group_views_for_testing())
      result.push_back(group_view_pair.second.get());
    return result;
  }

  // Owned by TabStrip.
  raw_ptr<FakeBaseTabStripController, DanglingUntriaged> controller_ = nullptr;
  raw_ptr<TabStrip, DanglingUntriaged> tab_strip_ = nullptr;
  raw_ptr<views::View, DanglingUntriaged> tab_strip_parent_ = nullptr;
  std::unique_ptr<views::Widget> widget_;

  ui::MouseEvent dummy_event_ = ui::MouseEvent(ui::EventType::kMousePressed,
                                               gfx::PointF(),
                                               gfx::PointF(),
                                               base::TimeTicks::Now(),
                                               0,
                                               0);

 private:
  ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper_;
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TabStripTest contains tests that will run with all permutations of touch ui
// and scrolling enabled and disabled.
class TabStripTest
    : public TabStripTestBase,
      public testing::WithParamInterface<TabStripUnittestParams> {
 public:
  TabStripTest()
      : TabStripTestBase(GetParam().touch_ui, GetParam().scrolling_enabled) {}
  TabStripTest(const TabStripTest&) = delete;
  TabStripTest& operator=(const TabStripTest&) = delete;
  ~TabStripTest() override = default;
};

TEST_P(TabStripTest, GetModelCount) {
  EXPECT_EQ(0, tab_strip_->GetModelCount());
}

TEST_P(TabStripTest, AccessibilityEvents) {
  views::test::AXEventCounter ax_counter(views::AXEventManager::Get());

  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  Tab* tab = tab_strip_->tab_at(1);
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionAdd));
  EXPECT_EQ(1, ax_counter.GetCount(ax::mojom::Event::kSelection));
  ui::AXNodeData node_data;
  tab->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionRemove));

  tab = tab_strip_->tab_at(0);
  controller_->RemoveTab(1);
  node_data = ui::AXNodeData();
  tab->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionAdd));
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kSelection));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionRemove));

  // Before the Widget actcivation changes to true, it must be deactivated
  // first.
  widget_->OnNativeWidgetActivationChanged(false);
  node_data = ui::AXNodeData();
  tab->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionAdd));
  EXPECT_EQ(2, ax_counter.GetCount(ax::mojom::Event::kSelection));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionRemove));

  // When activating widget, refire selection event on tab.
  widget_->OnNativeWidgetActivationChanged(true);
  node_data = ui::AXNodeData();
  tab->GetViewAccessibility().GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.GetBoolAttribute(ax::mojom::BoolAttribute::kSelected));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionAdd));
  EXPECT_EQ(3, ax_counter.GetCount(ax::mojom::Event::kSelection));
  EXPECT_EQ(0, ax_counter.GetCount(ax::mojom::Event::kSelectionRemove));
}

TEST_P(TabStripTest, IsValidModelIndex) {
  EXPECT_FALSE(tab_strip_->IsValidModelIndex(0));
}

TEST_P(TabStripTest, tab_count) {
  EXPECT_EQ(0, tab_strip_->GetTabCount());
}

TEST_P(TabStripTest, AddTabAt) {
  TestTabStripObserver observer(tab_strip_);
  controller_->AddTab(0, TabActive::kInactive);
  ASSERT_EQ(1, tab_strip_->GetTabCount());
  EXPECT_EQ(0, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  EXPECT_FALSE(tab == nullptr);
}

TEST_P(TabStripTest, MoveTab) {
  TestTabStripObserver observer(tab_strip_);
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kInactive);
  controller_->AddTab(2, TabActive::kInactive);
  ASSERT_EQ(3, tab_strip_->GetTabCount());
  EXPECT_EQ(2, observer.last_tab_added());
  Tab* tab = tab_strip_->tab_at(0);
  controller_->MoveTab(0, 1);
  EXPECT_EQ(0, observer.last_tab_moved_from());
  EXPECT_EQ(1, observer.last_tab_moved_to());
  EXPECT_EQ(tab, tab_strip_->tab_at(1));
}

// Verifies child views are deleted after an animation completes.
TEST_P(TabStripTest, RemoveTab) {
  TestTabStripObserver observer(tab_strip_);
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kInactive);
  const size_t num_children = NumTabSlotViews();
  EXPECT_EQ(2, tab_strip_->GetTabCount());
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
  // When removing a tab the tabcount should immediately decrement.
  EXPECT_EQ(1, tab_strip_->GetTabCount());
  // But the number of views should remain the same (it's animatining closed).
  EXPECT_EQ(num_children, NumTabSlotViews());

  CompleteAnimationAndLayout();

  EXPECT_EQ(num_children - 1, NumTabSlotViews());

  // Remove the last tab to make sure things are cleaned up correctly when
  // the TabStrip is destroyed and an animation is ongoing.
  controller_->RemoveTab(0);
  EXPECT_EQ(0, observer.last_tab_removed());
}

// Tests that the tab close buttons of non-active tabs are hidden when
// the tab sizes are shrunk into small sizes.
TEST_P(TabStripTest, TabCloseButtonVisibility) {
  // Set the tab strip width to be wide enough for three tabs to show all
  // three icons, but not enough for five tabs to show all three icons.
  // Touch-optimized UI requires a larger width for tabs to show close buttons.
  const bool touch_ui = ui::TouchUiController::Get()->touch_ui();
  SetMaxTabStripWidth(touch_ui ? 442 : 346);

  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  controller_->AddTab(2, TabActive::kInactive);
  ASSERT_EQ(3, tab_strip_->GetTabCount());

  Tab* tab0 = tab_strip_->tab_at(0);
  ASSERT_FALSE(tab0->IsActive());
  Tab* tab1 = tab_strip_->tab_at(1);
  ASSERT_TRUE(tab1->IsActive());
  Tab* tab2 = tab_strip_->tab_at(2);
  ASSERT_FALSE(tab2->IsActive());

  // Ensure that all tab close buttons are initially visible.
  EXPECT_TRUE(tab0->showing_close_button_);
  EXPECT_TRUE(tab1->showing_close_button_);
  EXPECT_TRUE(tab2->showing_close_button_);

  // Shrink the tab sizes by adding more tabs.
  // An inactive tab added to the tabstrip, now each tab size is not
  // big enough to accomodate 3 icons, so it should not show its
  // tab close button.
  controller_->AddTab(3, TabActive::kInactive);
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
  controller_->AddTab(4, TabActive::kActive);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
TEST_P(TabStripTest, CloseButtonHiddenWhenLockedForOnTask) {
  controller_->SetLockedForOnTask(true);

  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  controller_->AddTab(2, TabActive::kInactive);
  ASSERT_EQ(3, tab_strip_->GetTabCount());

  Tab* const tab0 = tab_strip_->tab_at(0);
  ASSERT_FALSE(tab0->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);

  Tab* const tab1 = tab_strip_->tab_at(1);
  ASSERT_TRUE(tab1->IsActive());
  EXPECT_FALSE(tab1->showing_close_button_);

  Tab* tab2 = tab_strip_->tab_at(2);
  ASSERT_FALSE(tab2->IsActive());
  EXPECT_FALSE(tab2->showing_close_button_);

  // Switch tabs and confirm close button remains hidden for all opened tabs.
  tab_strip_->SelectTab(tab2, dummy_event_);
  ASSERT_TRUE(tab2->IsActive());
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
  EXPECT_FALSE(tab2->showing_close_button_);

  // Closing a tab should not alter tab close button visibility either.
  tab_strip_->CloseTab(tab2, CLOSE_TAB_FROM_MOUSE);
  tab2 = nullptr;
  EXPECT_FALSE(tab0->showing_close_button_);
  EXPECT_FALSE(tab1->showing_close_button_);
}
#endif

// The cached widths are private, but if they give incorrect results it can
// cause subtle errors in other tests. Therefore it's prudent to test them.
TEST_P(TabStripTest, CachedWidthsReportCorrectSize) {
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  controller_->AddTab(2, TabActive::kInactive);

  const int standard_width = TabStyle::Get()->GetStandardWidth();

  SetMaxTabStripWidth(1000);

  EXPECT_EQ(standard_width, GetActiveTabWidth());
  EXPECT_EQ(standard_width, GetInactiveTabWidth());

  SetMaxTabStripWidth(240);

  EXPECT_LT(GetActiveTabWidth(), standard_width);
  EXPECT_EQ(GetInactiveTabWidth(), GetActiveTabWidth());

  SetMaxTabStripWidth(50);

  EXPECT_EQ(TabStyle::Get()->GetMinimumActiveWidth(), GetActiveTabWidth());
  EXPECT_EQ(TabStyle::Get()->GetMinimumInactiveWidth(), GetInactiveTabWidth());
}

// The active tab should always be at least as wide as its minimum width.
// http://crbug.com/587688
TEST_P(TabStripTest, ActiveTabWidthWhenTabsAreTiny) {
  // The bug was caused when it's animating. Therefore we should make widget
  // visible so that animation can be triggered.
  tab_strip_->GetWidget()->Show();
  SetMaxTabStripWidth(400);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyle::Get()->GetMinimumInactiveWidth();
  while (GetInactiveTabWidth() != min_inactive_width) {
    controller_->CreateNewTab();
    CompleteAnimationAndLayout();
  }

  EXPECT_GT(tab_strip_->GetTabCount(), 1);

  int active_index = tab_strip_->GetActiveIndex().value();
  EXPECT_EQ(tab_strip_->GetTabCount() - 1, active_index);
  EXPECT_LT(tab_strip_->tab_at(0)->bounds().width(),
            tab_strip_->tab_at(active_index)->bounds().width());

  // During mouse-based tab closure, the active tab should remain at least as
  // wide as it's minimum width.
  controller_->SelectTab(0, dummy_event_);
  while (tab_strip_->GetTabCount() > 0) {
    active_index = tab_strip_->GetActiveIndex().value();
    EXPECT_GE(tab_strip_->tab_at(active_index)->bounds().width(),
              TabStyle::Get()->GetMinimumActiveWidth());
    tab_strip_->CloseTab(tab_strip_->tab_at(active_index),
                         CLOSE_TAB_FROM_MOUSE);
    CompleteAnimationAndLayout();
  }
}

// Inactive tabs shouldn't shrink during mouse-based tab closure.
// http://crbug.com/850190
TEST_P(TabStripTest, InactiveTabWidthWhenTabsAreTiny) {
  SetMaxTabStripWidth(200);

  // Create a lot of tabs in order to make inactive tabs smaller than active
  // tab but not the minimum.
  const int min_inactive_width = TabStyle::Get()->GetMinimumInactiveWidth();
  const int min_active_width = TabStyle::Get()->GetMinimumActiveWidth();
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
  while (tab_strip_->GetTabCount() > 2) {
    const int last_inactive_width = GetInactiveTabWidth();
    tab_strip_->CloseTab(
        tab_strip_->tab_at(tab_strip_->GetActiveIndex().value()),
        CLOSE_TAB_FROM_MOUSE);
    CompleteAnimationAndLayout();
    EXPECT_GE(GetInactiveTabWidth(), last_inactive_width);
  }
}

// When dragged tabs are moving back to their position, changes to ideal bounds
// should be respected. http://crbug.com/848016
TEST_P(TabStripTest, ResetBoundsForDraggedTabs) {
  SetMaxTabStripWidth(200);

  // Create a lot of tabs in order to make inactive tabs tiny.
  const int min_inactive_width = TabStyle::Get()->GetMinimumInactiveWidth();
  while (GetInactiveTabWidth() != min_inactive_width) {
    controller_->CreateNewTab();
    CompleteAnimationAndLayout();
  }

  const int min_active_width = TabStyle::Get()->GetMinimumActiveWidth();

  int dragged_tab_index = tab_strip_->GetActiveIndex().value();
  ASSERT_GE(tab_strip_->tab_at(dragged_tab_index)->bounds().width(),
            min_active_width);

  // Mark the active tab as being dragged.
  Tab* dragged_tab = tab_strip_->tab_at(dragged_tab_index);
  tab_strip_->GetDragContext()->StartedDragging({dragged_tab});

  // Ending the drag triggers the tabstrip to begin animating this tab back
  // to its ideal bounds.
  ASSERT_FALSE(tab_strip_->IsAnimating());
  StopDragging();
  EXPECT_TRUE(tab_strip_->IsAnimating());

  // Change the ideal bounds of the tabs mid-animation by selecting a
  // different tab.
  controller_->SelectTab(0, dummy_event_);

  // Once the animation completes, the dragged tab should have animated to
  // the new ideal bounds (computed with this as an inactive tab) rather
  // than the original ones (where it's an active tab).
  tab_strip_->StopAnimating(false);

  EXPECT_FALSE(dragged_tab->dragging());
  EXPECT_LT(dragged_tab->bounds().width(), min_active_width);
}

// The "blocked" attention indicator should only show for background tabs.
TEST_P(TabStripTest, TabNeedsAttentionBlocked) {
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);

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
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);

  Tab* tab1 = tab_strip_->tab_at(1);

  tab1->SetTabNeedsAttention(true);

  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(0, dummy_event_);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
  controller_->SelectTab(1, dummy_event_);
  EXPECT_TRUE(IsShowingAttentionIndicator(tab1));
}

// Closing tab should be targeted during event dispatching.
TEST_P(TabStripTest, EventsOnClosingTab) {
  SetMaxTabStripWidth(200);

  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  CompleteAnimationAndLayout();

  Tab* first_tab = tab_strip_->tab_at(0);
  Tab* second_tab = tab_strip_->tab_at(1);
  gfx::Point tab_center = first_tab->bounds().CenterPoint();

  EXPECT_EQ(first_tab, tab_strip_->GetEventHandlerForPoint(tab_center));
  tab_strip_->CloseTab(first_tab, CLOSE_TAB_FROM_MOUSE);
  EXPECT_EQ(first_tab, tab_strip_->GetEventHandlerForPoint(tab_center));

  // Closing |first_tab| again should forward to |second_tab| instead.
  tab_strip_->CloseTab(first_tab, CLOSE_TAB_FROM_MOUSE);
  EXPECT_TRUE(second_tab->closing());
}

// TODO (crbug.com/1520595): Disabled for now due to test failing when CR2023
// enabled.
TEST_P(TabStripTest, DISABLED_ChangingLayoutTypeResizesTabs) {
  SetMaxTabStripWidth(1000);

  controller_->AddTab(0, TabActive::kInactive);
  Tab* tab = tab_strip_->tab_at(0);
  const int initial_height = tab->height();

  ui::TouchUiController::TouchUiScoperForTesting other_layout(
      !GetParam().touch_ui);

  CompleteAnimationAndLayout();
  if (GetParam().touch_ui) {
    // Touch -> normal.
    EXPECT_LT(tab->height(), initial_height);
  } else {
    // Normal -> touch.
    EXPECT_GT(tab->height(), initial_height);
  }
}

// Regression test for a crash when closing a tab under certain conditions. If
// the first tab in a group was animating closed, attempting to close the next
// tab could result in a crash. This was due to TabStripLayoutHelper mistakenly
// mapping the next tab's model index to the closing tab's slot. See
// https://crbug.com/1138748 for a related crash.
TEST_P(TabStripTest, CloseTabInGroupWhilePreviousTabAnimatingClosed) {
  controller_->AddTab(0, TabActive::kActive);
  controller_->AddTab(1, TabActive::kInactive);
  controller_->AddTab(2, TabActive::kInactive);

  auto group_id = tab_groups::TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(1, group_id);
  controller_->MoveTabIntoGroup(2, group_id);

  CompleteAnimationAndLayout();
  ASSERT_EQ(3, tab_strip_->GetTabCount());
  ASSERT_EQ(3, tab_strip_->GetModelCount());
  EXPECT_EQ(std::nullopt, tab_strip_->tab_at(0)->group());
  EXPECT_EQ(group_id, tab_strip_->tab_at(1)->group());
  EXPECT_EQ(group_id, tab_strip_->tab_at(2)->group());

  // We have the following tabs:
  // 1. An ungrouped tab with model index 0
  // 2. A tab in |group_id| with model index 1
  // 3. A tab in |group_id| with model index 2
  controller_->RemoveTab(1);

  // After closing the first tab, we now have:
  // 1. An ungrouped tab with model index 0
  // 2. A closing tab in |group_id| with no model index
  // 3. A tab in |group_id| with model index 1.
  //
  // Closing the tab at model index 1 should result in (3) above being
  // closed.
  controller_->RemoveTab(1);

  // We should now have:
  // 1. An ungrouped tab with model index 0
  // 2. A closing tab in |group_id| with no model index
  // 3. A closing tab in |group_id| with no model index.

  CompleteAnimationAndLayout();

  // After finishing animations, there should be exactly 1 tab in no
  // group.
  EXPECT_EQ(1, tab_strip_->GetTabCount());
  EXPECT_EQ(std::nullopt, tab_strip_->tab_at(0)->group());
  EXPECT_EQ(1, tab_strip_->GetModelCount());
}

TEST_P(TabStripTest, HeaderOnCollapseChangeAccessibilityProperties) {
  controller_->AddTab(0, TabActive::kActive);

  std::optional<tab_groups::TabGroupId> group =
      tab_groups::TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group);
  CompleteAnimationAndLayout();

  ASSERT_FALSE(controller_->IsGroupCollapsed(group.value()));
  EXPECT_TRUE(tab_strip_->group_header(group.value())->GetVisible());

  // Initially the tab group is expanded
  ui::AXNodeData node_data;
  tab_strip_->group_header(group.value())
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));

  // Using controller to change the collapsed state of the tab group .
  controller_->ToggleTabGroupCollapsedState(
      group.value(), ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  tab_strip_->group_header(group.value())->VisualsChanged();

  node_data = ui::AXNodeData();
  tab_strip_->group_header(group.value())
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kCollapsed));

  controller_->ToggleTabGroupCollapsedState(
      group.value(), ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  tab_strip_->group_header(group.value())->VisualsChanged();

  node_data = ui::AXNodeData();
  tab_strip_->group_header(group.value())
      ->GetViewAccessibility()
      .GetAccessibleNodeData(&node_data);
  EXPECT_TRUE(node_data.HasState(ax::mojom::State::kExpanded));
  EXPECT_FALSE(node_data.HasState(ax::mojom::State::kCollapsed));
}

namespace {

struct SizeChangeObserver : public views::ViewObserver {
  explicit SizeChangeObserver(views::View* observed_view)
      : view(observed_view) {
    view->AddObserver(this);
  }
  ~SizeChangeObserver() override { view->RemoveObserver(this); }

  void OnViewPreferredSizeChanged(views::View* observed_view) override {
    size_change_count++;
  }

  const raw_ptr<views::View> view;
  int size_change_count = 0;
};

}  // namespace

// When dragged tabs' bounds are modified through TabDragContext, both tab strip
// and its parent view must get re-laid out http://crbug.com/1151092.
TEST_P(TabStripTest, RelayoutAfterDraggedTabBoundsUpdate) {
  SetMaxTabStripWidth(400);

  // Creates a single tab.
  controller_->CreateNewTab();
  CompleteAnimationAndLayout();

  int dragged_tab_index = tab_strip_->GetActiveIndex().value();
  Tab* dragged_tab = tab_strip_->tab_at(dragged_tab_index);
  ASSERT_TRUE(dragged_tab);

  // Mark the active tab as being dragged.
  dragged_tab->set_dragging(true);

  constexpr int kXOffset = 20;
  std::vector<raw_ptr<TabSlotView, VectorExperimental>> tabs{dragged_tab};
  std::vector<gfx::Rect> bounds{gfx::Rect({kXOffset, 0}, dragged_tab->size())};
  SizeChangeObserver view_observer(tab_strip_);
  tab_strip_->GetDragContext()->SetBoundsForDrag(tabs, bounds);
  EXPECT_EQ(1, view_observer.size_change_count);
}

TEST_P(TabStripTest, PreferredWidthDuringDrag) {
  // Start with two full-width tabs.
  controller_->AddTab(0, TabActive::kInactive);
  controller_->AddTab(1, TabActive::kActive);
  SetMaxTabStripWidth(1000);
  CompleteAnimationAndLayout();

  Tab* const dragged_tab = tab_strip_->tab_at(1);
  gfx::Rect dragged_tab_bounds = dragged_tab->bounds();
  const int original_preferred_width = tab_strip_->GetPreferredSize().width();

  // Drag the second tab Y to the right.
  tab_strip_->GetDragContext()->StartedDragging({dragged_tab});
  constexpr int kXOffset = 10;
  dragged_tab_bounds.Offset(kXOffset, 0);
  tab_strip_->GetDragContext()->SetBoundsForDrag({dragged_tab},
                                                 {dragged_tab_bounds});

  // Preferred width should be larger by Y.
  EXPECT_EQ(original_preferred_width + kXOffset,
            tab_strip_->GetPreferredSize().width());
}

TEST_P(TabStripTest, TabIconActiveState) {
  controller_->AddTab(0, TabActive::kActive);
  ASSERT_EQ(1, tab_strip_->GetTabCount());
  Tab* tab0 = tab_strip_->tab_at(0);
  EXPECT_TRUE(tab0->GetTabIconForTesting()->GetActiveStateForTesting());

  controller_->AddTab(1, TabActive::kActive);
  ASSERT_EQ(2, tab_strip_->GetTabCount());
  EXPECT_FALSE(tab0->GetTabIconForTesting()->GetActiveStateForTesting());

  controller_->SelectTab(0, dummy_event_);
  ASSERT_EQ(2, tab_strip_->GetTabCount());
  EXPECT_TRUE(tab0->GetTabIconForTesting()->GetActiveStateForTesting());
}

// TabStripTestWithScrollingDisabled contains tests that will run with scrolling
// disabled.
// TODO(http://crbug.com/951078) Remove these tests as well as tests in
// TabStripTest with scrolling disabled once tab scrolling is fully launched.
class TabStripTestWithScrollingDisabled
    : public TabStripTestBase,
      public testing::WithParamInterface<bool> {
 public:
  TabStripTestWithScrollingDisabled() : TabStripTestBase(GetParam(), false) {}
  TabStripTestWithScrollingDisabled(const TabStripTestWithScrollingDisabled&) =
      delete;
  TabStripTestWithScrollingDisabled& operator=(
      const TabStripTestWithScrollingDisabled&) = delete;
  ~TabStripTestWithScrollingDisabled() override = default;
};

TEST_P(TabStripTestWithScrollingDisabled, VisibilityInOverflow) {
  constexpr int kInitialWidth = 250;
  SetMaxTabStripWidth(kInitialWidth);

  // The first tab added to a reasonable-width strip should be visible.  If we
  // add enough additional tabs, eventually one should be invisible due to
  // overflow.
  int invisible_tab_index = 0;
  for (; invisible_tab_index < 100; ++invisible_tab_index) {
    controller_->AddTab(invisible_tab_index, TabActive::kInactive);
    CompleteAnimationAndLayout();
    if (!tab_strip_->tab_at(invisible_tab_index)->GetVisible())
      break;
  }
  EXPECT_GT(invisible_tab_index, 0);
  EXPECT_LT(invisible_tab_index, 100);

  // The tabs before the invisible tab should still be visible.
  for (int i = 0; i < invisible_tab_index; ++i)
    EXPECT_TRUE(tab_strip_->tab_at(i)->GetVisible());

  // Enlarging the strip should result in the last tab becoming visible.
  SetMaxTabStripWidth(kInitialWidth * 2);
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // Shrinking it again should re-hide the last tab.
  SetMaxTabStripWidth(kInitialWidth);
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // Shrinking it still more should make more tabs invisible, though not all.
  // All the invisible tabs should be at the end of the strip.
  SetMaxTabStripWidth(kInitialWidth / 2);
  int i = 0;
  for (; i < invisible_tab_index; ++i) {
    if (!tab_strip_->tab_at(i)->GetVisible())
      break;
  }
  ASSERT_GT(i, 0);
  EXPECT_LT(i, invisible_tab_index);
  invisible_tab_index = i;
  for (int j = invisible_tab_index + 1; j < tab_strip_->GetTabCount(); ++j)
    EXPECT_FALSE(tab_strip_->tab_at(j)->GetVisible());

  // When we're already in overflow, adding tabs at the beginning or end of
  // the strip should not change how many tabs are visible.
  controller_->AddTab(tab_strip_->GetTabCount(), TabActive::kInactive);
  CompleteAnimationAndLayout();
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->GetVisible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());
  controller_->AddTab(0, TabActive::kInactive);
  CompleteAnimationAndLayout();
  EXPECT_TRUE(tab_strip_->tab_at(invisible_tab_index - 1)->GetVisible());
  EXPECT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());

  // If we remove enough tabs, all the tabs should be visible.
  for (int j = tab_strip_->GetTabCount() - 1; j >= invisible_tab_index; --j)
    controller_->RemoveTab(j);
  CompleteAnimationAndLayout();
  EXPECT_TRUE(tab_strip_->tab_at(tab_strip_->GetTabCount() - 1)->GetVisible());
}

TEST_P(TabStripTestWithScrollingDisabled, GroupedTabSlotOverflowVisibility) {
  constexpr int kInitialWidth = 250;
  SetMaxTabStripWidth(kInitialWidth);

  // The first tab added to a reasonable-width strip should be visible.  If we
  // add enough additional tabs, eventually one should be invisible due to
  // overflow.
  int invisible_tab_index = 0;
  for (; invisible_tab_index < 100; ++invisible_tab_index) {
    controller_->AddTab(invisible_tab_index, TabActive::kInactive);
    CompleteAnimationAndLayout();
    if (!tab_strip_->tab_at(invisible_tab_index)->GetVisible())
      break;
  }
  ASSERT_GT(invisible_tab_index, 0);
  ASSERT_LT(invisible_tab_index, 100);

  // The tabs before the invisible tab should still be visible.
  for (int i = 0; i < invisible_tab_index; ++i)
    ASSERT_TRUE(tab_strip_->tab_at(i)->GetVisible());

  // The group header of an invisible tab should not be visible.
  std::optional<tab_groups::TabGroupId> group1 =
      tab_groups::TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(invisible_tab_index, group1);
  CompleteAnimationAndLayout();
  ASSERT_FALSE(tab_strip_->tab_at(invisible_tab_index)->GetVisible());
  EXPECT_FALSE(tab_strip_->group_header(group1.value())->GetVisible());

  // The group header of a visible tab should be visible when the group is
  // expanded and collapsed.
  std::optional<tab_groups::TabGroupId> group2 =
      tab_groups::TabGroupId::GenerateNew();
  controller_->MoveTabIntoGroup(0, group2);
  CompleteAnimationAndLayout();
  ASSERT_FALSE(controller_->IsGroupCollapsed(group2.value()));
  EXPECT_TRUE(tab_strip_->group_header(group2.value())->GetVisible());
  controller_->ToggleTabGroupCollapsedState(
      group2.value(), ToggleTabGroupCollapsedStateOrigin::kMenuAction);
  ASSERT_TRUE(controller_->IsGroupCollapsed(group2.value()));
  EXPECT_TRUE(tab_strip_->group_header(group2.value())->GetVisible());
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripTest,
                         ::testing::ValuesIn(kTabStripUnittestParams));

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripTestWithScrollingDisabled,
                         ::testing::Values(false, true));
