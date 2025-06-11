// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/location_bar/zoom_bubble_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_view.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

using ZoomViewBrowserTest = InProcessBrowserTest;

namespace {

views::View* GetZoomView(Browser* browser) {
  auto* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();

  if (IsPageActionMigrated(PageActionIconType::kZoom)) {
    return toolbar_button_provider->GetPageActionView(kActionZoomNormal);
  }

  return toolbar_button_provider->GetPageActionIconView(
      PageActionIconType::kZoom);
}

}  // namespace

// https://crbug.com/900134: Zoom icons in inactive windows should not be
// visible when zoom is reset back to default.
IN_PROC_BROWSER_TEST_F(ZoomViewBrowserTest, SharedPageVisibility) {
  auto* zoom_icon = GetZoomView(browser());
  auto* second_zoom_icon = GetZoomView(CreateBrowser(browser()->profile()));

  // Initially no icon.
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());
  EXPECT_FALSE(zoom_icon->GetVisible());
  EXPECT_FALSE(second_zoom_icon->GetVisible());

  // Zooming in one browser should show the icon in all browsers on the same
  // URL.
  chrome::Zoom(browser(), content::PAGE_ZOOM_IN);
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
  EXPECT_TRUE(zoom_icon->GetVisible());
  EXPECT_TRUE(second_zoom_icon->GetVisible());

  ZoomBubbleView::CloseCurrentBubble();
  EXPECT_FALSE(ZoomBubbleView::GetZoomBubble());

  // Clearing the zoom should clear the icon for all browsers on the URL except
  // the one where the interaction occurred because the bubble is showing there.
  chrome::Zoom(browser(), content::PAGE_ZOOM_RESET);
  EXPECT_TRUE(ZoomBubbleView::GetZoomBubble());
  EXPECT_TRUE(zoom_icon->GetVisible());
  EXPECT_FALSE(second_zoom_icon->GetVisible());
}
