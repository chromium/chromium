// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include "build/build_config.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/content_settings/core/common/features.h"
#include "ui/views/bubble/bubble_border.h"

// This file contains the bubble_anchor_util implementation for a Views
// browser window (BrowserView).

namespace bubble_anchor_util {

AnchorConfiguration GetPageInfoAnchorConfiguration(Browser* browser,
                                                   Anchor anchor) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  auto* location_bar_view = browser_view->GetLocationBarView();

  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators)) {
    auto* permission_dashboard_view =
        location_bar_view->permission_dashboard_controller()
            ->permission_dashboard_view();

    if (anchor == kLocationBar && permission_dashboard_view->GetVisible()) {
      if (permission_dashboard_view->GetIndicatorChip()->GetVisible()) {
        return {permission_dashboard_view->GetIndicatorChip(),
                permission_dashboard_view->GetIndicatorChip(),
                views::BubbleBorder::TOP_LEFT};
      }

      return {permission_dashboard_view->GetRequestChip(),
              permission_dashboard_view->GetRequestChip(),
              views::BubbleBorder::TOP_LEFT};
    }
  } else {
    auto* request_chip_view = location_bar_view->GetChipController()->chip();
    if (anchor == kLocationBar && request_chip_view->GetVisible()) {
      return {request_chip_view, request_chip_view,
              views::BubbleBorder::TOP_LEFT};
    }
  }

  if (anchor == kLocationBar && location_bar_view->IsDrawn()) {
    return {location_bar_view, location_bar_view->location_icon_view(),
            views::BubbleBorder::TOP_LEFT};
  }

  if (anchor == kLocationBar && browser_view->GetIsPictureInPictureType()) {
    auto* frame_view = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->frame()->GetFrameView());
    return {frame_view->GetLocationIconView(),
            frame_view->GetLocationIconView(), views::BubbleBorder::TOP_LEFT};
  }

  if (anchor == kCustomTabBar && browser_view->toolbar()->custom_tab_bar())
    return {browser_view->toolbar()->custom_tab_bar(),
            browser_view->toolbar()->custom_tab_bar()->location_icon_view(),
            views::BubbleBorder::TOP_LEFT};

  // Fall back to menu button.
  views::Button* app_menu_button =
      browser_view->toolbar_button_provider()->GetAppMenuButton();
  if (!app_menu_button || !app_menu_button->IsDrawn())
    return {};

  // The app menu button is not visible when immersive mode is enabled and the
  // title bar is not revealed. So return null anchor configuration.
  if (browser_view->IsImmersiveModeEnabled() &&
      !browser_view->immersive_mode_controller()->IsRevealed()) {
    return {};
  }

  return {app_menu_button, app_menu_button, views::BubbleBorder::TOP_RIGHT};
}

AnchorConfiguration GetPermissionPromptBubbleAnchorConfiguration(
    Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view->GetLocationBarView()->GetChipController() &&
      browser_view->GetLocationBarView()
          ->GetChipController()
          ->IsPermissionPromptChipVisible()) {
    return {browser_view->GetLocationBarView(),
            browser_view->GetLocationBarView()->GetChipController()->chip(),
            views::BubbleBorder::TOP_LEFT};
  }
  return GetPageInfoAnchorConfiguration(browser);
}

AnchorConfiguration GetAppMenuAnchorConfiguration(Browser* browser) {
  return GetPageInfoAnchorConfiguration(browser, kAppMenuButton);
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
