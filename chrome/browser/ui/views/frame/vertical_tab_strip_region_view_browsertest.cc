// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

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
#include "chrome/browser/ui/views/tabs/vertical/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_split_tab_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_group.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"

class VerticalTabStripRegionViewTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  VerticalTabStripRegionView* region_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->vertical_tab_strip_region_view();
  }

  tabs::VerticalTabStripStateController* state_controller() {
    return tabs::VerticalTabStripStateController::From(browser());
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
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       SeparatorVisibilityChangesWithCollapsedState) {
  state_controller()->SetCollapsed(true);
  EXPECT_TRUE(state_controller()->IsCollapsed());
  EXPECT_TRUE(region_view()->tabs_separator_for_testing()->GetVisible());

  state_controller()->SetCollapsed(false);
  EXPECT_FALSE(state_controller()->IsCollapsed());
  EXPECT_FALSE(region_view()->tabs_separator_for_testing()->GetVisible());
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
  // Verify resize area width.
  EXPECT_EQ(VerticalTabStripRegionView::kResizeAreaWidth,
            region_view()->resize_area_for_testing()->bounds().width());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, ResizeViewMinWidth) {
  region_view()->SetBounds(0, 0, 200, 600);
  // Verify the initial bounds of the region view.
  EXPECT_EQ(200, region_view()->bounds().width());

  // Shrink the area a small amount and expect the preferred width to adjust.
  region_view()->OnResize(-10, false);
  EXPECT_EQ(200 - 10, region_view()->GetPreferredSize().width());

  // Shrink the area beyond the min width and the preferred width will be the
  // minimum width.
  region_view()->OnResize(-200, false);
  EXPECT_EQ(VerticalTabStripRegionView::kExpandedMinWidth,
            region_view()->GetPreferredSize().width());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest, ResizeViewMaxWidth) {
  region_view()->SetBounds(0, 0, 200, 600);
  // Verify the initial bounds of the region view.
  EXPECT_EQ(200, region_view()->bounds().width());

  // Grow the area a small amount and expect the preferred width to adjust.
  region_view()->OnResize(10, false);
  EXPECT_EQ(200 + 10, region_view()->GetPreferredSize().width());

  // Grow the area beyond the max width and the preferred width will be the
  // maximum width.
  region_view()->OnResize(1000, false);
  EXPECT_EQ(VerticalTabStripRegionView::kExpandedMaxWidth,
            region_view()->GetPreferredSize().width());
}

// Verify that the pinned tabs container will never be larger than the unpinned
// tabs area.
IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       PinnedTabsAreaSmallerThanUnpinned) {
  region_view()->SetBounds(0, 0, 200, 600);
  region_view()->pinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 500));
  region_view()->unpinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 400));
  EXPECT_LE(
      region_view()->pinned_tabs_container_for_testing()->bounds().height(),
      region_view()->unpinned_tabs_container_for_testing()->bounds().height());

  region_view()->unpinned_tabs_container_for_testing()->SetPreferredSize(
      gfx::Size(100, 50));
  EXPECT_LE(
      region_view()->pinned_tabs_container_for_testing()->bounds().height(),
      region_view()->unpinned_tabs_container_for_testing()->bounds().height());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       PinnedTabsStayWithinBoundingWidth) {
  // Add 10 pinned tabs.
  for (auto i = 0; i < 10; ++i) {
    AppendPinnedTab();
  }

  for (const auto child :
       region_view()->pinned_tabs_container_for_testing()->children()) {
    child->SetPreferredSize(gfx::Size(50, 60));
  }
  views::SizeBounds bounds;
  auto verify_for_width = [&](int width) {
    bounds.set_width(width);
    EXPECT_LE(region_view()
                  ->pinned_tabs_container_for_testing()
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

  // Create view hierarchy from an arbitrary parent view since we don't
  // currently support updates from the API.
  auto parent_view = std::make_unique<views::View>();
  parent_view->SetBounds(0, 0, 200, 600);
  RootTabCollectionNode root_node(
      browser()->tab_strip_model(),
      base::BindRepeating<TabCollectionNode::CustomAddChildView>(
          &views::View::AddChildView, base::Unretained(parent_view.get())));

  auto* pinned_tabs = root_node.children()[0]->get_view_for_testing();
  EXPECT_TRUE(views::IsViewClass<VerticalPinnedTabContainerView>(pinned_tabs));
  EXPECT_EQ(pinned_tabs->children().size(), 1);
  auto* unpinned_tabs = root_node.children()[1]->get_view_for_testing();
  EXPECT_TRUE(
      views::IsViewClass<VerticalUnpinnedTabContainerView>(unpinned_tabs));
  EXPECT_EQ(unpinned_tabs->children().size(), 2);

  // Expect pinned tabs to have equal width.
  auto pinned_split_tab = pinned_tabs->children()[0];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(pinned_split_tab));
  EXPECT_EQ(pinned_split_tab->children().size(), 2);
  EXPECT_EQ(pinned_split_tab->children()[0]->size().width(),
            pinned_split_tab->children()[1]->size().width());

  // Expect unpinned tabs to have equal width.
  auto unpinned_split_tab = unpinned_tabs->children()[1];
  EXPECT_TRUE(views::IsViewClass<VerticalSplitTabView>(unpinned_split_tab));
  EXPECT_EQ(unpinned_split_tab->children().size(), 2);
  EXPECT_EQ(unpinned_split_tab->children()[0]->size().width(),
            unpinned_split_tab->children()[1]->size().width());
}
