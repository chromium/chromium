// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/content_settings/core/common/features.h"
#include "ui/base/interaction/element_highlighter.h"
#include "ui/views/bubble/bubble_border.h"

// This file contains the bubble_anchor_util implementation for a Views
// browser window (BrowserView).

namespace bubble_anchor_util {

AnchorConfiguration GetPageInfoAnchorConfiguration(Browser* browser,
                                                   Anchor anchor) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  auto* location_bar_view =
      browser_view ? browser_view->GetLocationBarView() : nullptr;

  if (base::FeatureList::IsEnabled(
          content_settings::features::kLeftHandSideActivityIndicators) &&
      location_bar_view) {
    auto* permission_dashboard_view =
        location_bar_view->permission_dashboard_view();

    if (anchor == Anchor::kLocationBar &&
        permission_dashboard_view->GetVisible()) {
      if (permission_dashboard_view->GetIndicatorChip()->GetVisible()) {
        return {
            views::BubbleAnchor(permission_dashboard_view->GetIndicatorChip()),
            PermissionChipView::kIndicatorChipElementId,
            views::BubbleBorder::TOP_LEFT};
      }

      return {views::BubbleAnchor(permission_dashboard_view->GetRequestChip()),
              PermissionChipView::kPermissionRequestChipElementId,
              views::BubbleBorder::TOP_LEFT};
    }
  } else {
    auto chip_anchor = browser->window()->GetLocationBar()->GetChipAnchor();
    if (anchor == Anchor::kLocationBar && chip_anchor) {
      return *chip_anchor;
    }
  }

  if (anchor == Anchor::kLocationBar && location_bar_view->IsDrawn()) {
    return {views::BubbleAnchor(location_bar_view), kLocationIconElementId,
            views::BubbleBorder::TOP_LEFT};
  }

  if (anchor == Anchor::kLocationBar &&
      browser_view->GetIsPictureInPictureType()) {
    auto* frame_view = static_cast<PictureInPictureBrowserFrameView*>(
        browser_view->browser_widget()->GetFrameView());
    return {views::BubbleAnchor(frame_view->GetLocationIconView()),
            kLocationIconElementId, views::BubbleBorder::TOP_LEFT};
  }

  if (anchor == Anchor::kCustomTabBar &&
      browser_view->toolbar()->custom_tab_bar()) {
    return {views::BubbleAnchor(browser_view->toolbar()->custom_tab_bar()),
            kLocationIconElementId, views::BubbleBorder::TOP_LEFT};
  }

  // Fall back to menu button.
  auto* app_menu_control =
      browser_view->toolbar_button_provider()->GetAppMenuControl();

  // If the anchor is a physical View (like the button), it must be visible on
  // screen. If it's hidden (e.g. during fullscreen), return empty so the
  // bubble falls back to a default corner position instead of floating at
  // (0,0).
  if (!app_menu_control || !app_menu_control->IsDrawn()) {
    return {};
  }

  views::BubbleAnchor app_menu_anchor = app_menu_control->GetAnchor();

  // If the anchor is explicitly null, we can't anchor to it.
  if (app_menu_anchor.IsNull()) {
    return {};
  }

  // The app menu button is not visible when immersive mode is enabled and the
  // title bar is not revealed. So return null anchor configuration.
  auto* const controller = ImmersiveModeController::From(browser);
  if (controller->IsEnabled() && !controller->IsRevealed()) {
    return {};
  }

  return {app_menu_anchor, kToolbarAppMenuButtonElementId,
          views::BubbleBorder::TOP_RIGHT};
}

AnchorConfiguration GetPermissionPromptBubbleAnchorConfiguration(
    Browser* browser) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (browser_view && browser_view->GetLocationBar()->GetChipController() &&
      browser_view->GetLocationBar()
          ->GetChipController()
          ->IsPermissionPromptChipVisible()) {
    ui::TrackedElement* tracked_element =
        browser_view->GetLocationBar()->GetAnchorOrNull();
    if (tracked_element) {
      return {views::BubbleAnchor(tracked_element),
              PermissionChipView::kPermissionRequestChipElementId,
              views::BubbleBorder::TOP_LEFT};
    }
  }
  return GetPageInfoAnchorConfiguration(browser);
}

AnchorConfiguration GetAppMenuAnchorConfiguration(Browser* browser) {
  return GetPageInfoAnchorConfiguration(browser, Anchor::kAppMenuButton);
}

gfx::Rect GetPageInfoAnchorRect(Browser* browser) {
  // GetPageInfoAnchorConfiguration()'s anchor should be preferred if
  // available.
  DCHECK(GetPageInfoAnchorConfiguration(browser).anchor.IsNull());

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

// Returns true if the given anchor can be used as a highlight.
bool IsHighlightable(views::BubbleAnchor anchor) {
  if (views::View* view_anchor = anchor.GetIfView()) {
    return views::Button::AsButton(view_anchor);
  } else if (ui::TrackedElement* element_anchor = anchor.GetIfElement()) {
    return ui::ElementHighlighter::GetElementHighlighter()->CanBeHighlighted(
        element_anchor);
  } else {
    // nullptr isn't highlightable.
    return false;
  }
}

}  // namespace bubble_anchor_util
