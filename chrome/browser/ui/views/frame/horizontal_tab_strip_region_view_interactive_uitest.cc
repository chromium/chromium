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
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_search_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_features.h"
#include "ui/events/event_constants.h"
#include "ui/views/layout/flex_layout.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/constants/chromeos_features.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

class HorizontalTabStripRegionViewBrowserBaseTest : public InProcessBrowserTest {
 public:
  HorizontalTabStripRegionViewBrowserBaseTest() = default;
  HorizontalTabStripRegionViewBrowserBaseTest(const HorizontalTabStripRegionViewBrowserBaseTest&) =
      delete;
  HorizontalTabStripRegionViewBrowserBaseTest& operator=(
      const HorizontalTabStripRegionViewBrowserBaseTest&) = delete;
  ~HorizontalTabStripRegionViewBrowserBaseTest() override = default;

  void SetUp() override { InProcessBrowserTest::SetUp(); }

  void AppendTab() { chrome::AddTabAt(browser(), GURL(), -1, false); }

  HorizontalTabStripRegionView* tab_strip_region_view() {
    return views::AsViewClass<HorizontalTabStripRegionView>(
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
    return tab_strip_region_view()->new_tab_button_for_testing();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class HorizontalTabStripRegionViewBrowserTest : public HorizontalTabStripRegionViewBrowserBaseTest {
 public:
  HorizontalTabStripRegionViewBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kGlicRollout,
#if BUILDFLAG(IS_CHROMEOS)
                              chromeos::features::kFeatureManagementGlic
#endif  // BUILDFLAG(IS_CHROMEOS)
        },
        /*disabled_features=*/{features::kGlicLocaleFiltering,
                               features::kGlicCountryFiltering});
  }
  HorizontalTabStripRegionViewBrowserTest(const HorizontalTabStripRegionViewBrowserTest&) = delete;
  HorizontalTabStripRegionViewBrowserTest& operator=(
      const HorizontalTabStripRegionViewBrowserTest&) = delete;
  ~HorizontalTabStripRegionViewBrowserTest() override = default;
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest, TestForwardFocus) {
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
  if (tabs::GetTabSearchPosition(browser()) !=
      tabs::TabSearchPosition::kToolbarButton) {
    EXPECT_TRUE(tab_search_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_0->HasFocus());
    EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
  }

  if (tabs::GetTabSearchPosition(browser()) !=
      tabs::TabSearchPosition::kToolbarButton) {
    // Focus should cycle back around to tab_0.
    press_right();
    EXPECT_TRUE(tab_0->HasFocus());
    EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
  }
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest, TestReverseFocus) {
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
  if (tabs::GetTabSearchPosition(browser()) ==
      tabs::TabSearchPosition::kToolbarButton) {
    EXPECT_TRUE(new_tab_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_search_button()->HasFocus());
  }

  press_left();
  if (tabs::GetTabSearchPosition(browser()) !=
      tabs::TabSearchPosition::kToolbarButton) {
    EXPECT_TRUE(new_tab_button()->HasFocus());
  } else {
    EXPECT_TRUE(tab_2->HasFocus());
  }

  if (tabs::GetTabSearchPosition(browser()) !=
      tabs::TabSearchPosition::kToolbarButton) {
    move_back_to_tab(tab_2);
    EXPECT_TRUE(tab_2->HasFocus());
  }

  move_back_to_tab(tab_1);
  EXPECT_TRUE(tab_1->HasFocus());

  move_back_to_tab(tab_0);
  EXPECT_TRUE(tab_0->HasFocus());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest, TestBeginEndFocus) {
  AppendTab();
  AppendTab();
  Tab* tab_0 = tab_strip()->tab_at(0);
  tab_strip()->tab_at(1);
  tab_strip()->tab_at(2);

  // Request focus on the tab strip region view.
  tab_strip_region_view()->RequestFocus();
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());

  if (tabs::GetTabSearchPosition(browser()) ==
      tabs::TabSearchPosition::kLeadingHorizontalTabstrip) {
    EXPECT_TRUE(tab_0->HasFocus());

#if !BUILDFLAG(IS_WIN)
    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->end_key()));
    EXPECT_TRUE(new_tab_button()->HasFocus());
#endif  // !BUILDFLAG(IS_WIN)

    EXPECT_TRUE(tab_strip_region_view()->AcceleratorPressed(
        tab_strip_region_view()->home_key()));
    if (tabs::GetTabSearchPosition(browser()) ==
        tabs::TabSearchPosition::kToolbarButton) {
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
    if (tabs::GetTabSearchPosition(browser()) ==
        tabs::TabSearchPosition::kToolbarButton) {
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

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest,
                       DefaultTestSearchContainerIsEndAligned) {
  if (tabs::GetTabSearchPosition(browser()) ==
      tabs::TabSearchPosition::kLeadingHorizontalTabstrip) {
    // The TabSearchContainer is calculated as controls padding away from the
    // first tab (not including bottom corner radius)
    const int tab_search_container_expected_end =
        tab_strip_region_view()->tab_strip()->x() +
        TabStyle::Get()->GetBottomCornerRadius() -
        GetLayoutConstant(LayoutConstant::kTabStripPadding);

    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  } else if (tabs::GetTabSearchPosition(browser()) !=
             tabs::TabSearchPosition::kToolbarButton) {
    const int tab_search_container_expected_end =
        tab_strip_region_view()->GetLocalBounds().right() -
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
    EXPECT_EQ(tab_search_container()->bounds().right(),
              tab_search_container_expected_end);
  }
}

// This test uses both shift+click and shift+ctrl/cmd+click to verify the
// AddSelectionFromAnchorTo function.
IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest,
                       MultiSelectAcrossNoncontiguousTabs) {
  AppendTab();
  AppendTab();
  AppendTab();
  Tab* tab_0 = tab_strip()->tab_at(0);
  Tab* tab_1 = tab_strip()->tab_at(1);
  Tab* tab_2 = tab_strip()->tab_at(2);
  Tab* tab_3 = tab_strip()->tab_at(3);

  auto click_tab = [](Tab* tab, int flags) {
    const int event_flags = flags | ui::EF_LEFT_MOUSE_BUTTON;
    ui::MouseEvent press_event(ui::EventType::kMousePressed, gfx::Point(),
                               gfx::Point(), base::TimeTicks::Now(),
                               event_flags, ui::EF_LEFT_MOUSE_BUTTON);
    tab->OnMousePressed(press_event);
  };
#if BUILDFLAG(IS_MAC)
  const int kPlatformModifier = ui::EF_COMMAND_DOWN;
#else
  const int kPlatformModifier = ui::EF_CONTROL_DOWN;
#endif
  // Establish Tab 2 as an anchor.
  click_tab(tab_2, ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_0));
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_1));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_2));
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_3));
  EXPECT_TRUE(tab_2->IsActive());

  // Shift click tab_3.
  click_tab(tab_3, ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN);
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_0));
  EXPECT_FALSE(tab_strip()->IsTabSelected(tab_1));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_2));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_3));
  EXPECT_TRUE(tab_3->IsActive());

  // Shift + Platform click tab_0.
  click_tab(tab_0,
            ui::EF_LEFT_MOUSE_BUTTON | ui::EF_SHIFT_DOWN | kPlatformModifier);
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_0));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_1));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_2));
  EXPECT_TRUE(tab_strip()->IsTabSelected(tab_3));
  EXPECT_TRUE(tab_0->IsActive());
}


