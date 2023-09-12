// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
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

  TabSearchContainer* tab_search_container() {
    return tab_strip_region_view()->tab_search_container();
  }

  TabSearchButton* tab_search_button() {
    return tab_search_container()->tab_search_button();
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

#if !BUILDFLAG(IS_WIN)
  press_right();
  EXPECT_TRUE(tab_search_button()->HasFocus());
#else
  if (features::IsChromeRefresh2023()) {
    press_right();
    EXPECT_TRUE(tab_search_button()->HasFocus());
  }
#endif

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
#if !BUILDFLAG(IS_WIN)
  press_left();
  EXPECT_TRUE(tab_search_button()->HasFocus());
#else
  if (features::IsChromeRefresh2023()) {
    press_left();
    EXPECT_TRUE(tab_search_button()->HasFocus());
  }
#endif

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

  if (TabSearchBubbleHost::ShouldTabSearchRenderBeforeTabStrip()) {
    EXPECT_TRUE(tab_0->HasFocus());

#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->end_key()));
    EXPECT_TRUE(new_tab_button()->HasFocus());
#endif  // !BUILDFLAG(IS_WIN)

    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->home_key()));
    EXPECT_TRUE(tab_search_button()->HasFocus());

  } else {
    // The first tab should be active.
    EXPECT_TRUE(tab_0->HasFocus());

#if !BUILDFLAG(IS_WIN)
  EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
      tab_strip_region_view()->end_key()));
  EXPECT_TRUE(tab_search_button()->HasFocus());
#endif  // !BUILDFLAG(IS_WIN)

  EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
      tab_strip_region_view()->home_key()));
  EXPECT_TRUE(tab_0->HasFocus());
  }
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest,
                       TestSearchContainerIsEndAligned) {
  if (WindowFrameUtil::IsWindowsTabSearchCaptionButtonEnabled(browser())) {
    EXPECT_EQ(tab_search_container(), nullptr);
  } else {
    if (TabSearchBubbleHost::ShouldTabSearchRenderBeforeTabStrip()) {
      // The TabSearchContainer is calculated as controls padding away from the
      // first tab (not including bottom corner radius)
      const int tab_search_container_expected_end =
          tab_strip_region_view()->GetTabStripContainerForTesting()->x() +
          TabStyle::Get()->GetBottomCornerRadius() -
          GetLayoutConstant(TAB_STRIP_PADDING);

      EXPECT_EQ(tab_search_container()->bounds().right(),
                tab_search_container_expected_end);
    } else {
      if (features::IsChromeRefresh2023()) {
        const int tab_search_container_expected_end =
            tab_strip_region_view()->GetLocalBounds().right() -
            GetLayoutConstant(TAB_STRIP_PADDING);
        EXPECT_EQ(tab_search_container()->bounds().right(),
                  tab_search_container_expected_end);
      } else {
        const int tab_search_container_expected_end =
            tab_strip_region_view()->GetLocalBounds().right() -
            GetLayoutConstant(TABSTRIP_REGION_VIEW_CONTROL_PADDING);
        EXPECT_EQ(tab_search_container()->bounds().right(),
                  tab_search_container_expected_end);
      }
    }
  }
}
