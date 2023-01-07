// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/layout/flex_layout.h"

class TabStripRegionViewBrowserTest : public InProcessBrowserTest {
 public:
  TabStripRegionViewBrowserTest() = default;
  TabStripRegionViewBrowserTest(const TabStripRegionViewBrowserTest&) = delete;
  TabStripRegionViewBrowserTest& operator=(
      const TabStripRegionViewBrowserTest&) = delete;
  ~TabStripRegionViewBrowserTest() override = default;

  void SetUp() override {
    // Ensure we run our tests with the tab search button placement configured
    // for the tab strip region view.
#if BUILDFLAG(IS_CHROMEOS)
    scoped_feature_list_.InitAndDisableFeature(
        features::kChromeOSTabSearchCaptionButton);
#endif

#if BUILDFLAG(IS_WIN)
    scoped_feature_list_.InitAndDisableFeature(
        features::kWin10TabSearchCaptionButton);
#endif
    InProcessBrowserTest::SetUp();
  }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, false); }

  BrowserView* browser_view() {
    return BrowserView::GetBrowserViewForBrowser(browser());
  }

  TabStripRegionView* tab_strip_region_view() {
    return browser_view()->tab_strip_region_view();
  }

  TabStrip* tab_strip() { return browser_view()->tabstrip(); }

  TabSearchButton* tab_search_button() {
    return tab_strip_region_view()->tab_search_button();
  }

  views::View* new_tab_button() {
    return tab_strip_region_view()->new_tab_button();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest, TestForwardFocus) {
  AppendTab();
  AppendTab();
  Tab* tab_0 = tab_strip()->tab_at(0);
  Tab* tab_1 = tab_strip()->tab_at(1);
  Tab* tab_2 = tab_strip()->tab_at(2);

  const auto press_right = [&]() {
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->right_key()));
  };
  const auto move_forward_over_tab = [&](Tab* tab) {
    // When skipping over tabs two right presses are needed if the close button
    // is showing.
    if (tab->showing_close_button_for_testing())
      press_right();
    press_right();
  };

  // Request focus on the tab strip region view.
  tab_strip_region_view()->RequestFocus();
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());

  // The first tab should be active.
  EXPECT_TRUE(tab_0->HasFocus());

  move_forward_over_tab(tab_0);
  EXPECT_TRUE(tab_1->HasFocus());

  move_forward_over_tab(tab_1);
  EXPECT_TRUE(tab_2->HasFocus());

  move_forward_over_tab(tab_2);
  EXPECT_TRUE(new_tab_button()->HasFocus());

  press_right();
  EXPECT_TRUE(tab_search_button()->HasFocus());

  // Focus should cycle back around to tab_0.
  press_right();
  EXPECT_TRUE(tab_0->HasFocus());
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest, TestReverseFocus) {
  AppendTab();
  AppendTab();
  Tab* tab_0 = tab_strip()->tab_at(0);
  Tab* tab_1 = tab_strip()->tab_at(1);
  Tab* tab_2 = tab_strip()->tab_at(2);

  const auto press_left = [&]() {
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->left_key()));
  };
  const auto move_back_to_tab = [&](Tab* tab) {
    // When skipping back to the previous tab two left presses are needed if the
    // close button is showing.
    if (tab->showing_close_button_for_testing())
      press_left();
    press_left();
  };

  // Request focus on the tab strip region view.
  tab_strip_region_view()->RequestFocus();
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());

  // The first tab should be active.
  EXPECT_TRUE(tab_0->HasFocus());

  // Pressing left should immediately cycle back around to the last button.
  press_left();
  EXPECT_TRUE(tab_search_button()->HasFocus());
  press_left();
  EXPECT_TRUE(new_tab_button()->HasFocus());

  move_back_to_tab(tab_2);
  EXPECT_TRUE(tab_2->HasFocus());

  move_back_to_tab(tab_1);
  EXPECT_TRUE(tab_1->HasFocus());

  move_back_to_tab(tab_0);
  EXPECT_TRUE(tab_0->HasFocus());
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest, TestBeginEndFocus) {
  AppendTab();
  AppendTab();
  Tab* tab_0 = tab_strip()->tab_at(0);
  tab_strip()->tab_at(1);
  tab_strip()->tab_at(2);

  // Request focus on the tab strip region view.
  tab_strip_region_view()->RequestFocus();
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());

  // The first tab should be active.
  EXPECT_TRUE(tab_0->HasFocus());

  EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
      tab_strip_region_view()->end_key()));
  EXPECT_TRUE(tab_search_button()->HasFocus());

  EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
      tab_strip_region_view()->home_key()));
  EXPECT_TRUE(tab_0->HasFocus());
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest,
                       TestSearchButtonIsEndAligned) {
  const int kRightMargin =
      GetLayoutConstant(TABSTRIP_REGION_VIEW_CONTROL_PADDING);
  EXPECT_EQ(tab_strip_region_view()->GetLocalBounds().right() - kRightMargin,
            tab_search_button()->bounds().right());
}
