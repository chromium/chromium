// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/common/tab_collection_animating_layout_manager.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_style.h"
#include "chrome/browser/ui/views/frame/base_tab_strip_region_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/common/root_tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_collection_node.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_collection_controller.h"
#include "chrome/browser/ui/views/tabs/common/tab_strip_utils.h"
#include "chrome/browser/ui/views/tabs/common/tab_view.h"
#include "chrome/browser/ui/views/tabs/common/unpinned_tab_container_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view_utils.h"

class HorizontalTabCollectionAnimatingLayoutManagerInteractiveUiTest
    : public InteractiveBrowserTest {
 public:
  HorizontalTabCollectionAnimatingLayoutManagerInteractiveUiTest() {
    scoped_feature_list_.InitAndEnableFeature(tabs::kTabStripUnification);
  }
  ~HorizontalTabCollectionAnimatingLayoutManagerInteractiveUiTest() override =
      default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
  }

  void SetWindowWidth(int width) {
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    CHECK(browser_view && browser_view->GetWidget());
    gfx::Rect bounds = browser_view->GetWidget()->GetWindowBoundsInScreen();
    bounds.set_width(width);
    browser_view->GetWidget()->SetBounds(bounds);
    browser_view->GetWidget()->LayoutRootViewIfNecessary();
  }

  BaseTabStripRegionView* GetBaseTabStripRegionView() {
    return views::AsViewClass<BaseTabStripRegionView>(
        BrowserView::GetBrowserViewForBrowser(browser())->tab_strip_view());
  }

  TabStripCollectionController* GetController() {
    auto* region = GetBaseTabStripRegionView();
    return region ? region->GetTabStripCollectionController() : nullptr;
  }

  TabCollectionNode* GetUnpinnedNode() {
    auto* region = GetBaseTabStripRegionView();
    return region && region->root_node_for_testing()
               ? region->root_node_for_testing()->GetChildNodeOfType(
                     TabCollectionNode::Type::UNPINNED)
               : nullptr;
  }

  void CreateHorizontalOverflowingTabs() {
    SetWindowWidth(600);
    const views::ScrollView* scroll_view = nullptr;
    auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
    auto* unpinned_container =
        views::AsViewClass<UnpinnedTabContainerView>(GetUnpinnedNode()->view());

    // Keep adding tabs until the minimum required width of unpinned tabs
    // exceeds the scroll view's visible viewport, ensuring the strip is truly
    // overflowing in steady-state while layout animations run naturally.
    while (!scroll_view || !scroll_view->IsHorizontalContentOverflowing() ||
           !unpinned_container ||
           unpinned_container->GetMinimumSize().width() <=
               scroll_view->GetVisibleRect().width()) {
      CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                          ui::PAGE_TRANSITION_TYPED));
      if (browser_view && browser_view->GetWidget()) {
        browser_view->GetWidget()->LayoutRootViewIfNecessary();
      }
      if (auto* unpinned = GetUnpinnedNode()) {
        unpinned_container =
            views::AsViewClass<UnpinnedTabContainerView>(unpinned->view());
        if (unpinned_container) {
          scroll_view =
              views::ScrollView::GetScrollViewForContents(unpinned_container);
        }
      }
    }
    // Add additional tabs so the strip is well into the overflowing state.
    for (int i = 0; i < 3; ++i) {
      CHECK(AddTabAtIndex(-1, GURL(url::kAboutBlankURL),
                          ui::PAGE_TRANSITION_TYPED));
    }
    if (browser_view && browser_view->GetWidget()) {
      browser_view->GetWidget()->LayoutRootViewIfNecessary();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// When tabs overflow into a scroll view in the horizontal tab strip, switching
// the active tab calculates target layouts bounded to the visible viewport
// rather than the total scrolled content width, ensuring inactive tabs remain
// at their minimum width without flashing or expanding.
IN_PROC_BROWSER_TEST_F(
    HorizontalTabCollectionAnimatingLayoutManagerInteractiveUiTest,
    SwitchingActiveTabWhenOverflowingMaintainsTabSizes) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId),
      Do([this]() { CreateHorizontalOverflowingTabs(); }), Do([this]() {
        TabStripModel* model = browser()->tab_strip_model();
        auto* controller = GetController();
        ASSERT_NE(controller, nullptr);

        TabCollectionNode* unpinned = GetUnpinnedNode();
        ASSERT_NE(unpinned, nullptr);
        ASSERT_GE(unpinned->children().size(), 10u);

        auto* unpinned_container =
            views::AsViewClass<UnpinnedTabContainerView>(unpinned->view());
        ASSERT_NE(unpinned_container, nullptr);

        const auto* scroll_view =
            views::ScrollView::GetScrollViewForContents(unpinned_container);
        ASSERT_NE(scroll_view, nullptr);
        EXPECT_TRUE(scroll_view->IsHorizontalContentOverflowing());

        const int min_inactive_width =
            TabStyle::Get()->GetMinimumInactiveWidth();
        const int min_active_width =
            TabStyle::Get()->GetMinimumActiveWidth(/*is_split=*/false);

        const int initial_active_idx = model->active_index();
        EXPECT_EQ(GetTabStripViewTargetBounds(
                      unpinned->children()[initial_active_idx]->view())
                      .width(),
                  min_active_width);
        for (size_t i = 0; i < unpinned->children().size(); ++i) {
          if (static_cast<int>(i) == initial_active_idx) {
            continue;
          }
          EXPECT_EQ(GetTabStripViewTargetBounds(unpinned->children()[i]->view())
                        .width(),
                    min_inactive_width);
        }

        // Switch to a different tab (Tab 0).
        constexpr int kNewActiveIdx = 0;
        ASSERT_NE(initial_active_idx, kNewActiveIdx);
        controller->SelectTab(
            model->GetTabAtIndex(kNewActiveIdx),
            TabStripUserGestureDetails(
                TabStripUserGestureDetails::GestureType::kMouse));

        auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
        if (browser_view && browser_view->GetWidget()) {
          browser_view->GetWidget()->LayoutRootViewIfNecessary();
        }

        // Newly active tab target is at minimum active width, and other tab
        // targets remain at minimum inactive width without any transient
        // expansion.
        EXPECT_EQ(GetTabStripViewTargetBounds(
                      unpinned->children()[kNewActiveIdx]->view())
                      .width(),
                  min_active_width);
        for (size_t i = 0; i < unpinned->children().size(); ++i) {
          if (static_cast<int>(i) == kNewActiveIdx) {
            continue;
          }
          EXPECT_EQ(GetTabStripViewTargetBounds(unpinned->children()[i]->view())
                        .width(),
                    min_inactive_width);
        }
      }));
}

// When overflowing, the animating layout manager's target layout and available
// space override strictly use the visible viewport width.
IN_PROC_BROWSER_TEST_F(
    HorizontalTabCollectionAnimatingLayoutManagerInteractiveUiTest,
    TargetLayoutBoundsToScrollViewViewportWhenOverflowing) {
  RunTestSequence(
      WaitForShow(kNewTabButtonElementId),
      Do([this]() { CreateHorizontalOverflowingTabs(); }), Do([this]() {
        TabCollectionNode* unpinned = GetUnpinnedNode();
        ASSERT_NE(unpinned, nullptr);

        auto* unpinned_container =
            views::AsViewClass<UnpinnedTabContainerView>(unpinned->view());
        ASSERT_NE(unpinned_container, nullptr);

        const auto* scroll_view =
            views::ScrollView::GetScrollViewForContents(unpinned_container);
        ASSERT_NE(scroll_view, nullptr);
        EXPECT_TRUE(scroll_view->IsHorizontalContentOverflowing());

        const auto space_override =
            unpinned_container->GetAvailableMainAxisSpaceOverride();
        ASSERT_TRUE(space_override.has_value());
        ASSERT_TRUE(space_override->is_bounded());

        const int viewport_width = scroll_view->GetVisibleRect().width();
        EXPECT_EQ(space_override->value(), viewport_width);
        EXPECT_LT(viewport_width, unpinned_container->width());
      }));
}
