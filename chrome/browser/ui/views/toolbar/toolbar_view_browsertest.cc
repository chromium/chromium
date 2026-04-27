// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/separator.h"
#include "ui/views/view.h"

class ToolbarViewUnitTest : public InProcessBrowserTest {
 public:
  ToolbarButton* GetForwardButton() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->forward_button();
  }
};

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, ForwardButtonVisibility) {
  // Forward button should be visible by default.
  EXPECT_TRUE(GetForwardButton()->GetVisible());

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());

  browser_view->GetProfile()->GetPrefs()->SetBoolean(prefs::kShowForwardButton,
                                                     false);
  EXPECT_FALSE(GetForwardButton()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, AccessibleProperties) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ui::AXNodeData data;

  toolbar->GetViewAccessibility().GetAccessibleNodeData(&data);
  EXPECT_EQ(data.role, ax::mojom::Role::kToolbar);
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       BubbleAnchorUsesLocationBarWhenDrawn) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);

  views::BubbleAnchor anchor =
      browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
  views::View* anchor_view = anchor.GetIfView();
  EXPECT_EQ(anchor_view, browser_view->toolbar()->location_bar_view());
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       BubbleAnchorUsesTopContainerWhenLocationBarNotDrawn) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  ASSERT_TRUE(browser_view);

  ToolbarView* toolbar = browser_view->toolbar();
  ASSERT_TRUE(toolbar);
  ASSERT_TRUE(toolbar->location_bar_view());
  // Simulate app windows (no visible location bar) by hiding the location bar.
  toolbar->location_bar_view()->SetVisible(false);

  views::BubbleAnchor anchor =
      browser_view->toolbar_button_provider()->GetBubbleAnchor(std::nullopt);
  views::View* anchor_view = anchor.GetIfView();
  // The fallback uses top_container when the location bar is not drawn.
  EXPECT_EQ(anchor_view,
            static_cast<views::View*>(browser_view->top_container()));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_EmptyArea) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // A point in the toolbar's empty area (beyond all children on the right)
  // should be treated as caption.
  gfx::Point empty_area(toolbar->width() - 1,
                        toolbar->height() / 2);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(empty_area));

  // A point in the upper portion of empty area should also be caption.
  gfx::Point upper_empty(toolbar->width() - 1, 1);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(upper_empty));

  // A point in the lower portion of empty area should also be caption
  // (no centerline restriction).
  gfx::Point lower_empty(toolbar->width() - 1,
                          toolbar->height() - 1);
  EXPECT_TRUE(toolbar->IsPositionInWindowCaption(lower_empty));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_OnButton) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // The forward button center should NOT be caption (it's an interactive
  // control).
  ToolbarButton* forward = toolbar->forward_button();
  ASSERT_TRUE(forward);
  ASSERT_TRUE(forward->GetVisible());
  gfx::Point forward_center = forward->bounds().CenterPoint();
  EXPECT_FALSE(toolbar->IsPositionInWindowCaption(forward_center));
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_BelowButton) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // A point directly below a button (same x, but y below the button's bottom
  // edge) should be caption since it's not inside any child's bounds.
  ToolbarButton* forward = toolbar->forward_button();
  ASSERT_TRUE(forward);
  ASSERT_TRUE(forward->GetVisible());

  gfx::Point below_button(forward->bounds().CenterPoint().x(),
                           toolbar->height() - 1);
  // This point is below the button — should be treated as caption.
  if (!forward->bounds().Contains(below_button)) {
    EXPECT_TRUE(toolbar->IsPositionInWindowCaption(below_button));
  }
}

IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest,
                       IsPositionInWindowCaption_OnSeparator) {
  ToolbarView* toolbar =
      BrowserView::GetBrowserViewForBrowser(browser())->toolbar();
  ASSERT_TRUE(toolbar);

  // Find a visible separator or divider in the toolbar and verify it's treated
  // as caption.
  for (views::View* child : toolbar->children()) {
    if (!child->GetVisible()) {
      continue;
    }
    if (views::IsViewClass<views::Separator>(child) &&
        child->bounds().width() > 0 && child->bounds().height() > 0) {
      gfx::Point separator_center = child->bounds().CenterPoint();
      EXPECT_TRUE(toolbar->IsPositionInWindowCaption(separator_center));
      break;
    }
  }
}

// Regression test for crbug.com/505902447: Verify that popup windows still
// have pinned toolbar actions, as they're used for pinning the download button.
IN_PROC_BROWSER_TEST_F(ToolbarViewUnitTest, PinnedToolbarActionsInPopup) {
  Browser* popup = CreateBrowserForPopup(browser()->profile());
  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(popup)
                  ->toolbar_button_provider()
                  ->GetPinnedToolbarActions());
}
