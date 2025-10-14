// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"

#include <memory>
#include <string>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_prefs.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/fake_base_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab_strip_control_button.h"
#include "chrome/browser/ui/views/tabs/tab_strip_scroll_container.h"
#include "chrome/browser/ui/views/tabs/tab_style_views.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/test/views_test_utils.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

class LayoutWaiter : public views::ViewObserver {
 public:
  explicit LayoutWaiter(views::View* view) : view_(view) {
    view_->AddObserver(this);
  }
  ~LayoutWaiter() override {
    if (view_) {
      view_->RemoveObserver(this);
    }
  }

  void Wait() {
    // The view can be null if it's deleted before Wait() is called.
    if (!view_) {
      return;
    }
    run_loop_.Run();
  }

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override {
    // Quit the run loop when the observed view's bounds change.
    run_loop_.Quit();
  }

  void OnViewIsDeleting(views::View* observed_view) override {
    // If the view is deleted while we're waiting, stop observing and
    // ensure we don't try to access it later.
    view_ = nullptr;
    run_loop_.Quit();
  }

 private:
  raw_ptr<views::View> view_ = nullptr;
  base::RunLoop run_loop_;
};

// TabStripRegionViewTestBase contains no test cases.
class TabStripRegionViewTestBase : public InProcessBrowserTest {
 public:
  explicit TabStripRegionViewTestBase(bool has_scrolling)
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_ENABLED)) {
    if (has_scrolling) {
      scoped_feature_list_.InitWithFeatures(
          {tabs::kScrollableTabStrip, features::kTabScrollingButtonPosition},
          {});
    } else {
      scoped_feature_list_.InitWithFeatures({}, {tabs::kScrollableTabStrip});
    }
  }
  TabStripRegionViewTestBase(const TabStripRegionViewTestBase&) = delete;
  TabStripRegionViewTestBase& operator=(const TabStripRegionViewTestBase&) =
      delete;
  ~TabStripRegionViewTestBase() override = default;

  void SetUp() override {
    InProcessBrowserTest::SetUp();

    // Prevent hover cards from appearing when the mouse is over the tab. Tests
    // don't typically account for this possibly, so it can cause unrelated
    // tests to fail due to tab data not being set. See crbug.com/40672885.
    Tab::SetShowHoverCardOnMouseHoverForTesting(false);
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    Tab::SetShowHoverCardOnMouseHoverForTesting(true);
  }

 protected:
  TabStripRegionView* tab_strip_region_view() {
    return views::AsViewClass<TabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStrip* tab_strip() { return tab_strip_region_view()->tab_strip(); }

  TabStripModel* tab_strip_model() { return browser()->tab_strip_model(); }

 private:
  gfx::AnimationTestApi::RenderModeResetter animation_mode_reset_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TabStripRegionViewTest contains tests that will run with scrolling enabled
// and disabled.
class TabStripRegionViewTest : public TabStripRegionViewTestBase,
                               public testing::WithParamInterface<bool> {
 public:
  TabStripRegionViewTest() : TabStripRegionViewTestBase(GetParam()) {}
  TabStripRegionViewTest(const TabStripRegionViewTest&) = delete;
  TabStripRegionViewTest& operator=(const TabStripRegionViewTest&) = delete;
  ~TabStripRegionViewTest() override = default;
};

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest,
                       DISABLED_GrabHandleSpaceStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, i == 0);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LE(tab_strip_region_view()
                  ->reserved_grab_handle_space_for_testing()
                  ->bounds()
                  .right(),
              kTabStripRegionViewWidth);
  }
}

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest,
                       DISABLED_NewTabButtonStaysVisible) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  for (int i = 0; i < 100; ++i) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, i == 0);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LE(tab_strip_region_view()->GetNewTabButton()->bounds().right(),
              kTabStripRegionViewWidth);
  }
}

// TODO(crbug.com/41493572): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest,
                       DISABLED_NewTabButtonRightOfTabs) {
  const int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth, 20);

  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  EXPECT_EQ(tab_strip_region_view()->GetNewTabButton()->bounds().x(),
            tab_strip()->tab_at(0)->bounds().right());
}

// TODO(crbug.com/41496209): Skip for now due to test failing when CR2023
// enabled.
IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest, DISABLED_NewTabButtonInkDrop) {
  constexpr int kTabStripRegionViewWidth = 500;
  tab_strip_region_view()->SetBounds(0, 0, kTabStripRegionViewWidth,
                                     GetLayoutConstant(TAB_STRIP_HEIGHT));

  // Add a few tabs and simulate the new tab button's ink drop animation. This
  // should not cause any crashes since the ink drop layer size as well as the
  // ink drop container size should remain equal to the new tab button visible
  // bounds size. https://crbug.com/814105.
  auto* button = static_cast<TabStripControlButton*>(
      tab_strip_region_view()->GetNewTabButton());
  for (int i = 0; i < 10; ++i) {
    button->AnimateToStateForTesting(views::InkDropState::ACTION_TRIGGERED);
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    button->AnimateToStateForTesting(views::InkDropState::HIDDEN);
  }
}

// We want to make sure that the following children views sits flush with the
// top of tab strip region view:
// * tab strip
// * new tab button
// This is important in ensuring that we maximise the targetable area of these
// views when the tab strip is flush with the top of the screen when the window
// is maximized (Fitt's Law).
IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest,
                       ChildrenAreFlushWithTopOfTabStripRegionView) {
  tab_strip_region_view()->SetBounds(0, 0, 1000, 100);
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);

  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  // The tab strip should sit flush with the top of the
  // |tab_strip_region_view()|.
  gfx::Point tab_strip_origin(tab_strip()->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip(), tab_strip_region_view(),
                                    &tab_strip_origin);
  EXPECT_EQ(0, tab_strip_origin.y());

  // The new tab button should sit flush with the top of the
  // |tab_strip_region_view()|.
  gfx::Point new_tab_button_origin(
      tab_strip_region_view()->GetNewTabButton()->bounds().origin());
  views::View::ConvertPointToTarget(tab_strip(), tab_strip_region_view(),
                                    &new_tab_button_origin);
  EXPECT_EQ(0, new_tab_button_origin.y());
}

IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest,
                       TabSearchPositionLoggedOnConstruction) {
  using TabSearchPositionEnum = TabStripRegionView::TabSearchPositionEnum;
  const bool tab_search_trailing_tabstrip =
      tabs::GetTabSearchTrailingTabstrip(browser()->profile());
  TabSearchPositionEnum expected_enum_val =
      tab_search_trailing_tabstrip ? TabSearchPositionEnum::kTrailing
                                   : TabSearchPositionEnum::kLeading;

  base::HistogramTester histogram_tester;
  tab_strip_region_view()->LogTabSearchPositionForTesting();  // IN-TEST
  histogram_tester.ExpectUniqueSample("Tabs.TabSearch.PositionInTabstrip",
                                      expected_enum_val, 1);
}

IN_PROC_BROWSER_TEST_P(TabStripRegionViewTest, HasMultiselectableState) {
  ui::AXNodeData ax_node_data;
  tab_strip_region_view()->GetViewAccessibility().GetAccessibleNodeData(
      &ax_node_data);
  EXPECT_TRUE(ax_node_data.HasState(ax::mojom::State::kMultiselectable));
}

class TabStripRegionViewTestWithScrollingDisabled
    : public TabStripRegionViewTestBase {
 public:
  TabStripRegionViewTestWithScrollingDisabled()
      : TabStripRegionViewTestBase(false) {}
  TabStripRegionViewTestWithScrollingDisabled(
      const TabStripRegionViewTestWithScrollingDisabled&) = delete;
  TabStripRegionViewTestWithScrollingDisabled& operator=(
      const TabStripRegionViewTestWithScrollingDisabled&) = delete;
  ~TabStripRegionViewTestWithScrollingDisabled() override = default;

 private:
};

// When scrolling is disabled, the tab strip cannot be larger than the container
// so tabs that do not fit in the tabstrip will become invisible. This is the
// opposite behavior from
// TabStripRegionViewTestWithScrollingEnabled.TabStripCanBeLargerThanContainer.
// TODO(crbug.com/451682395): Disabled on Linux dbg due to flakiness.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_TabStripCannotBeLargerThanContainer \
  DISABLED_TabStripCannotBeLargerThanContainer
#else
#define MAYBE_TabStripCannotBeLargerThanContainer \
  TabStripCannotBeLargerThanContainer
#endif
IN_PROC_BROWSER_TEST_F(TabStripRegionViewTestWithScrollingDisabled,
                       MAYBE_TabStripCannotBeLargerThanContainer) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  // Add tabs to the tabstrip until it is full.
  while (tab_strip()->tab_at(0)->width() > minimum_active_width) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LT(tab_strip()->width(), tab_strip_region_view()->width());
  }

  // Add a few more tabs after the tabstrip is full to ensure tabs added
  // afterwards are not visible.
  for (int i = 0; i < 10; i++) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
  }
  EXPECT_LT(tab_strip()->width(), tab_strip_region_view()->width());
  EXPECT_FALSE(
      tab_strip()->tab_at(tab_strip()->GetModelCount() - 1)->GetVisible());
}

class TabStripRegionViewTestWithScrollingEnabled
    : public TabStripRegionViewTestBase {
 public:
  TabStripRegionViewTestWithScrollingEnabled()
      : TabStripRegionViewTestBase(true) {}
  TabStripRegionViewTestWithScrollingEnabled(
      const TabStripRegionViewTestWithScrollingEnabled&) = delete;
  TabStripRegionViewTestWithScrollingEnabled& operator=(
      const TabStripRegionViewTestWithScrollingEnabled&) = delete;
  ~TabStripRegionViewTestWithScrollingEnabled() override = default;
};

// When scrolling is enabled, the tab strip can grow to be larger than the
// container. This is the opposite behavior from
// TabStripRegionViewTestWithScrollingDisabled.
// TabStripCannotBeLargerThanContainer.
// TODO(crbug.com/442378742): Fix failures on the Linux ASan LSan Tests bot.
IN_PROC_BROWSER_TEST_F(TabStripRegionViewTestWithScrollingEnabled,
                       DISABLED_TabStripCanBeLargerThanContainer) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  // Add tabs to the tabstrip until it is full and should start overflowing.
  while (tab_strip()->tab_at(0)->width() > minimum_active_width) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
    EXPECT_LT(tab_strip()->width(), tab_strip_region_view()->width());
  }

  // Add a few more tabs after the tabstrip is full to ensure the tabstrip
  // starts scrolling. This needs to expand the tabstrip width by a decent
  // amount in order to get the tabstrip to be wider than the entire tabstrip
  // region, not just the portion of that that's allocated to the tabstrip
  // itself (e.g. some of that space is for the NTB).
  for (int i = 0; i < 10; i++) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
  }
  EXPECT_GT(tab_strip()->width(), tab_strip_region_view()->width());
  EXPECT_TRUE(
      tab_strip()->tab_at(tab_strip()->GetModelCount() - 1)->GetVisible());
}

IN_PROC_BROWSER_TEST_F(TabStripRegionViewTestWithScrollingEnabled,
                       DISABLED_TabStripScrollButtonsNotInWindowCaption) {
  const int minimum_active_width = TabStyle::Get()->GetMinimumInactiveWidth();
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  {
    LayoutWaiter waiter(tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
    RunScheduledLayouts();
    waiter.Wait();
  }

  // Add tabs to the tabstrip until it is full and should start overflowing.
  while (tab_strip()->tab_at(0)->width() > minimum_active_width) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
  }

  // Add a few more tabs after the tabstrip is full to ensure the tabstrip
  // starts scrolling. This needs to expand the tabstrip width by a decent
  // amount in order to get the tabstrip to be wider than the entire tabstrip
  // region, not just the portion of that that's allocated to the tabstrip
  // itself (e.g. some of that space is for the NTB).
  for (int i = 0; i < 10; i++) {
    chrome::AddTabAt(browser(), GURL("about:blank"), -1, false);
    {
      LayoutWaiter waiter(
          tab_strip()->tab_at(tab_strip()->GetModelCount() - 1));
      RunScheduledLayouts();
      waiter.Wait();
    }
  }

  raw_ptr<TabStripScrollContainer> scroll_container =
      views::AsViewClass<TabStripScrollContainer>(
          tab_strip_region_view()->GetTabStripContainerForTesting());
  raw_ptr<views::ImageButton> leading_scroll_button_ =
      scroll_container->GetLeadingScrollButtonForTesting();
  raw_ptr<views::ImageButton> trailing_scroll_button_ =
      scroll_container->GetTrailingScrollButtonForTesting();

  // Check to see if children are visible
  EXPECT_TRUE(leading_scroll_button_ != nullptr &&
              leading_scroll_button_->IsDrawn());
  EXPECT_TRUE(trailing_scroll_button_ != nullptr &&
              trailing_scroll_button_->IsDrawn());

  gfx::Point scrolling_button_point =
      leading_scroll_button_->bounds().CenterPoint();
  gfx::Rect scrolling_button_rect =
      gfx::Rect(scrolling_button_point, gfx::Size(1, 1));
  gfx::RectF floating_rect_in_target_coords_f(scrolling_button_rect);
  views::View::ConvertRectToTarget(leading_scroll_button_,
                                   tab_strip_region_view(),
                                   &floating_rect_in_target_coords_f);

  EXPECT_FALSE(tab_strip_region_view()->IsRectInWindowCaption(
      gfx::ToEnclosingRect(floating_rect_in_target_coords_f)));
}

INSTANTIATE_TEST_SUITE_P(All,
                         TabStripRegionViewTest,
                         ::testing::Values(true, false));
