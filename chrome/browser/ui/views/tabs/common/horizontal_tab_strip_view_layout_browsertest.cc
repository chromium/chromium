// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/i18n/test/scoped_rtl_for_testing.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view_layout.h"
#include "chrome/browser/ui/views/tabs/horizontal/horizontal_tab_strip_overflow_indicator_view.h"
#include "chrome/browser/ui/views/tabs/horizontal/tab_scroll_button_container.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

class HorizontalTabStripViewLayoutBrowserTest : public InProcessBrowserTest {
 public:
  HorizontalTabStripViewLayoutBrowserTest()
      : animation_mode_reset_(gfx::AnimationTestApi::SetRichAnimationRenderMode(
            gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED)) {
    feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }
  ~HorizontalTabStripViewLayoutBrowserTest() override = default;

 protected:
  TabStripModel* GetTabStripModel() { return browser()->GetTabStripModel(); }

  HorizontalTabStripRegionViewNew* GetTabStripRegionView() {
    return views::AsViewClass<HorizontalTabStripRegionViewNew>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStripView* GetTabStripView() {
    auto* base_region_view = GetTabStripRegionView();
    return base_region_view ? views::AsViewClass<TabStripView>(
                                  base_region_view->GetTabStripView())
                            : nullptr;
  }

  views::View* GetTabViewAt(int index) {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->tab_strip_view()
        ->GetTabAnchorView(
            GetTabStripModel()->GetTabAtIndex(index)->GetHandle());
  }

  void SetWindowWidth(int width) {
    auto* widget =
        BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
    gfx::Rect bounds = widget->GetWindowBoundsInScreen();
    bounds.set_width(width);
    widget->SetBounds(bounds);
    widget->LayoutRootViewIfNecessary();
  }

  void AppendTab() {
    chrome::AddTabAt(browser(), GURL("about:blank"), /*index=*/-1,
                     /*foreground=*/false);
  }

  void AppendPinnedTab() {
    chrome::AddTabAt(browser(), GURL("about:blank"),
                     /*index=*/-1,
                     /*foreground=*/false,
                     /*group=*/std::nullopt,
                     /*pinned=*/true);
  }

  void AddTabsUntilScrollable() {
    auto* tab_strip_view = GetTabStripView();
    ASSERT_NE(tab_strip_view, nullptr);
    auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
    ASSERT_NE(unpinned_scroll, nullptr);

    while (!unpinned_scroll->GetDrawOverflowIndicator() &&
           GetTabStripModel()->count() < kMaxTabsToAdd) {
      AppendTab();
      tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  void AddPinnedTabsUntilScrollable() {
    auto* tab_strip_view = GetTabStripView();
    ASSERT_NE(tab_strip_view, nullptr);
    auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
    ASSERT_NE(pinned_scroll, nullptr);

    while (!pinned_scroll->GetDrawOverflowIndicator() &&
           GetTabStripModel()->count() < kMaxTabsToAdd) {
      AppendPinnedTab();
      tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

  static constexpr int kMaxTabsToAdd = 100;

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_;
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       ContainerOverlapAndZOrderOnOverflow) {
  AppendPinnedTab();
  SetWindowWidth(800);

  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
  auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
  ASSERT_NE(pinned_scroll, nullptr);
  ASSERT_NE(unpinned_scroll, nullptr);

  auto get_scroll_view_layer = [](views::ScrollView* scroll_view) {
    return scroll_view->layer() ? scroll_view->layer()
                                : scroll_view->contents()->parent()->layer();
  };
  auto is_drawn_under = [&](views::ScrollView* bottom_scroll,
                            views::ScrollView* top_scroll) {
    const views::View::Views z_children = tab_strip_view->GetChildrenInZOrder();
    auto bottom_view_it = std::ranges::find(z_children, bottom_scroll);
    auto top_view_it = std::ranges::find(z_children, top_scroll);
    const auto& layers = tab_strip_view->layer()->children();
    auto bottom_layer_it =
        std::ranges::find(layers, get_scroll_view_layer(bottom_scroll));
    auto top_layer_it =
        std::ranges::find(layers, get_scroll_view_layer(top_scroll));
    return bottom_view_it != z_children.end() &&
           top_view_it != z_children.end() && bottom_view_it < top_view_it &&
           bottom_layer_it != layers.end() && top_layer_it != layers.end() &&
           bottom_layer_it < top_layer_it;
  };

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();
  auto* pinned_right_indicator =
      tab_strip_view->GetPinnedRightOverflowIndicatorForTesting();
  auto* unpinned_left_indicator =
      tab_strip_view->GetUnpinnedLeftOverflowIndicatorForTesting();
  ASSERT_NE(pinned_right_indicator, nullptr);
  ASSERT_NE(unpinned_left_indicator, nullptr);
  auto expect_seam_hit_targets = [&]() {
    const int seam_x = pinned_scroll->bounds().right() -
                       pinned_right_indicator->opaque_thickness();
    EXPECT_EQ(seam_x, unpinned_scroll->bounds().x() +
                          unpinned_left_indicator->opaque_thickness());
    EXPECT_TRUE(pinned_scroll->Contains(
        tab_strip_view->GetEventHandlerForPoint(gfx::Point(seam_x - 3, 20))));
    EXPECT_TRUE(unpinned_scroll->Contains(
        tab_strip_view->GetEventHandlerForPoint(gfx::Point(seam_x + 3, 20))));
  };

  // In non-overflowing state, the unpinned container overlaps the pinned
  // container by `tab_overlap`, and pinned is ordered before unpinned.
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_TRUE(is_drawn_under(pinned_scroll, unpinned_scroll));
  expect_seam_hit_targets();
  EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(pinned_scroll),
            pinned_scroll->GetLocalBounds());
  EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(unpinned_scroll),
            unpinned_scroll->GetLocalBounds());

  // When the unpinned container overflows and its leading overflow indicator
  // is visible, the containers still overlap by `tab_overlap`, the overflowing
  // unpinned container is drawn under the pinned container, and its leading
  // indicator opaque region spans the full `tab_overlap` so its shadow starts
  // after the last pinned tab.
  SetWindowWidth(500);
  AddTabsUntilScrollable();
  unpinned_scroll->ScrollToOffset(gfx::PointF(50, 0));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_FALSE(pinned_right_indicator->GetVisible());
  EXPECT_TRUE(unpinned_left_indicator->GetVisible());
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), tab_overlap);
  EXPECT_EQ(pinned_right_indicator->opaque_thickness(), 0);
  EXPECT_EQ(
      views::View::ConvertRectToTarget(
          GetTabViewAt(0)->parent(), tab_strip_view, GetTabViewAt(0)->bounds())
          .right(),
      unpinned_scroll->bounds().x() +
          unpinned_left_indicator->opaque_thickness());
  EXPECT_TRUE(is_drawn_under(unpinned_scroll, pinned_scroll));
  expect_seam_hit_targets();
  EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(unpinned_scroll).x(),
            tab_overlap);

  const gfx::Size pinned_size = pinned_scroll->size();

  // Remove the extra unpinned tabs and restore the window width.
  while (GetTabStripModel()->count() > 2) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->size(), pinned_size);
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_TRUE(is_drawn_under(pinned_scroll, unpinned_scroll));

  // When the pinned container overflows (and the unpinned container does not),
  // the containers overlap by `tab_overlap / 2`, the overflowing pinned
  // container is drawn under the unpinned container, and its trailing indicator
  // opaque region ends at the unpinned scroll view's leading edge so the shadow
  // ends before the first unpinned tab.
  SetWindowWidth(500);
  AddPinnedTabsUntilScrollable();
  pinned_scroll->ScrollToOffset(gfx::PointF(0, 0));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_TRUE(pinned_right_indicator->GetVisible());
  EXPECT_FALSE(unpinned_left_indicator->GetVisible());
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap / 2);
  EXPECT_EQ(pinned_right_indicator->opaque_thickness(), tab_overlap / 2);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), 0);
  const int first_unpinned_index =
      GetTabStripModel()->IndexOfFirstNonPinnedTab();
  EXPECT_EQ(views::View::ConvertRectToTarget(
                GetTabViewAt(first_unpinned_index)->parent(), tab_strip_view,
                GetTabViewAt(first_unpinned_index)->bounds())
                .x(),
            pinned_scroll->bounds().right() -
                pinned_right_indicator->opaque_thickness());
  EXPECT_TRUE(is_drawn_under(pinned_scroll, unpinned_scroll));
  expect_seam_hit_targets();
  EXPECT_EQ(
      tab_strip_view->GetScrollViewUnmaskedBounds(pinned_scroll).right(),
      pinned_scroll->width() - pinned_right_indicator->opaque_thickness());
  EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(unpinned_scroll).x(),
            0);

  // When both the pinned and unpinned containers are scrollable and both
  // adjacent overflow indicators are visible at the overlap seam, the
  // containers overlap by `tab_overlap / 2`, and the two overflow gradients
  // meet flush back to back at the unpinned scroll view's leading edge.
  AddTabsUntilScrollable();
  pinned_scroll->ScrollToOffset(gfx::PointF(0, 0));
  unpinned_scroll->ScrollToOffset(gfx::PointF(50, 0));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_TRUE(pinned_right_indicator->GetVisible());
  EXPECT_TRUE(unpinned_left_indicator->GetVisible());
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap / 2);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), 0);
  EXPECT_EQ(pinned_right_indicator->opaque_thickness(), tab_overlap / 2);
  EXPECT_EQ(pinned_scroll->bounds().right() -
                pinned_right_indicator->opaque_thickness(),
            unpinned_scroll->bounds().x() +
                unpinned_left_indicator->opaque_thickness());
  EXPECT_TRUE(is_drawn_under(pinned_scroll, unpinned_scroll));
  expect_seam_hit_targets();
  EXPECT_EQ(
      tab_strip_view->GetScrollViewUnmaskedBounds(pinned_scroll).right(),
      pinned_scroll->width() - pinned_right_indicator->opaque_thickness());
  EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(unpinned_scroll).x(),
            0);

  // Scrolling the unpinned container back to the start hides its left indicator
  // without moving the seam indicator.
  unpinned_scroll->ScrollToOffset(gfx::PointF(0, 0));
  EXPECT_TRUE(pinned_right_indicator->GetVisible());
  EXPECT_FALSE(unpinned_left_indicator->GetVisible());
  EXPECT_EQ(pinned_right_indicator->opaque_thickness(), tab_overlap / 2);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), 0);
  EXPECT_EQ(views::View::ConvertRectToTarget(
                GetTabViewAt(first_unpinned_index)->parent(), tab_strip_view,
                GetTabViewAt(first_unpinned_index)->bounds())
                .x(),
            pinned_scroll->bounds().right() -
                pinned_right_indicator->opaque_thickness());
  expect_seam_hit_targets();

  // Scrolling the pinned container to the end while the unpinned container is
  // scrolled keeps the last pinned tab clear of the unpinned left indicator's
  // shadow without moving the seam indicator.
  unpinned_scroll->ScrollToOffset(gfx::PointF(50, 0));
  pinned_scroll->ScrollToOffset(gfx::PointF(
      pinned_scroll->contents()->width() - pinned_scroll->width(), 0));
  EXPECT_FALSE(pinned_right_indicator->GetVisible());
  EXPECT_TRUE(unpinned_left_indicator->GetVisible());
  EXPECT_EQ(pinned_right_indicator->opaque_thickness(), tab_overlap / 2);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), 0);
  const int last_pinned_index = first_unpinned_index - 1;
  EXPECT_EQ(views::View::ConvertRectToTarget(
                GetTabViewAt(last_pinned_index)->parent(), tab_strip_view,
                GetTabViewAt(last_pinned_index)->bounds())
                .right(),
            unpinned_scroll->bounds().x() +
                unpinned_left_indicator->opaque_thickness());
  expect_seam_hit_targets();

  // Restore mid-scroll on both containers and verify RTL unmasked bounds and
  // hit targets.
  pinned_scroll->ScrollToOffset(gfx::PointF(0, 0));
  unpinned_scroll->ScrollToOffset(gfx::PointF(50, 0));
  {
    base::i18n::ScopedRTLForTesting rtl(true);
    tab_strip_view->InvalidateLayout();
    tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

    EXPECT_EQ(tab_strip_view->GetScrollViewUnmaskedBounds(pinned_scroll).x(),
              pinned_right_indicator->opaque_thickness());
    const int rtl_seam_x = tab_strip_view->GetMirroredXInView(
        pinned_scroll->bounds().right() -
        pinned_right_indicator->opaque_thickness());
    EXPECT_TRUE(pinned_scroll->Contains(tab_strip_view->GetEventHandlerForPoint(
        gfx::Point(rtl_seam_x + 3, 20))));
    EXPECT_TRUE(
        unpinned_scroll->Contains(tab_strip_view->GetEventHandlerForPoint(
            gfx::Point(rtl_seam_x - 3, 20))));
  }
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       UnpinnedContainerLeadingPaddingOnOverflow) {
  SetWindowWidth(800);

  auto* region_view = GetTabStripRegionView();
  ASSERT_NE(region_view, nullptr);
  auto* combo_button = region_view->combo_button();
  ASSERT_NE(combo_button, nullptr);
  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
  auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
  ASSERT_NE(pinned_scroll, nullptr);
  ASSERT_NE(unpinned_scroll, nullptr);

  // In non-overflowing state without pinned tabs, the unpinned container starts
  // at x = 0.
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddTabsUntilScrollable();

  // When overflowing without pinned tabs, the unpinned container remains at
  // x = 0 while its left overflow indicator uses an opaque region equal to the
  // bottom extension width so its shadow starts past the tab search button.
  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  auto* unpinned_left_indicator =
      tab_strip_view->GetUnpinnedLeftOverflowIndicatorForTesting();
  ASSERT_NE(unpinned_left_indicator, nullptr);
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(),
            TabStyle::Get()->GetBottomExtensionWidth());
  EXPECT_LE(combo_button->GetBoundsInScreen().right(),
            unpinned_scroll->GetBoundsInScreen().x() +
                unpinned_left_indicator->opaque_thickness());

  const gfx::Size pinned_size = pinned_scroll->size();

  // Remove the extra unpinned tabs and restore the window width.
  while (GetTabStripModel()->count() > 1) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->size(), pinned_size);

  // Verify container remains at x = 0 once no longer overflowing.
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       PinnedContainerLeadingPaddingOnOverflow) {
  AppendPinnedTab();
  SetWindowWidth(800);

  auto* region_view = GetTabStripRegionView();
  ASSERT_NE(region_view, nullptr);
  auto* combo_button = region_view->combo_button();
  ASSERT_NE(combo_button, nullptr);
  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
  ASSERT_NE(pinned_scroll, nullptr);

  // In non-overflowing state, the pinned container starts at x = 0.
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(pinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddPinnedTabsUntilScrollable();

  // When overflowing, the pinned container remains at x = 0 while its left
  // overflow indicator uses an opaque region equal to the bottom extension
  // width so its shadow starts past the tab search button.
  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  auto* pinned_left_indicator =
      tab_strip_view->GetPinnedLeftOverflowIndicatorForTesting();
  ASSERT_NE(pinned_left_indicator, nullptr);
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_EQ(pinned_left_indicator->opaque_thickness(),
            TabStyle::Get()->GetBottomExtensionWidth());
  EXPECT_LE(combo_button->GetBoundsInScreen().right(),
            pinned_scroll->GetBoundsInScreen().x() +
                pinned_left_indicator->opaque_thickness());
  const int last_pinned_with_unpinned =
      GetTabStripModel()->IndexOfFirstNonPinnedTab() - 1;
  EXPECT_EQ(pinned_scroll->contents()->width(),
            GetTabViewAt(last_pinned_with_unpinned)->bounds().right() +
                TabStyle::Get()->GetTabOverlap() / 2);

  GetTabStripModel()->SetTabPinned(GetTabStripModel()->count() - 1, true);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  EXPECT_EQ(pinned_scroll->contents()->width(),
            GetTabViewAt(GetTabStripModel()->count() - 1)->bounds().right());
  GetTabStripModel()->SetTabPinned(GetTabStripModel()->count() - 1, false);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  // Remove the extra pinned tabs and restore the window width.
  while (GetTabStripModel()->IndexOfFirstNonPinnedTab() > 1) {
    GetTabStripModel()->CloseWebContentsAt(0,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);

  // Verify container remains at x = 0 once no longer overflowing.
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(pinned_scroll->GetDrawOverflowIndicator());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       NoLeadingPaddingOnOverflowWithoutLeadingButtons) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kTabSearchPinnedToTabstrip, false);
  SetWindowWidth(800);

  auto* region_view = GetTabStripRegionView();
  ASSERT_NE(region_view, nullptr);
  EXPECT_FALSE(region_view->HasLeadingButtons());
  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
  auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
  ASSERT_NE(pinned_scroll, nullptr);
  ASSERT_NE(unpinned_scroll, nullptr);
  auto* pinned_left_indicator =
      tab_strip_view->GetPinnedLeftOverflowIndicatorForTesting();
  auto* unpinned_left_indicator =
      tab_strip_view->GetUnpinnedLeftOverflowIndicatorForTesting();
  ASSERT_NE(pinned_left_indicator, nullptr);
  ASSERT_NE(unpinned_left_indicator, nullptr);

  // Without leading buttons, the unpinned container remains at x = 0 with zero
  // leading opaque thickness even when overflowing.
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddTabsUntilScrollable();

  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_EQ(unpinned_left_indicator->opaque_thickness(), 0);

  while (GetTabStripModel()->count() > 1) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  // With pinned tabs and no leading buttons, the pinned container also remains
  // at x = 0 with zero leading opaque thickness when overflowing.
  AppendPinnedTab();
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(pinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddPinnedTabsUntilScrollable();

  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_EQ(pinned_left_indicator->opaque_thickness(), 0);

  while (GetTabStripModel()->IndexOfFirstNonPinnedTab() > 1) {
    GetTabStripModel()->CloseWebContentsAt(0,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(pinned_scroll->GetDrawOverflowIndicator());
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       SplitTabCrossoverWidthConsistentAcrossActiveTabChanges) {
  // Create a split tab from the first two tabs (model indices 0 and 1) and add
  // 4 regular tabs (model indices 2..5).
  for (int i = 0; i < 5; ++i) {
    AppendTab();
  }
  GetTabStripModel()->ActivateTabAt(0);
  GetTabStripModel()->AddToNewSplit(
      {1}, split_tabs::SplitTabVisualData(),
      split_tabs::SplitTabCreatedSource::kTabContextMenu);
  SetWindowWidth(800);

  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  ASSERT_EQ(GetTabStripModel()->count(), 6);

  const int standard_width =
      TabStyle::Get()->GetStandardWidth(/*is_split=*/false);
  const int min_active_width =
      TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/false);
  const int split_standard_width =
      TabStyle::Get()->GetStandardWidth(/*is_split=*/true);
  const int split_min_active_width =
      TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/true);

  // Verify both split halves and regular tabs are strictly in the
  // above-crossover proportional domain.
  GetTabStripModel()->ActivateTabAt(0);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  ASSERT_NE(GetTabViewAt(0), nullptr);
  ASSERT_NE(GetTabViewAt(2), nullptr);
  ASSERT_GT(GetTabViewAt(0)->width(), split_min_active_width);
  ASSERT_LT(GetTabViewAt(0)->width(), split_standard_width);
  ASSERT_GT(GetTabViewAt(2)->width(), min_active_width);
  ASSERT_LT(GetTabViewAt(2)->width(), standard_width);

  std::vector<int> widths_when_split_active;
  for (int i = 0; i < GetTabStripModel()->count(); ++i) {
    ASSERT_NE(GetTabViewAt(i), nullptr);
    widths_when_split_active.push_back(GetTabViewAt(i)->width());
  }

  // Switching active tab from the split tab (index 0) to a regular tab
  // (index 2) above crossover must keep every tab's width identical.
  GetTabStripModel()->ActivateTabAt(2);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  for (int i = 0; i < GetTabStripModel()->count(); ++i) {
    EXPECT_EQ(GetTabViewAt(i)->width(), widths_when_split_active[i])
        << "tab index=" << i;
  }

  // Group the split tab (indices 0, 1) and one regular tab (index 2) into an
  // expanded tab group and verify widths remain identical above crossover
  // whether the grouped split tab, grouped regular tab, or an ungrouped
  // regular tab is active.
  GetTabStripModel()->AddToNewGroup({0, 1, 2});
  GetTabStripModel()->ActivateTabAt(0);
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  ASSERT_GT(GetTabViewAt(0)->width(), split_min_active_width);
  ASSERT_LT(GetTabViewAt(0)->width(), split_standard_width);
  ASSERT_GT(GetTabViewAt(2)->width(), min_active_width);
  ASSERT_LT(GetTabViewAt(2)->width(), standard_width);

  std::vector<int> grouped_widths_reference;
  for (int i = 0; i < GetTabStripModel()->count(); ++i) {
    grouped_widths_reference.push_back(GetTabViewAt(i)->width());
  }

  for (int active_index : {2, 3}) {
    SCOPED_TRACE(testing::Message() << "active_index=" << active_index);
    GetTabStripModel()->ActivateTabAt(active_index);
    tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
    for (int i = 0; i < GetTabStripModel()->count(); ++i) {
      EXPECT_EQ(GetTabViewAt(i)->width(), grouped_widths_reference[i])
          << "tab index=" << i;
    }
  }
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       NewTabButtonSpacingMatchesTabStripPadding) {
  SetWindowWidth(800);
  AppendTab();
  auto* region_view = GetTabStripRegionView();
  auto* last_tab = GetTabViewAt(1);
  ASSERT_TRUE(region_view && region_view->new_tab_button_for_testing() &&
              last_tab);

  GetTabStripModel()->ActivateTabAt(1);
  region_view->GetWidget()->LayoutRootViewIfNecessary();

  const int padding = GetLayoutConstant(LayoutConstant::kTabStripPadding);
  const int last_tab_visual_right = last_tab->GetBoundsInScreen().right() -
                                    TabStyle::Get()->GetBottomExtensionWidth();
  EXPECT_EQ(region_view->new_tab_button_for_testing()->GetBoundsInScreen().x() -
                last_tab_visual_right,
            padding);
  EXPECT_EQ(region_view->new_tab_button_for_testing()->bounds().y(), padding);
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       NewTabButtonTrailingPaddingOnOverflowWithScrollButtons) {
  SetWindowWidth(500);
  auto* region_view = GetTabStripRegionView();
  auto* scroll_buttons = GetTabStripView()
                             ? GetTabStripView()->GetScrollButtonContainer()
                             : nullptr;
  ASSERT_TRUE(region_view && region_view->new_tab_button_for_testing() &&
              scroll_buttons);

  AddTabsUntilScrollable();

  EXPECT_TRUE(scroll_buttons->GetVisible());
  EXPECT_EQ(region_view->new_tab_button_for_testing()->GetBoundsInScreen().x() -
                scroll_buttons->GetBoundsInScreen().right(),
            GetLayoutConstant(LayoutConstant::kTabStripPadding));
}

IN_PROC_BROWSER_TEST_F(
    HorizontalTabStripViewLayoutBrowserTest,
    NewTabButtonTrailingPaddingOnOverflowWithoutScrollButtons) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kTabScrollButtonsPinnedToTabstrip, false);
  SetWindowWidth(500);
  auto* region_view = GetTabStripRegionView();
  auto* unpinned_scroll = GetTabStripView()
                              ? GetTabStripView()->unpinned_tabs_scroll_view()
                              : nullptr;
  ASSERT_TRUE(region_view && region_view->new_tab_button_for_testing() &&
              unpinned_scroll);

  AddTabsUntilScrollable();

  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(region_view->new_tab_button_for_testing()->GetBoundsInScreen().x() -
                unpinned_scroll->GetBoundsInScreen().right(),
            GetLayoutConstant(LayoutConstant::kTabStripPadding));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       NewTabButtonTrailingPaddingOnPinnedOnlyOverflow) {
  SetWindowWidth(500);
  GetTabStripModel()->SetTabPinned(0, true);
  auto* region_view = GetTabStripRegionView();
  auto* pinned_scroll = GetTabStripView()
                            ? GetTabStripView()->pinned_tabs_scroll_view()
                            : nullptr;
  ASSERT_TRUE(region_view && region_view->new_tab_button_for_testing() &&
              pinned_scroll);

  AddPinnedTabsUntilScrollable();

  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(region_view->new_tab_button_for_testing()->GetBoundsInScreen().x() -
                pinned_scroll->GetBoundsInScreen().right(),
            GetLayoutConstant(LayoutConstant::kTabStripPadding));
}

IN_PROC_BROWSER_TEST_F(HorizontalTabStripViewLayoutBrowserTest,
                       ScrollToFitViewsAccountsForPostScrollOverflowIndicator) {
  AppendPinnedTab();
  SetWindowWidth(500);
  AddTabsUntilScrollable();
  AppendTab();
  AppendTab();

  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
  ASSERT_NE(unpinned_scroll, nullptr);
  auto* unpinned_left_indicator =
      tab_strip_view->GetUnpinnedLeftOverflowIndicatorForTesting();
  ASSERT_NE(unpinned_left_indicator, nullptr);

  const int first_unpinned_index =
      GetTabStripModel()->IndexOfFirstNonPinnedTab();
  GetTabStripModel()->ActivateTabAt(first_unpinned_index + 1);
  unpinned_scroll->ScrollToOffset(gfx::PointF(0, 0));
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();
  ASSERT_FALSE(unpinned_left_indicator->GetVisible());

  views::View* primary_tab = GetTabViewAt(first_unpinned_index + 1);
  ASSERT_NE(primary_tab, nullptr);
  int secondary_tab_index = -1;
  views::View* secondary_tab = nullptr;
  const int right_unoccluded_edge =
      unpinned_scroll->width() -
      HorizontalTabStripOverflowIndicatorView::kShadowThickness;
  for (int i = first_unpinned_index + 2; i < GetTabStripModel()->count(); ++i) {
    views::View* candidate = GetTabViewAt(i);
    if (candidate->bounds().right() > right_unoccluded_edge) {
      secondary_tab_index = i;
      secondary_tab = candidate;
      break;
    }
  }
  ASSERT_NE(secondary_tab, nullptr);

  tab_strip_view->OnChildrenAdded(
      {GetTabStripModel()->GetTabAtIndex(secondary_tab_index)->GetHandle()});
  unpinned_scroll->InvalidateLayout();
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(unpinned_scroll->GetVisibleRect().x(), 0);
  EXPECT_FALSE(unpinned_left_indicator->GetVisible());

  GetTabStripModel()->ActivateTabAt(secondary_tab_index);
  tab_strip_view->OnChildrenAdded({GetTabStripModel()
                                       ->GetTabAtIndex(first_unpinned_index + 1)
                                       ->GetHandle()});
  unpinned_scroll->InvalidateLayout();
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_GT(unpinned_scroll->GetVisibleRect().x(), 0);
  EXPECT_TRUE(unpinned_left_indicator->GetVisible());
  EXPECT_LE(secondary_tab->bounds().right(),
            unpinned_scroll->GetVisibleRect().right() -
                HorizontalTabStripOverflowIndicatorView::kShadowThickness);

  GetTabStripModel()->ActivateTabAt(secondary_tab_index - 1);
  tab_strip_view->OnChildrenAdded(
      {GetTabStripModel()->GetTabAtIndex(first_unpinned_index)->GetHandle()});
  unpinned_scroll->InvalidateLayout();
  tab_strip_view->GetWidget()->LayoutRootViewIfNecessary();

  EXPECT_EQ(unpinned_scroll->GetVisibleRect().x(), 0);
  EXPECT_FALSE(unpinned_left_indicator->GetVisible());
}
