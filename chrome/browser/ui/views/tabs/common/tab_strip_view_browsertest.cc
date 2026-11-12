// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"

#include "base/auto_reset.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view_utils.h"

class TabStripViewBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabStripViewBrowserTest() = default;
  ~TabStripViewBrowserTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }

  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  views::LayoutOrientation layout_orientation() const {
    return is_horizontal() ? views::LayoutOrientation::kHorizontal
                           : views::LayoutOrientation::kVertical;
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

  TabStripView* tab_strip_view() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? views::AsViewClass<TabStripView>(
                                  base_region_view->GetTabStripView())
                            : nullptr;
  }

  views::ScrollView* pinned_tabs_scroll_view() {
    return tab_strip_view() ? tab_strip_view()->pinned_tabs_scroll_view()
                            : nullptr;
  }

  int GetMainAxisSize(const views::View* view) const {
    return views::GetMainAxis(layout_orientation(), view->size());
  }

  void EnableRichAnimations() {
    animation_mode_reset_.reset();
    animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
        gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED);
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       PinnedContainerDoesNotExceedHalfAvailableSpace) {
  // Add 15 pinned tabs and 4 unpinned tabs so that the pinned tabs prefer more
  // than 50% of the available tab strip space.
  for (int i = 0; i < 15; ++i) {
    AppendPinnedTab();
  }
  for (int i = 0; i < 4; ++i) {
    AppendTab();
  }

  ASSERT_EQ(tab_strip_model()->count(), 20);
  ASSERT_EQ(tab_strip_model()->IndexOfFirstNonPinnedTab(), 15);

  RunScheduledLayouts();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);

  // The pinned tabs scroll view must not exceed half of the tab strip's size
  // along the main layout axis.
  const int pinned_size = GetMainAxisSize(pinned_tabs_scroll_view());
  const int tab_strip_size = GetMainAxisSize(tab_strip_view());
  EXPECT_GT(pinned_size, 0);
  EXPECT_LE(pinned_size, tab_strip_size / 2);
}

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       PinnedContainerExpandsWhenUnpinnedDoesNotFillSpace) {
  // Pin the initial unpinned tab and append 14 more pinned tabs so there are 15
  // pinned tabs with no unpinned tabs present.
  tab_strip_model()->SetTabPinned(0, true);
  for (int i = 1; i < 15; ++i) {
    AppendPinnedTab();
  }

  ASSERT_EQ(tab_strip_model()->count(), 15);
  ASSERT_EQ(tab_strip_model()->IndexOfFirstNonPinnedTab(), 15);

  RunScheduledLayouts();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);

  // The pinned tabs scroll view should expand to occupy more than half of the
  // tab strip's main-axis size when unpinned tabs do not fill the space.
  EXPECT_GT(GetMainAxisSize(pinned_tabs_scroll_view()),
            GetMainAxisSize(tab_strip_view()) / 2);
}

IN_PROC_BROWSER_TEST_P(
    TabStripViewBrowserTest,
    SwitchingOrientationWithHoveredAndFocusedTabDoesNotCrash) {
  AppendPinnedTab();
  RunScheduledLayouts();

  ASSERT_NE(unpinned_collection_node(), nullptr);
  ASSERT_FALSE(unpinned_collection_node()->children().empty());
  views::View* tab_view = unpinned_collection_node()->children()[0]->view();
  ASSERT_NE(tab_view, nullptr);

  // Hover and focus the tab before switching orientation.
  ui::MouseEvent hover_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  tab_view->OnMouseEntered(hover_event);
  tab_view->RequestFocus();

  if (is_horizontal()) {
    EnterVerticalTabsMode();
  } else {
    ExitVerticalTabsMode();
  }

  EXPECT_NE(tab_strip_view(), nullptr);
}

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       RecordsVerticalTimeToSwitchOnlyInVerticalMode) {
  AppendTab();
  RunScheduledLayouts();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(unpinned_collection_node(), nullptr);
  ASSERT_GE(unpinned_collection_node()->children().size(), 2u);
  views::View* tab_view = unpinned_collection_node()->children()[0]->view();
  ASSERT_NE(tab_view, nullptr);

  base::HistogramTester histogram_tester;

  ui::MouseEvent enter_event(ui::EventType::kMouseEntered, gfx::Point(),
                             gfx::Point(), ui::EventTimeForNow(), 0, 0);
  tab_strip_view()->OnMouseEntered(enter_event);

  ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(5, 5),
                             gfx::Point(5, 5), ui::EventTimeForNow(),
                             ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  tab_view->OnMousePressed(press_event);
  ui::MouseEvent release_event(ui::EventType::kMouseReleased, gfx::Point(5, 5),
                               gfx::Point(5, 5), ui::EventTimeForNow(),
                               ui::EF_LEFT_MOUSE_BUTTON,
                               ui::EF_LEFT_MOUSE_BUTTON);
  tab_view->OnMouseReleased(release_event);

  histogram_tester.ExpectTotalCount("TabStrip.Vertical.TimeToSwitch",
                                    is_horizontal() ? 0 : 1);
  histogram_tester.ExpectTotalCount("TabStrip.TimeToSwitch",
                                    is_horizontal() ? 1 : 0);
}

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       ClosingLastPinnedTabAnimatesOut) {
  AppendPinnedTab();
  RunScheduledLayouts();

  EnableRichAnimations();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);
  PinnedTabContainerView* const pinned_container =
      tab_strip_view()->GetPinnedTabsContainer();
  ASSERT_NE(pinned_container, nullptr);
  ASSERT_EQ(pinned_container->children().size(), 1u);

  const int initial_pinned_size = GetMainAxisSize(pinned_tabs_scroll_view());
  EXPECT_GT(initial_pinned_size, 0);

  // Close the last (and only) pinned tab.
  tab_strip_model()->CloseWebContentsAt(0, TabCloseTypes::CLOSE_USER_GESTURE);
  RunScheduledLayouts();

  // The pinned container should animate out the closing tab rather than
  // snapping to 0 immediately and clipping or destroying the tab view.
  EXPECT_TRUE(
      TabCollectionAnimatingLayoutManager::IsViewAnimating(pinned_container));
  EXPECT_EQ(pinned_container->children().size(), 1u);
  EXPECT_EQ(GetMainAxisSize(pinned_tabs_scroll_view()), initial_pinned_size);

  // Once the animation completes, the pending-delete tab view should be removed
  // and the pinned scroll view should collapse to 0 along the main axis.
  vertical_tab_strip_controller()
      ->animation_coordinator()
      .StopAllAnimationsForTesting();
  RunScheduledLayouts();

  EXPECT_FALSE(
      TabCollectionAnimatingLayoutManager::IsViewAnimating(pinned_container));
  EXPECT_TRUE(pinned_container->children().empty());
  EXPECT_EQ(GetMainAxisSize(pinned_tabs_scroll_view()), 0);
}

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       ClosingLastPinnedSplitTabAnimatesOut) {
  AppendPinnedTab();
  AppendPinnedTab();
  tab_strip_model()->ActivateTabAt(
      0, TabStripUserGestureDetails(
             TabStripUserGestureDetails::GestureType::kOther));
  tab_strip_model()->AddToNewSplit(
      {1}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);
  RunScheduledLayouts();

  EnableRichAnimations();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);
  PinnedTabContainerView* const pinned_container =
      tab_strip_view()->GetPinnedTabsContainer();
  ASSERT_NE(pinned_container, nullptr);
  ASSERT_EQ(pinned_container->children().size(), 1u);

  const int initial_pinned_size = GetMainAxisSize(pinned_tabs_scroll_view());
  EXPECT_GT(initial_pinned_size, 0);

  // Close both tabs in the pinned split view together.
  tab_strip_model()->CloseSelectedTabs();
  RunScheduledLayouts();

  // The pinned container should animate out the closing split view rather than
  // snapping to 0 immediately.
  EXPECT_TRUE(
      TabCollectionAnimatingLayoutManager::IsViewAnimating(pinned_container));
  EXPECT_FALSE(pinned_container->children().empty());
  EXPECT_EQ(GetMainAxisSize(pinned_tabs_scroll_view()), initial_pinned_size);

  // Once the animation completes, the pending-delete views should be removed
  // and the pinned scroll view should collapse to 0 along the main axis.
  vertical_tab_strip_controller()
      ->animation_coordinator()
      .StopAllAnimationsForTesting();
  RunScheduledLayouts();

  EXPECT_FALSE(
      TabCollectionAnimatingLayoutManager::IsViewAnimating(pinned_container));
  EXPECT_TRUE(pinned_container->children().empty());
  EXPECT_EQ(GetMainAxisSize(pinned_tabs_scroll_view()), 0);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabStripViewBrowserTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });
