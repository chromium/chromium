// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/accessibility/view_accessibility.h"
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
  auto* anchor_view = std::get_if<views::View*>(&anchor);
  ASSERT_TRUE(anchor_view);
  EXPECT_EQ(*anchor_view, browser_view->toolbar()->location_bar_view());
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
  auto* anchor_view = std::get_if<views::View*>(&anchor);
  ASSERT_TRUE(anchor_view);
  // The fallback uses top_container when the location bar is not drawn.
  EXPECT_EQ(*anchor_view,
            static_cast<views::View*>(browser_view->top_container()));
}
