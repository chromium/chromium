// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/auto_reset.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_view_layout.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/scroll_view.h"
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

  TabStripView* GetTabStripView() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? views::AsViewClass<TabStripView>(
                                  base_region_view->GetTabStripView())
                            : nullptr;
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

  // Remove the extra unpinned tabs and restore the window width.
  while (GetTabStripModel()->count() > 2) {
    GetTabStripModel()->CloseWebContentsAt(GetTabStripModel()->count() - 1,
                                           TabCloseTypes::CLOSE_USER_GESTURE);
  }
  SetWindowWidth(800);

  // Verify overlap is back once no longer overflowing.
  EXPECT_EQ(pinned_scroll->bounds().right() - unpinned_scroll->bounds().x(),
            tab_overlap);
  EXPECT_FALSE(unpinned_scroll->GetDrawOverflowIndicator());
}
