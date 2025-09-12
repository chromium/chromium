// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_pinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/vertical/vertical_unpinned_tab_container_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/controls/resize_area.h"
#include "ui/views/controls/separator.h"

class VerticalTabStripRegionViewTest : public InProcessBrowserTest {
 public:
  VerticalTabStripRegionViewTest() = default;
  ~VerticalTabStripRegionViewTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitAndEnableFeature(tabs::kVerticalTabs);
    InProcessBrowserTest::SetUp();
  }

  VerticalTabStripRegionView* region_view() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->vertical_tab_strip_region_view();
  }
  tabs::VerticalTabStripStateController* controller() {
    return browser()
        ->browser_window_features()
        ->vertical_tab_strip_state_controller();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripRegionViewTest,
                       SeparatorVisibilityChangesWithCollapsedState) {
  controller()->SetCollapsed(true);
  EXPECT_TRUE(controller()->IsCollapsed());
  EXPECT_TRUE(region_view()->tabs_separator_for_testing()->GetVisible());

  controller()->SetCollapsed(false);
  EXPECT_FALSE(controller()->IsCollapsed());
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
    std::unique_ptr<content::WebContents> contents =
        content::WebContents::Create(
            content::WebContents::CreateParams(browser()->profile()));
    content::WebContents* raw_contents = contents.get();
    browser()->tab_strip_model()->AppendWebContents(std::move(contents), true);
    const int index =
        browser()->tab_strip_model()->GetIndexOfWebContents(raw_contents);
    browser()->tab_strip_model()->SetTabPinned(index, true);
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
