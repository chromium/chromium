// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/horizontal_tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view_layout.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_combo_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/split_tabs/split_tab_visual_data.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
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
                       ContainerOverlapDisabledOnOverflow) {
  AppendPinnedTab();
  SetWindowWidth(800);

  auto* tab_strip_view = GetTabStripView();
  ASSERT_NE(tab_strip_view, nullptr);
  auto* pinned_scroll = tab_strip_view->pinned_tabs_scroll_view();
  auto* unpinned_scroll = tab_strip_view->unpinned_tabs_scroll_view();
  ASSERT_NE(pinned_scroll, nullptr);
  ASSERT_NE(unpinned_scroll, nullptr);

  const int tab_overlap = TabStyle::Get()->GetTabOverlap();

  // In non-overflowing state, the unpinned container overlaps the pinned
  // container by `tab_overlap`.
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddTabsUntilScrollable();

  // When overflowing, the containers do not overlap.
  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(unpinned_scroll->bounds().x(), pinned_scroll->bounds().right());

  const gfx::Size pinned_size = pinned_scroll->size();

  // Remove the extra unpinned tabs and restore the window width.
  while (GetTabStripModel()->count() > 2) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->size(), pinned_size);

  // Verify overlap is back once no longer overflowing.
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
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

  // When overflowing without pinned tabs, the unpinned container has leading
  // padding equal to the bottom extension width to avoid overlapping the tab
  // search button and provide padding for the overflow indicator.
  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(unpinned_scroll->bounds().x(),
            TabStyle::Get()->GetBottomExtensionWidth());
  EXPECT_FALSE(unpinned_scroll->GetBoundsInScreen().Intersects(
      combo_button->GetBoundsInScreen()));

  const gfx::Size pinned_size = pinned_scroll->size();

  // Remove the extra unpinned tabs and restore the window width.
  while (GetTabStripModel()->count() > 1) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->size(), pinned_size);

  // Verify container starts back at x = 0 once no longer overflowing.
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

  // When overflowing, the pinned container has leading padding equal to the
  // bottom extension width to avoid overlapping the tab search button and
  // provide padding for the overflow indicator.
  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(pinned_scroll->bounds().x(),
            TabStyle::Get()->GetBottomExtensionWidth());
  EXPECT_FALSE(pinned_scroll->GetBoundsInScreen().Intersects(
      combo_button->GetBoundsInScreen()));

  // Remove the extra pinned tabs and restore the window width.
  while (GetTabStripModel()->IndexOfFirstNonPinnedTab() > 1) {
    GetTabStripModel()->CloseWebContentsAt(0,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);

  // Verify container starts back at x = 0 once no longer overflowing.
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

  // Without leading buttons, the unpinned container remains at x = 0 even when
  // overflowing.
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddTabsUntilScrollable();

  EXPECT_TRUE(unpinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);

  while (GetTabStripModel()->count() > 1) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);
  EXPECT_EQ(unpinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());

  // With pinned tabs and no leading buttons, the pinned container also remains
  // at x = 0 when overflowing.
  AppendPinnedTab();
  SetWindowWidth(800);
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);
  EXPECT_FALSE(pinned_scroll->GetDrawOverflowIndicator());

  SetWindowWidth(500);
  AddPinnedTabsUntilScrollable();

  EXPECT_TRUE(pinned_scroll->GetDrawOverflowIndicator());
  EXPECT_EQ(pinned_scroll->bounds().x(), 0);

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
