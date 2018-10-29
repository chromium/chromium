// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include "build/build_config.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/location_bar/location_icon_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"

// This file contains the bubble_anchor_util implementation for a Views
// browser window (BrowserView).

namespace bubble_anchor_util {

AnchorConfiguration GetPageInfoAnchorConfiguration(Browser* browser,
                                                   Anchor anchor) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);

  if (anchor == kLocationBar && browser_view->GetLocationBarView()->IsDrawn())
    return {browser_view->GetLocationBarView(),
            browser_view->GetLocationBarView()->location_icon_view(),
            views::BubbleBorder::TOP_LEFT};
  // Fall back to menu button if no location bar present.

  views::Button* app_menu_button =
      browser_view->toolbar_button_provider()->GetAppMenuButton();
  if (app_menu_button && app_menu_button->IsDrawn())
    return {app_menu_button, app_menu_button, views::BubbleBorder::TOP_RIGHT};
  return {};
}

gfx::Rect GetPageInfoAnchorRect(Browser* browser) {
  // GetPageInfoAnchorConfiguration()'s anchor_view should be preferred if
  // available.
  DCHECK_EQ(GetPageInfoAnchorConfiguration(browser).anchor_view, nullptr);

  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  // Get position in view (taking RTL UI into account).
  int x_within_browser_view = browser_view->GetMirroredXInView(
      bubble_anchor_util::kNoToolbarLeftOffset);
  // Get position in screen, taking browser view origin into account. This is
  // 0,0 in fullscreen on the primary display, but not on secondary displays, or
  // in Hosted App windows.
  gfx::Point browser_view_origin = browser_view->GetBoundsInScreen().origin();
  browser_view_origin += gfx::Vector2d(x_within_browser_view, 0);
  return gfx::Rect(browser_view_origin, gfx::Size());
}

}  // namespace bubble_anchor_util
