// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

class TabStripRegionViewBrowserTest : public InProcessBrowserTest {
 public:
  TabStripRegionViewBrowserTest() = default;
  TabStripRegionViewBrowserTest(const TabStripRegionViewBrowserTest&) = delete;
  TabStripRegionViewBrowserTest& operator=(
      const TabStripRegionViewBrowserTest&) = delete;
  ~TabStripRegionViewBrowserTest() override = default;

  void SetUp() override {
    base::test::ScopedFeatureList feature_list;
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kTabSearch,
                              features::kTabSearchFixedEntrypoint},
        /*disabled_features=*/{});
    InProcessBrowserTest::SetUp();
  }

 protected:
  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest,
                       TabSearchBubble_CreateAndClose_Fixed) {
  TabSearchButton* tab_search_button =
      browser_view()->tab_strip_region_view()->tab_search_button();

  DCHECK_EQ(nullptr, tab_search_button->bubble_for_testing());
  auto event = ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::PointF(),
                              gfx::PointF(), base::TimeTicks::Now(), 0, 0);
  tab_search_button->ButtonPressed(tab_search_button, event);
  DCHECK_NE(nullptr, tab_search_button->bubble_for_testing());

  // Close the tab search bubble widget, the bubble should be cleared from the
  // TabSearchButton.
  tab_search_button->bubble_for_testing()->CloseWithReason(
      views::Widget::ClosedReason::kUnspecified);
  DCHECK_EQ(nullptr, tab_search_button->bubble_for_testing());
}
