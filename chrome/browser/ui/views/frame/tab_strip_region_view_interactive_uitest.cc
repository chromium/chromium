// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/frame/window_frame_util.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/layout/flex_layout.h"

class TabStripRegionViewBrowserBaseTest : public InProcessBrowserTest {
 public:
  TabStripRegionViewBrowserBaseTest() = default;
  TabStripRegionViewBrowserBaseTest(const TabStripRegionViewBrowserBaseTest&) =
      delete;
  TabStripRegionViewBrowserBaseTest& operator=(
      const TabStripRegionViewBrowserBaseTest&) = delete;
  ~TabStripRegionViewBrowserBaseTest() override = default;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, false); }

  TabStripRegionView* tab_strip_region_view() {
    return views::AsViewClass<TabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStrip* tab_strip() { return tab_strip_region_view()->tab_strip(); }

  TabSearchContainer* tab_search_container() {
    return BrowserElementsViews::From(browser())->GetViewAs<TabSearchContainer>(
        kTabSearchContainerElementId);
  }

  TabSearchButton* tab_search_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<TabSearchButton>(
        kTabSearchButtonElementId);
  }

  views::View* new_tab_button() {
    return tab_strip_region_view()->GetNewTabButton();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class TabStripRegionViewBrowserTest : public TabStripRegionViewBrowserBaseTest {
 public:
  TabStripRegionViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{});
  }
  TabStripRegionViewBrowserTest(const TabStripRegionViewBrowserTest&) = delete;
  TabStripRegionViewBrowserTest& operator=(
      const TabStripRegionViewBrowserTest&) = delete;
  ~TabStripRegionViewBrowserTest() override = default;
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
    if (tab->showing_close_button_for_testing()) {
      press_right();
    }
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
  if (!features::HasTabSearchToolbarButton()) {
    EXPECT_TRUE(tab_search_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_0->HasFocus());
    EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
  }

  if (!features::HasTabSearchToolbarButton()) {
    // Focus should cycle back around to tab_0.
    press_right();
    EXPECT_TRUE(tab_0->HasFocus());
    EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
  }
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
    if (tab->showing_close_button_for_testing()) {
      press_left();
    }
    press_left();
  };

  // Request focus on the tab strip region view.
  tab_strip_region_view()->RequestFocus();
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());

  // The first tab should be active.
  EXPECT_TRUE(tab_0->HasFocus());

  // Pressing left should immediately cycle back around to the last button.
  press_left();
  if (features::HasTabSearchToolbarButton()) {
    EXPECT_TRUE(new_tab_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_search_button()->HasFocus());
  }

  press_left();
  if (!features::HasTabSearchToolbarButton()) {
    EXPECT_TRUE(new_tab_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_2->HasFocus());
  }

  if (!features::HasTabSearchToolbarButton()) {
    move_back_to_tab(tab_2);
    EXPECT_TRUE(tab_2->HasFocus());
  }

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

  if (!tabs::GetTabSearchTrailingTabstrip(browser()->profile()) &&
      !features::HasTabSearchToolbarButton()) {
    EXPECT_TRUE(tab_0->HasFocus());

#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->end_key()));
    EXPECT_TRUE(new_tab_button()->HasFocus());
#endif  // !BUILDFLAG(IS_WIN)

    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->home_key()));
    if (features::HasTabSearchToolbarButton()) {
      EXPECT_TRUE(new_tab_button()->HasFocus());
    } else {
      EXPECT_TRUE(tab_search_button()->HasFocus());
    }

  } else {
    // The first tab should be active.
    EXPECT_TRUE(tab_0->HasFocus());

#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->end_key()));
    if (features::HasTabSearchToolbarButton()) {
      EXPECT_TRUE(new_tab_button()->HasFocus());
    } else {
      EXPECT_TRUE(tab_search_button()->HasFocus());
    }
#endif  // !BUILDFLAG(IS_WIN)

    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->home_key()));
    EXPECT_TRUE(tab_0->HasFocus());
  }
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewBrowserTest,
                       DefaultTestSearchContainerIsEndAligned) {
  if (!features::HasTabSearchToolbarButton() &&
      !tabs::GetTabSearchTrailingTabstrip(browser()->profile())) {
    // The TabSearchContainer is calculated as controls padding away from the
    // first tab (not including bottom corner radius)
    const int tab_search_container_expected_end =
        tab_strip_region_view()->GetTabStripContainerForTesting()->x() +
        TabStyle::Get()->GetBottomCornerRadius() -
        GetLayoutConstant(TAB_STRIP_PADDING);

    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  } else if (!features::HasTabSearchToolbarButton()) {
    const int tab_search_container_expected_end =
        tab_strip_region_view()->GetLocalBounds().right() -
        GetLayoutConstant(TAB_STRIP_PADDING);
    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  }
}

class TabSearchForcedPositionTest : public TabStripRegionViewBrowserBaseTest,
                                    public testing::WithParamInterface<bool> {
 public:
  TabSearchForcedPositionTest() {
    const bool is_right_aligned = GetParam();

    std::vector<base::test::FeatureRef> enabled_features;
    enabled_features.push_back(tabs::kTabSearchPositionSetting);

    scoped_feature_list_.InitWithFeatures(
        enabled_features, std::vector<base::test::FeatureRef>{});
    tabs::SetTabSearchRightAlignedForTesting(is_right_aligned);
  }

  TabSearchForcedPositionTest(const TabSearchForcedPositionTest&) = delete;
  TabSearchForcedPositionTest& operator=(const TabSearchForcedPositionTest&) =
      delete;
  ~TabSearchForcedPositionTest() override = default;
};

IN_PROC_BROWSER_TEST_P(TabSearchForcedPositionTest,
                       DefaultTestSearchContainerIsEndAligned) {
  if (!features::HasTabSearchToolbarButton() &&
      !tabs::GetTabSearchTrailingTabstrip(browser()->profile())) {
    // The TabSearchContainer is calculated as controls padding away from the
    // first tab (not including bottom corner radius)
    const int tab_search_container_expected_end =
        tab_strip_region_view()->GetTabStripContainerForTesting()->x() +
        TabStyle::Get()->GetBottomCornerRadius() -
        GetLayoutConstant(TAB_STRIP_PADDING);

    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  } else if (!features::HasTabSearchToolbarButton()) {
    const int tab_search_container_expected_end =
        tab_strip_region_view()->GetLocalBounds().right() -
        GetLayoutConstant(TAB_STRIP_PADDING);
    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  }
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabSearchForcedPositionTest,
                         ::testing::Values(true, false));
