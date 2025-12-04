// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/vertical/vertical_tab_strip_top_container.h"

#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/vertical_tab_strip_state_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/vertical_tab_strip_region_view.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/view_test_api.h"

class VerticalTabStripTopContainerTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest> {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeatures({tabs::kVerticalTabs}, {});
    VerticalTabsBrowserTestMixin::SetUp();
  }

  VerticalTabStripTopContainer* top_container() {
    return browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view()
        ->GetTopContainer();
  }

  views::LabelButton* tab_search_button() {
    return top_container()->GetTabSearchButton();
  }

  views::LabelButton* collapse_button() {
    return top_container()->GetCollapseButton();
  }

  void LayoutBrowserView() {
    // Force top level layout so the top container adjusts to any exclusion
    // zones.
    browser()
        ->GetBrowserView()
        .vertical_tab_strip_region_view()
        ->InvalidateLayout();
    BrowserFrameView* const frame_view =
        static_cast<BrowserFrameView*>(browser()
                                           ->GetBrowserView()
                                           .GetWidget()
                                           ->non_client_view()
                                           ->frame_view());
    views::ViewTestApi frame_view_test_api(frame_view);
    browser()->GetBrowserView().GetWidget()->LayoutRootViewIfNecessary();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerTest,
                       LayoutWithoutExclusionZone) {
  top_container()->SetExclusionWidthForLayout(0);
  top_container()->SetToolbarHeightForLayout(0);
  LayoutBrowserView();

  const gfx::Rect container_bounds = top_container()->GetLocalBounds();
  const gfx::Rect search_bounds = tab_search_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // The tab search button should be right aligned to the container and
  // vertically centered.
  EXPECT_EQ(search_bounds.top_right().x(), container_bounds.top_right().x());
  EXPECT_EQ(search_bounds.right_center().y(),
            container_bounds.right_center().y());

  // The collapse button should be to the left of the tab search button.
  EXPECT_LT(collapse_bounds.CenterPoint().x(), search_bounds.CenterPoint().x());
  EXPECT_EQ(collapse_bounds.CenterPoint().y(), search_bounds.CenterPoint().y());
}

IN_PROC_BROWSER_TEST_F(VerticalTabStripTopContainerTest,
                       LayoutWithFullWidthExclusionZone) {
  top_container()->SetExclusionWidthForLayout(0);
  top_container()->SetToolbarHeightForLayout(0);
  LayoutBrowserView();

  const gfx::Rect initial_search_bounds = tab_search_button()->bounds();
  const gfx::Rect initial_collapse_bounds = collapse_button()->bounds();

  const int top_container_width = top_container()->bounds().width();
  top_container()->SetExclusionWidthForLayout(top_container_width);
  constexpr int kExclusionHeight = 50;
  top_container()->SetToolbarHeightForLayout(kExclusionHeight);
  LayoutBrowserView();

  const gfx::Rect search_bounds = tab_search_button()->bounds();
  const gfx::Rect collapse_bounds = collapse_button()->bounds();

  // Both buttons are shifted down
  EXPECT_EQ(search_bounds.top_right().x(),
            initial_search_bounds.top_right().x());
  EXPECT_EQ(search_bounds.right_center().y(),
            initial_search_bounds.right_center().y() + kExclusionHeight);

  EXPECT_EQ(collapse_bounds.top_right().x(),
            initial_collapse_bounds.top_right().x());
  EXPECT_EQ(collapse_bounds.right_center().y(),
            initial_collapse_bounds.right_center().y() + kExclusionHeight);
}
