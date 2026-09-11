// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_strip_view.h"

#include "base/auto_reset.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/shared/tab_strip_types.h"
#include "chrome/browser/ui/views/test/vertical_tabs_browser_test_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/normalized_geometry.h"
#include "ui/views/view_utils.h"

class TabStripViewBrowserTest
    : public VerticalTabsBrowserTestMixin<InProcessBrowserTest>,
      public testing::WithParamInterface<TabStripOrientation> {
 public:
  TabStripViewBrowserTest() = default;
  ~TabStripViewBrowserTest() override = default;

  TabStripOrientation orientation() const { return GetParam(); }

  bool is_horizontal() const {
    return orientation() == TabStripOrientation::kHorizontal;
  }

  views::LayoutOrientation layout_orientation() const {
    return is_horizontal() ? views::LayoutOrientation::kHorizontal
                           : views::LayoutOrientation::kVertical;
  }

  void SetUpOnMainThread() override {
    VerticalTabsBrowserTestMixin<InProcessBrowserTest>::SetUpOnMainThread();
    if (is_horizontal()) {
      ExitVerticalTabsMode();
    }
  }

  const std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      override {
    auto enabled = VerticalTabsBrowserTestMixin<
        InProcessBrowserTest>::GetEnabledFeatures();
    enabled.push_back({tabs::kTabStripUnification, {}});
    return enabled;
  }

  TabStripView* tab_strip_view() {
    auto* base_region_view = views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
    return base_region_view ? views::AsViewClass<TabStripView>(
                                  base_region_view->GetTabStripView())
                            : nullptr;
  }

  views::ScrollView* pinned_tabs_scroll_view() {
    return tab_strip_view() ? tab_strip_view()->pinned_tabs_scroll_view()
                            : nullptr;
  }

  int GetMainAxisSize(const views::View* view) const {
    return views::GetMainAxis(layout_orientation(), view->size());
  }

 private:
  std::unique_ptr<base::AutoReset<gfx::Animation::RichAnimationRenderMode>>
      animation_mode_reset_ = gfx::AnimationTestApi::SetRichAnimationRenderMode(
          gfx::Animation::RichAnimationRenderMode::FORCE_DISABLED);
};

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       PinnedContainerDoesNotExceedHalfAvailableSpace) {
  // Add 15 pinned tabs and 4 unpinned tabs so that the pinned tabs prefer more
  // than 50% of the available tab strip space.
  for (int i = 0; i < 15; ++i) {
    AppendPinnedTab();
  }
  for (int i = 0; i < 4; ++i) {
    AppendTab();
  }

  ASSERT_EQ(tab_strip_model()->count(), 20);
  ASSERT_EQ(tab_strip_model()->IndexOfFirstNonPinnedTab(), 15);

  RunScheduledLayouts();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);

  // The pinned tabs scroll view must not exceed half of the tab strip's size
  // along the main layout axis.
  const int pinned_size = GetMainAxisSize(pinned_tabs_scroll_view());
  const int tab_strip_size = GetMainAxisSize(tab_strip_view());
  EXPECT_GT(pinned_size, 0);
  EXPECT_LE(pinned_size, tab_strip_size / 2);
}

IN_PROC_BROWSER_TEST_P(TabStripViewBrowserTest,
                       PinnedContainerExpandsWhenUnpinnedDoesNotFillSpace) {
  // Pin the initial unpinned tab and append 14 more pinned tabs so there are 15
  // pinned tabs with no unpinned tabs present.
  tab_strip_model()->SetTabPinned(0, true);
  for (int i = 1; i < 15; ++i) {
    AppendPinnedTab();
  }

  ASSERT_EQ(tab_strip_model()->count(), 15);
  ASSERT_EQ(tab_strip_model()->IndexOfFirstNonPinnedTab(), 15);

  RunScheduledLayouts();

  ASSERT_NE(tab_strip_view(), nullptr);
  ASSERT_NE(pinned_tabs_scroll_view(), nullptr);

  // The pinned tabs scroll view should expand to occupy more than half of the
  // tab strip's main-axis size when unpinned tabs do not fill the space.
  EXPECT_GT(GetMainAxisSize(pinned_tabs_scroll_view()),
            GetMainAxisSize(tab_strip_view()) / 2);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    TabStripViewBrowserTest,
    testing::Values(TabStripOrientation::kVertical,
                    TabStripOrientation::kHorizontal),
    [](const testing::TestParamInfo<TabStripOrientation>& info) {
      switch (info.param) {
        case TabStripOrientation::kVertical:
          return "Vertical";
        case TabStripOrientation::kHorizontal:
          return "Horizontal";
      }
    });
