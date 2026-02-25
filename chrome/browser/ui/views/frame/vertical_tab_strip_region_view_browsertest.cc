// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/views/controls/button/button_controller.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"
#include "ui/views/focus/focus_manager.h"

class VerticalTabStripRegionViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  VerticalTabStripRegionView* region_view() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view_for_testing();
  }

  RootTabCollectionNode* root_node() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view_for_testing()
        ->root_node_for_testing();
  }

  tabs::VerticalTabStripStateController* state_controller() {
    return tabs::VerticalTabStripStateController::From(browser());
  }

  TabStrip* horizontal_tab_strip() {
    return browser()->GetBrowserView().horizontal_tab_strip_for_testing();
  }

 protected:
  // Appends a new tab to the end of the tab strip.
  content::WebContents* AppendTab() {
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    return raw_contents;
  }

  // Appends a new pinned tab to the end of the pinned tabs.
  content::WebContents* AppendPinnedTab() {
    content::WebContents* contents = AppendTab();
    const int index =
        browser()->tab_strip_model()->GetIndexOfWebContents(contents);
    browser()->tab_strip_model()->SetTabPinned(index, true);
    return contents;
  }

  // After changing the preferred width, wait for a layout to happen in order
  // for the bounds to match the preferred width.
  void WaitForBoundsToMatchPreferredWidth() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return region_view()->width() ==
             region_view()->GetPreferredSize().width();
    }));
  }

  void PressCollapseButton() {
    region_view()
        ->GetTopContainer()
        ->GetCollapseButton()
        ->button_controller()
        ->NotifyClick();
  }
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       SeparatorVisibilityChangesWithCollapsedState) {
  auto* tabs_separator = region_view()->tabs_separator_for_testing();

  state_controller()->SetCollapsed(true);
  EXPECT_TRUE(state_controller()->IsCollapsed());
  ui_test_utils::ViewVisibilityWaiter(tabs_separator, false).Wait();

  AppendPinnedTab();
  ui_test_utils::ViewVisibilityWaiter(tabs_separator, true).Wait();

  state_controller()->SetCollapsed(false);
  EXPECT_FALSE(state_controller()->IsCollapsed());
  ui_test_utils::ViewVisibilityWaiter(tabs_separator, false).Wait();
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, ResizeAreaBounds) {
  region_view()->SetBounds(0, 0, 200, 600);
  // Verify resize area is on the right side of the VerticalTabStripRegionView.
  EXPECT_EQ(region_view()->bounds().right(),
            region_view()->resize_area_for_testing()->bounds().right());
  // Verify resize area fills VerticalTabStripRegionView height.
  EXPECT_EQ(region_view()->bounds().height(),
            region_view()->resize_area_for_testing()->bounds().height());
  EXPECT_EQ(0, region_view()->resize_area_for_testing()->bounds().y());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, ResizeViewSmaller) {
  const int initial_width = tabs::kVerticalTabStripDefaultUncollapsedWidth;

  // Verify the initial state of the region view.
  ASSERT_EQ(initial_width, region_view()->GetPreferredSize().width());
  ASSERT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
  ASSERT_EQ(
      initial_width,
      region_view()->target_collapse_state_for_testing().uncollapsed_width);
  ASSERT_FALSE(region_view()->is_animating());
  ASSERT_FALSE(state_controller()->IsCollapsed());
  WaitForBoundsToMatchPreferredWidth();

  // Shrink the area a small amount and the preferred width will adjust
  // immediately.
  {
    const int resize_amount = -10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(resize_width, region_view()->GetPreferredSize().width());
    EXPECT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        resize_width,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_FALSE(region_view()->is_animating());
    EXPECT_FALSE(state_controller()->IsCollapsed());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Shrink the area beyond the minimum expanded width and the preferred width
  // will be that minimum width.
  {
    const int resize_amount = -120;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LT(VerticalTabStripRegionView::kCollapseSnapWidth, resize_width);
    ASSERT_LT(resize_width, VerticalTabStripRegionView::kUncollapsedMinWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(VerticalTabStripRegionView::kUncollapsedMinWidth,
              region_view()->GetPreferredSize().width());
    EXPECT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        VerticalTabStripRegionView::kUncollapsedMinWidth,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_FALSE(region_view()->is_animating());
    EXPECT_FALSE(state_controller()->IsCollapsed());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Shrink the area beyond the snap point and the tab strip will start
  // collapsing.
  {
    const int resize_amount = -180;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kCollapseSnapWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_TRUE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        VerticalTabStripRegionView::kUncollapsedMinWidth,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_TRUE(region_view()->is_animating());

    // Some time after the animation starts, the state controller collapsed
    // state will true.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return state_controller()->IsCollapsed(); }));

    // When the animation completes, the preferred width will be the collapsed
    // width.
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !region_view()->is_animating(); }));
    EXPECT_EQ(VerticalTabStripRegionView::kCollapsedWidth,
              region_view()->GetPreferredSize().width());
    WaitForBoundsToMatchPreferredWidth();
  }
}

// TODO(https://crbug.com/481074869): Re-enable this test
#if BUILDFLAG(IS_WIN) || BUILDFLAG(ENABLE_GLIC)
#define MAYBE_ResizeViewBigger DISABLED_ResizeViewBigger
#else
#define MAYBE_ResizeViewBigger ResizeViewBigger
#endif
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, MAYBE_ResizeViewBigger) {
  const int initial_width = VerticalTabStripRegionView::kCollapsedWidth;

  // Start this test from the collapsed state.
  state_controller()->SetCollapsed(true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !region_view()->is_animating(); }));
  WaitForBoundsToMatchPreferredWidth();

  // Verify the initial state of the region view.
  ASSERT_EQ(initial_width, region_view()->GetPreferredSize().width());
  ASSERT_TRUE(region_view()->target_collapse_state_for_testing().collapsed);
  ASSERT_EQ(
      tabs::kVerticalTabStripDefaultUncollapsedWidth,
      region_view()->target_collapse_state_for_testing().uncollapsed_width);
  ASSERT_FALSE(region_view()->is_animating());
  ASSERT_TRUE(state_controller()->IsCollapsed());

  // Grow the area a small amount and nothing will happen.
  {
    const int resize_amount = 10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kCollapseSnapWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(initial_width, region_view()->GetPreferredSize().width());
    EXPECT_TRUE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        tabs::kVerticalTabStripDefaultUncollapsedWidth,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_FALSE(region_view()->is_animating());
    EXPECT_TRUE(state_controller()->IsCollapsed());
    EXPECT_EQ(initial_width, region_view()->width());
  }

  // Grow the area beyond the snap point and tab strip will start expanding.
  {
    const int resize_amount = 50;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LT(VerticalTabStripRegionView::kCollapseSnapWidth, resize_width);
    ASSERT_LT(resize_width, VerticalTabStripRegionView::kUncollapsedMinWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        VerticalTabStripRegionView::kUncollapsedMinWidth,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_TRUE(region_view()->is_animating());

    // Some time after the animation starts, the state controller collapsed
    // state will become false.
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return !state_controller()->IsCollapsed(); }));

    // When the animation completes, the preferred width will be the minimum
    // expanded width.
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !region_view()->is_animating(); }));
    EXPECT_EQ(VerticalTabStripRegionView::kUncollapsedMinWidth,
              region_view()->GetPreferredSize().width());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Grow the area beyond the minimum expanded width and the preferred width
  // will adjust immediately.
  {
    const int resize_amount = 100;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kUncollapsedMaxWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(resize_width, region_view()->GetPreferredSize().width());
    EXPECT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        resize_width,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_FALSE(region_view()->is_animating());
    EXPECT_FALSE(state_controller()->IsCollapsed());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Grow the area beyond the maximum expanded width and the preferred width
  // will be that maximum width.
  {
    const int resize_amount = 500;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LT(VerticalTabStripRegionView::kUncollapsedMaxWidth, resize_width);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(VerticalTabStripRegionView::kUncollapsedMaxWidth,
              region_view()->GetPreferredSize().width());
    EXPECT_FALSE(region_view()->target_collapse_state_for_testing().collapsed);
    EXPECT_EQ(
        VerticalTabStripRegionView::kUncollapsedMaxWidth,
        region_view()->target_collapse_state_for_testing().uncollapsed_width);
    EXPECT_FALSE(region_view()->is_animating());
    EXPECT_FALSE(state_controller()->IsCollapsed());
    WaitForBoundsToMatchPreferredWidth();
  }
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       RestoreUncollapsedWidth) {
  const int initial_width = tabs::kVerticalTabStripDefaultUncollapsedWidth;

  // Verify the initial state of the region view.
  ASSERT_EQ(initial_width, region_view()->GetPreferredSize().width());
  ASSERT_EQ(initial_width, state_controller()->GetUncollapsedWidth());
  WaitForBoundsToMatchPreferredWidth();

  // Adjust the area without finishing resizing. The state controller's
  // uncollapsed width will not change.
  {
    const int resize_amount = 10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kUncollapsedMaxWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(resize_width, region_view()->GetPreferredSize().width());
    EXPECT_EQ(initial_width, state_controller()->GetUncollapsedWidth());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Adjust the area and finish resizing. The state controller's uncollapsed
  // width will update.
  {
    const int resize_amount = -10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kUncollapsedMaxWidth);

    region_view()->OnResize(resize_amount, true);
    EXPECT_EQ(resize_width, region_view()->GetPreferredSize().width());
    EXPECT_EQ(resize_width, state_controller()->GetUncollapsedWidth());
    WaitForBoundsToMatchPreferredWidth();
  }

  const int last_uncollapsed_width = state_controller()->GetUncollapsedWidth();

  // Collapse then expand the tab strip using the collapse button. The width
  // should be restored to the state controller's uncollapsed width.
  PressCollapseButton();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return region_view()->GetPreferredSize().width() ==
           VerticalTabStripRegionView::kCollapsedWidth;
  }));
  WaitForBoundsToMatchPreferredWidth();
  PressCollapseButton();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return region_view()->GetPreferredSize().width() == last_uncollapsed_width;
  }));
  WaitForBoundsToMatchPreferredWidth();

  // Shrink the area beyond the minimum expanded width without finishing
  // resizing. The state controller's uncollapsed width should not update.
  {
    const int resize_amount = -120;
    const int resize_width = last_uncollapsed_width + resize_amount;
    ASSERT_LT(VerticalTabStripRegionView::kCollapseSnapWidth, resize_width);
    ASSERT_LT(resize_width, VerticalTabStripRegionView::kUncollapsedMinWidth);

    region_view()->OnResize(resize_amount, false);
    EXPECT_EQ(VerticalTabStripRegionView::kUncollapsedMinWidth,
              region_view()->GetPreferredSize().width());
    EXPECT_EQ(last_uncollapsed_width,
              state_controller()->GetUncollapsedWidth());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Shrink the area beyond the snap point and finish resizing, so that the tab
  // strip is collapsed.
  {
    const int resize_amount = -180;
    const int resize_width = last_uncollapsed_width + resize_amount;
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kCollapseSnapWidth);

    region_view()->OnResize(resize_amount, true);
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return !region_view()->is_animating(); }));
    EXPECT_EQ(VerticalTabStripRegionView::kCollapsedWidth,
              region_view()->GetPreferredSize().width());
    EXPECT_EQ(last_uncollapsed_width,
              state_controller()->GetUncollapsedWidth());
    WaitForBoundsToMatchPreferredWidth();
  }

  // Expand the tab strip using the collapse button. The width will be restored
  // to what it was before the drag-to-collapse operation and not the minimum
  // expanded width.
  PressCollapseButton();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return region_view()->GetPreferredSize().width() == last_uncollapsed_width;
  }));
  WaitForBoundsToMatchPreferredWidth();
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, LogsResizeMetrics) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;
  const int initial_width = tabs::kVerticalTabStripDefaultUncollapsedWidth;

  ASSERT_EQ(initial_width, region_view()->GetPreferredSize().width());
  ASSERT_EQ(initial_width, state_controller()->GetUncollapsedWidth());
  WaitForBoundsToMatchPreferredWidth();
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "VerticalTabs_TabStrip_ResizeToCollapsed"));
  ASSERT_EQ(0, user_action_tester.GetActionCount(
                   "VerticalTabs_TabStrip_ResizeToUncollapsed"));
  ASSERT_EQ(0, histogram_tester.GetTotalSum("Tabs.VerticalTabs.TabStripSize"));

  // Adjust the area without finishing resizing. Nothing should be logged.
  {
    const int resize_amount = 10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kUncollapsedMaxWidth);

    region_view()->OnResize(resize_amount, false);
    WaitForBoundsToMatchPreferredWidth();
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToCollapsed"));
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToUncollapsed"));
    histogram_tester.ExpectTotalCount("Tabs.VerticalTabs.TabStripSize", 0);
  }

  // Adjust the area and finish resizing. The resize UMA and width histogram
  // will be logged.
  {
    const int resize_amount = -10;
    const int resize_width = initial_width + resize_amount;
    ASSERT_LE(VerticalTabStripRegionView::kUncollapsedMinWidth, resize_width);
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kUncollapsedMaxWidth);

    region_view()->OnResize(resize_amount, true);
    WaitForBoundsToMatchPreferredWidth();
    EXPECT_EQ(0, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToCollapsed"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToUncollapsed"));
    histogram_tester.ExpectTotalCount("Tabs.VerticalTabs.TabStripSize", 1);
    histogram_tester.ExpectBucketCount("Tabs.VerticalTabs.TabStripSize",
                                       resize_width, 1);
  }

  // Resize the tabstrip so that it is collapsed. The resize UMA and width
  // histogram will be logged.
  {
    const int resize_amount = -180;
    const int resize_width =
        region_view()->GetPreferredSize().width() + resize_amount;
    ASSERT_LE(resize_width, VerticalTabStripRegionView::kCollapseSnapWidth);

    region_view()->OnResize(resize_amount, true);
    WaitForBoundsToMatchPreferredWidth();
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToCollapsed"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "VerticalTabs_TabStrip_ResizeToUncollapsed"));
    histogram_tester.ExpectTotalCount("Tabs.VerticalTabs.TabStripSize", 2);
    histogram_tester.ExpectBucketCount(
        "Tabs.VerticalTabs.TabStripSize",
        VerticalTabStripRegionView::kCollapsedWidth, 1);
  }
}

// Verify that the pinned tabs container will never be larger than the unpinned
// tabs area.
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       PinnedTabsAreaSmallerThanUnpinned) {
  region_view()->SetBounds(0, 0, 200, 600);
  region_view()->GetPinnedTabsContainer()->SetPreferredSize(
      gfx::Size(100, 500));
  region_view()->GetUnpinnedTabsContainer()->SetPreferredSize(
      gfx::Size(100, 400));
  EXPECT_LE(region_view()->GetPinnedTabsContainer()->bounds().height(),
            region_view()->GetUnpinnedTabsContainer()->bounds().height());

  region_view()->GetUnpinnedTabsContainer()->SetPreferredSize(
      gfx::Size(100, 50));
  EXPECT_LE(region_view()->GetPinnedTabsContainer()->bounds().height(),
            region_view()->GetUnpinnedTabsContainer()->bounds().height());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       PinnedTabsStayWithinBoundingWidth) {
  // Add 10 pinned tabs.
  for (auto i = 0; i < 10; ++i) {
    AppendPinnedTab();
  }

  for (const auto child : region_view()->GetPinnedTabsContainer()->children()) {
    child->SetPreferredSize(gfx::Size(50, 60));
  }
  views::SizeBounds bounds;
  auto verify_for_width = [&](int width) {
    bounds.set_width(width);
    EXPECT_LE(region_view()
                  ->GetPinnedTabsContainer()
                  ->CalculateProposedLayout(bounds)
                  .host_size.width(),
              bounds.width());
  };
  // Test for a variety of bounding widths.
  verify_for_width(200);
  verify_for_width(205);
  verify_for_width(195);
  verify_for_width(140);
  verify_for_width(75);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       GetTabAnchorViewAtReturnsCorrectView) {
  // Add a few tabs.
  AppendTab();
  AppendTab();
  AppendTab();

  // Verify GetTabAnchorViewAt for a valid index.
  const int tab_index = 1;
  views::View* tab_anchor_view = region_view()->GetTabAnchorViewAt(tab_index);
  EXPECT_NE(nullptr, tab_anchor_view);

  // Get the tab from the model and its corresponding node view for comparison.
  tabs::TabInterface* tab =
      browser()->tab_strip_model()->GetTabAtIndex(tab_index);
  const TabCollectionNode* node =
      region_view()->root_node_for_testing()->GetNodeForHandle(
          tab->GetHandle());
  EXPECT_EQ(node->view(), tab_anchor_view);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       GetTabGroupAnchorViewReturnsCorrectView) {
  // Add a few tabs.
  AppendTab();
  AppendTab();
  AppendTab();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // Create a tab group.
  std::vector<int> tab_indices = {0, 1};
  std::optional<tab_groups::TabGroupId> group_id =
      tab_strip_model->AddToNewGroup(tab_indices);
  ASSERT_TRUE(group_id.has_value());

  // Verify GetTabGroupAnchorView for the created group.
  views::View* group_anchor_view =
      region_view()->GetTabGroupAnchorView(group_id.value());
  EXPECT_NE(nullptr, group_anchor_view);

  // Get the tab group from the model and its corresponding node view for
  // comparison.
  const TabGroup* tab_group =
      tab_strip_model->group_model()->GetTabGroup(group_id.value());
  const TabCollectionNode* node =
      region_view()->root_node_for_testing()->GetNodeForHandle(
          tab_group->GetCollectionHandle());
  EXPECT_EQ(node->view(), group_anchor_view);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       AccessiblePaneViewDefaultFocusableChild) {
  // Ensure the default focusable element is the VerticalTabStripTopContainer.
  views::View* view = region_view()->GetDefaultFocusableChild();
  ASSERT_TRUE(view);
  EXPECT_EQ(view, region_view()->GetTabAnchorViewAt(0));
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       CanFocusVerticalTabView) {
  // Add a tab.
  AppendTab();

  // Get the view for the first tab.
  views::View* first_tab_view = region_view()->GetTabAnchorViewAt(0);
  ASSERT_TRUE(first_tab_view);

  // Directly set focus using the FocusManager.
  views::FocusManager* focus_manager =
      BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);
  focus_manager->SetFocusedView(first_tab_view);

  // Verify that the first tab is focused. Setting focus is only synchronous if
  // the widget is active. Activating the widget is an asynchronous process so
  // wait until the view has focus.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return first_tab_view->HasFocus(); }));
  EXPECT_EQ(first_tab_view, focus_manager->GetFocusedView());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       GetFocusedTabIndexReturnsCorrectIndex) {
  // 1. Verify no tab is focused to start.
  EXPECT_EQ(std::nullopt, region_view()->GetFocusedTabIndex());

  // Add a few tabs.
  AppendTab();
  AppendTab();
  AppendTab();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();

  // 2. Activate the first tab and explicitly set the focus on the tab's view
  // using FocusManager.
  const int focused_tab_index = 1;
  tab_strip_model->ActivateTabAt(
      focused_tab_index, TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));

  views::View* tab_view = region_view()->GetTabAnchorViewAt(focused_tab_index);

  views::FocusManager* focus_manager =
      BrowserView::GetBrowserViewForBrowser(browser())->GetFocusManager();
  ASSERT_NE(nullptr, focus_manager);

  focus_manager->SetFocusedView(tab_view);
  ASSERT_TRUE(base::test::RunUntil([&]() { return tab_view->HasFocus(); }));

  std::optional<int> focused_index = region_view()->GetFocusedTabIndex();
  ASSERT_TRUE(focused_index.has_value());
  EXPECT_EQ(focused_tab_index, focused_index.value());

  // 3. Verify unfocusing the tab returns the correct value.
  focus_manager->SetFocusedView(nullptr);
  ASSERT_TRUE(base::test::RunUntil([&]() { return !tab_view->HasFocus(); }));

  EXPECT_EQ(std::nullopt, region_view()->GetFocusedTabIndex());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       SplitTabsShareSpace) {
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  // Add split tabs.
  content::WebContents* contents1 = AppendTab();
  content::WebContents* contents2 = AppendTab();

  const int index1 = tab_strip_model->GetIndexOfWebContents(contents1);
  const int index2 = tab_strip_model->GetIndexOfWebContents(contents2);

  tab_strip_model->ActivateTabAt(
      index1, TabStripUserGestureDetails(
                  TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model->AddToNewSplit(
      {index2}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);

  // Add pinned split tabs.
  content::WebContents* contents3 = AppendPinnedTab();
  content::WebContents* contents4 = AppendPinnedTab();

  const int index3 = tab_strip_model->GetIndexOfWebContents(contents3);
  const int index4 = tab_strip_model->GetIndexOfWebContents(contents4);

  tab_strip_model->ActivateTabAt(
      index3, TabStripUserGestureDetails(
                  TabStripUserGestureDetails::GestureType::kOther));

  tab_strip_model->AddToNewSplit(
      {index4}, {}, split_tabs::SplitTabCreatedSource::kTabContextMenu);

  auto* pinned_tabs = root_node()->children()[0]->view();
  EXPECT_TRUE(views::IsViewClass<VerticalPinnedTabContainerView>(pinned_tabs));
  EXPECT_EQ(pinned_tabs->children().size(), 1);
  auto* unpinned_tabs = root_node()->children()[1]->view();
  EXPECT_TRUE(
      views::IsViewClass<VerticalUnpinnedTabContainerView>(unpinned_tabs));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return unpinned_tabs->children().size() == 2; }));

  // Expect pinned tabs to have equal width.
  auto pinned_split_tab = pinned_tabs->children()[0];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(pinned_split_tab));
  EXPECT_EQ(pinned_split_tab->children().size(), 2);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return pinned_split_tab->children()[0]->size().width() ==
           pinned_split_tab->children()[1]->size().width();
  }));

  // Expect unpinned tabs to have equal width.
  auto unpinned_split_tab = unpinned_tabs->children()[1];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(unpinned_split_tab));
  EXPECT_EQ(unpinned_split_tab->children().size(), 2);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return unpinned_split_tab->children()[0]->size().width() ==
           unpinned_split_tab->children()[1]->size().width();
  }));
}

// Simulates swapping between horizontal and vertical modes. The inactive
// TabStrip should have no tabs.
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, SwitchModes) {
  EXPECT_TRUE(root_node());

  TabStripModel* model = browser()->tab_strip_model();

  // 1. Unpinned Tab
  // This tab is added by default for browser tests.

  // 2. Pinned Tab
  AppendPinnedTab();

  // 3. Split Tab (Unpinned)
  {
    content::WebContents* c1 = AppendTab();
    content::WebContents* c2 = AppendTab();
    int i1 = model->GetIndexOfWebContents(c1);
    int i2 = model->GetIndexOfWebContents(c2);
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  // 4. Pinned Split Tab
  {
    content::WebContents* c1 = AppendPinnedTab();
    content::WebContents* c2 = AppendPinnedTab();
    int i1 = model->GetIndexOfWebContents(c1);
    int i2 = model->GetIndexOfWebContents(c2);
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  // 5. Grouped Tab
  {
    content::WebContents* c = AppendTab();
    int i = model->GetIndexOfWebContents(c);
    model->AddToNewGroup({i});
  }

  // 6. Split within a Group
  {
    content::WebContents* c1 = AppendTab();
    content::WebContents* c2 = AppendTab();
    int i1 = model->GetIndexOfWebContents(c1);
    int i2 = model->GetIndexOfWebContents(c2);
    model->AddToNewGroup({i1, i2});
    model->ActivateTabAt(i1,
                         TabStripUserGestureDetails(
                             TabStripUserGestureDetails::GestureType::kOther));
    model->AddToNewSplit({i2}, {},
                         split_tabs::SplitTabCreatedSource::kTabContextMenu);
  }

  // Total tabs: 1 (pinned) + 1 (unpinned) + 2 (split) + 2 (pinned split) + 1
  // (grouped) + 2 (grouped split) = 9.
  auto* pinned_tabs_view = root_node()->children()[0]->view();
  auto* unpinned_tabs_view = root_node()->children()[1]->view();

  // Horizontal tabstrip should be empty when in vertical mode.
  EXPECT_EQ(horizontal_tab_strip()->GetTabCount(), 0);

  // Vertical tabstrip should have the tabs.
  // Pinned: 1 single + 1 split = 2 children.
  // Unpinned: 1 single + 1 split + 1 group + 1 group = 4 children.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return pinned_tabs_view->children().size() == 2 &&
           unpinned_tabs_view->children().size() == 4;
  }));

  ExitVerticalTabsMode();

  // Root node should be null after exiting vertical tabs mode.
  EXPECT_FALSE(root_node());

  // Horizontal tabstrip should have the tabs.
  // 1 pinned + 1 unpinned + 2 split + 2 pinned split + 1 grouped +
  // 2 grouped split = 10.
  EXPECT_EQ(horizontal_tab_strip()->GetTabCount(), 9);
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, TabStripEditableState) {
  // Default state should be editable.
  EXPECT_TRUE(region_view()->IsTabStripEditable());

  // Disable editing.
  region_view()->DisableTabStripEditingForTesting();
  EXPECT_FALSE(region_view()->IsTabStripEditable());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, TabStripCloseableState) {
  // Default state should be closeable (no drag session).
  EXPECT_TRUE(region_view()->IsTabStripCloseable());
}

// Verifies that entering Touch UI mode with vertical tabs enabled doesn't
// crash and correctly handles the vertical tab strip. This is a regression test
// for crbug.com/479887003.
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       NoCrashOnTouchUiModeChange) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  // Toggle Touch UI mode ON.
  {
    ui::TouchUiController::TouchUiScoperForTesting touch_ui_scoper(true);

    // Verify that the tab strip view is still the vertical one.
    // If it crashed, we won't reach here.
    TabStripRegionView* touch_view = browser_view->tab_strip_view();
    EXPECT_TRUE(views::IsViewClass<VerticalTabStripRegionView>(touch_view));
    EXPECT_EQ(region_view(), touch_view);
  }

  // Toggle Touch UI mode OFF (happens when touch_ui_scoper goes out of scope).

  // Verify it's still vertical.
  TabStripRegionView* final_view = browser_view->tab_strip_view();
  EXPECT_TRUE(views::IsViewClass<VerticalTabStripRegionView>(final_view));
  EXPECT_EQ(region_view(), final_view);
}
