// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"

#include "base/i18n/base_i18n_switches.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_switches.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view_utils.h"

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

  views::LabelButton* tab_search_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::LabelButton>(
        kTabSearchButtonElementId);
  }

  views::View* new_tab_button() {
    return BrowserElementsViews::From(browser())->GetViewAs<views::View>(
        kNewTabButtonElementId);
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
                               features::kGlicCountryFiltering,
                               tabs::kTabStripUnification});
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
  EXPECT_TRUE(tab_search_button()->HasFocus());

  // Focus should cycle back around to tab_0.
  press_right();
  EXPECT_TRUE(tab_0->HasFocus());
  EXPECT_TRUE(tab_strip_region_view()->pane_has_focus());
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

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewBrowserTest,
                       DefaultTabSearchButtonIsEndAligned) {
  if (tabs::GetTabSearchPosition(browser()) ==
      tabs::TabSearchPosition::kLeadingHorizontalTabstrip) {
    // The tab search button is calculated as controls padding away from the
    // first tab (not including bottom corner radius)
    int tab_search_button_expected_end =
        tab_strip_region_view()->tab_strip()->x() +
        TabStyle::Get()->GetBottomCornerRadius() -
        (2 * GetLayoutConstant(LayoutConstant::kTabStripPadding));

    EXPECT_EQ(tab_search_button()->bounds().right(),
              tab_search_button_expected_end);
  } else {
    const int tab_search_button_expected_end =
        tab_strip_region_view()->GetLocalBounds().right() -
        GetLayoutConstant(LayoutConstant::kTabStripPadding);
    EXPECT_EQ(tab_search_button()->bounds().right(),
              tab_search_button_expected_end);
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

class HorizontalTabStripRegionViewNewInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  HorizontalTabStripRegionViewNewInteractiveUiTest()
      : render_mode_resetter_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    scoped_feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }
  HorizontalTabStripRegionViewNewInteractiveUiTest(
      const HorizontalTabStripRegionViewNewInteractiveUiTest&) = delete;
  HorizontalTabStripRegionViewNewInteractiveUiTest& operator=(
      const HorizontalTabStripRegionViewNewInteractiveUiTest&) = delete;
  ~HorizontalTabStripRegionViewNewInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  auto SetWindowBounds(const gfx::Rect& bounds) {
    return Do([this, bounds]() { browser()->GetWindow()->SetBounds(bounds); });
  }

  HorizontalTabStripRegionViewNew* horizontal_tab_strip_region_view() {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    return views::AsViewClass<HorizontalTabStripRegionViewNew>(
        browser_view->tab_strip_view());
  }

  views::View* scroll_button_container() {
    return horizontal_tab_strip_region_view()
               ? horizontal_tab_strip_region_view()
                     ->scroll_button_container_for_testing()
               : nullptr;
  }
  TabStripView* tab_strip_view() {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* const region_view =
        views::AsViewClass<HorizontalTabStripRegionViewNew>(
            browser_view->tab_strip_view());
    return views::AsViewClass<TabStripView>(region_view->GetTabStripView());
  }

  // Adds unpinned tabs until the unpinned tab container is scrollable. Will
  // also add `extra_tabs` tabs after reaching this state.
  void AddTabsUntilScrollable(int extra_tabs = 0) {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    views::View* scroll_buttons = scroll_button_container();

    views::test::RunScheduledLayout(browser_view);

    int tabs_added = 0;
    while (!scroll_buttons->GetVisible() && tabs_added < kMaxTabsToAdd) {
      chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
      views::test::RunScheduledLayout(browser_view);
      ++tabs_added;
    }

    for (int i = 0; i < extra_tabs; ++i) {
      chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    }
    views::test::RunScheduledLayout(browser_view);
  }

  void AddPinnedTabsUntilScrollable() {
    auto* const browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    views::View* scroll_buttons = scroll_button_container();

    views::test::RunScheduledLayout(browser_view);

    int tabs_added = 0;
    while (!scroll_buttons->GetVisible() && tabs_added < kMaxTabsToAdd) {
      chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
      browser()->tab_strip_model()->SetTabPinned(
          browser()->tab_strip_model()->count() - 1, true);
      views::test::RunScheduledLayout(browser_view);
      ++tabs_added;
    }
  }

  // Checks if the unpinned tab at `index` is visible in the unpinned scroll
  // view.
  bool IsTabVisible(int index) {
    views::ScrollView* scroll_view =
        tab_strip_view()->unpinned_tabs_scroll_view();
    views::View* tab =
        tab_strip_view()->GetUnpinnedTabsContainer()->children()[index];
    return scroll_view->GetVisibleRect().Intersects(tab->bounds());
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  gfx::AnimationTestApi::RenderModeResetter render_mode_resetter_;
  static constexpr int kMaxTabsToAdd = 100;
};

class HorizontalTabStripRegionViewNewRTLInteractiveUiTest
    : public HorizontalTabStripRegionViewNewInteractiveUiTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    HorizontalTabStripRegionViewNewInteractiveUiTest::SetUpCommandLine(
        command_line);
    command_line->AppendSwitchASCII(::switches::kForceUIDirection,
                                    ::switches::kForceDirectionRTL);
    command_line->AppendSwitchASCII(::switches::kForceTextDirection,
                                    ::switches::kForceDirectionRTL);
  }
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewInteractiveUiTest,
                       ScrollButtonsHiddenWhenTabsFit) {
  for (int i = 0; i < 4; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
  }
  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      EnsureNotPresent(TabScrollButtonContainer::kTabScrollButtonContainer));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewInteractiveUiTest,
                       ScrollButtonsShowOnTabsOverflowAndHideWhenTabsFitAgain) {
  AddTabsUntilScrollable();
  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      WaitForShow(TabScrollButtonContainer::kTabScrollButtonContainer),
      Do([this]() {
        auto* const model = browser()->tab_strip_model();
        while (model->count() > 1) {
          model->CloseWebContentsAt(model->count() - 1,
                                    TabCloseTypes::CLOSE_USER_GESTURE);
        }
      }),
      WaitForHide(TabScrollButtonContainer::kTabScrollButtonContainer));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewInteractiveUiTest,
                       ScrollButtonsRespondToWindowResize) {
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      gfx::Rect(100, 100, 700, 600));
  AddTabsUntilScrollable();

  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      SetWindowBounds(gfx::Rect(100, 100, 1200, 800)),
      WaitForHide(TabScrollButtonContainer::kTabScrollButtonContainer),
      SetWindowBounds(gfx::Rect(100, 100, 700, 600)),
      WaitForShow(TabScrollButtonContainer::kTabScrollButtonContainer),
      SetWindowBounds(gfx::Rect(100, 100, 1200, 800)),
      WaitForHide(TabScrollButtonContainer::kTabScrollButtonContainer));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewInteractiveUiTest,
                       ScrollButtonsShowOnOverflowWithPinnedTabs) {
  // Add unpinned tabs and pinned tabs. The pinned tabs
  // should cause the unpinned tabs container to overflow.
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      gfx::Rect(100, 100, 1000, 800));

  for (int i = 0; i < 20; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
  }
  AddPinnedTabsUntilScrollable();

  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      WaitForShow(TabScrollButtonContainer::kTabScrollButtonContainer),
      Do([this]() {
        auto* const model = browser()->tab_strip_model();
        for (int i = model->IndexOfFirstNonPinnedTab() - 1; i >= 0; --i) {
          model->CloseWebContentsAt(i, /*close_types=*/0);
        }
      }),
      WaitForHide(TabScrollButtonContainer::kTabScrollButtonContainer));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewInteractiveUiTest,
                       ClickHorizontalScrollButtons) {
  // We set the window size and number of tabs explicitly so that
  // first and last tabs are scrolled into and out of view.
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      gfx::Rect(10, 10, 1000, 780));

  AddTabsUntilScrollable(/*extra_tabs=*/10);
  browser()->tab_strip_model()->ActivateTabAt(0);

  const int last_tab_index = browser()->tab_strip_model()->count() - 1;

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kFirstTabVisibleObserver);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kLastTabVisibleObserver);

  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      WaitForShow(TabScrollButtonContainer::kTabScrollButtonContainer),
      PollState(
          kFirstTabVisibleObserver,
          base::BindRepeating(
              &HorizontalTabStripRegionViewNewInteractiveUiTest::IsTabVisible,
              base::Unretained(this), 0)),
      PollState(
          kLastTabVisibleObserver,
          base::BindRepeating(
              &HorizontalTabStripRegionViewNewInteractiveUiTest::IsTabVisible,
              base::Unretained(this), last_tab_index)),
      WaitForState(kFirstTabVisibleObserver, true),
      WaitForState(kLastTabVisibleObserver, false),
      // After scrolling to the end, last tab should be scrolled into view and
      // first tab should no longer be visible.
      PressButton(TabScrollButtonContainer::kEndScrollButton),
      WaitForState(kFirstTabVisibleObserver, false),
      WaitForState(kLastTabVisibleObserver, true),
      // We should be in previous state.
      PressButton(TabScrollButtonContainer::kStartScrollButton),
      WaitForState(kFirstTabVisibleObserver, true),
      WaitForState(kLastTabVisibleObserver, false));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripRegionViewNewRTLInteractiveUiTest,
                       ClickHorizontalScrollButtonsRTL) {
  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetBounds(
      gfx::Rect(10, 10, 1000, 780));
  AddTabsUntilScrollable(/*extra_tabs=*/10);
  browser()->tab_strip_model()->ActivateTabAt(0);

  const int last_tab_index = browser()->tab_strip_model()->count() - 1;

  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kFirstTabVisibleObserver);
  DEFINE_LOCAL_STATE_IDENTIFIER_VALUE(ui::test::PollingStateObserver<bool>,
                                      kLastTabVisibleObserver);

  RunTestSequence(
      EnsurePresent(kTabStripRegionElementId),
      WaitForShow(TabScrollButtonContainer::kTabScrollButtonContainer),
      PollState(
          kFirstTabVisibleObserver,
          base::BindRepeating(
              &HorizontalTabStripRegionViewNewInteractiveUiTest::IsTabVisible,
              base::Unretained(this), 0)),
      PollState(
          kLastTabVisibleObserver,
          base::BindRepeating(
              &HorizontalTabStripRegionViewNewInteractiveUiTest::IsTabVisible,
              base::Unretained(this), last_tab_index)),
      WaitForState(kFirstTabVisibleObserver, true),
      WaitForState(kLastTabVisibleObserver, false),
      // After scrolling to the left, last tab should be scrolled into view and
      // first tab should no longer be visible. Note we are in RTL.
      PressButton(TabScrollButtonContainer::kStartScrollButton),
      WaitForState(kFirstTabVisibleObserver, false),
      WaitForState(kLastTabVisibleObserver, true),
      PressButton(TabScrollButtonContainer::kEndScrollButton),
      // We should be in previous state.
      WaitForState(kFirstTabVisibleObserver, true),
      WaitForState(kLastTabVisibleObserver, false));
}
