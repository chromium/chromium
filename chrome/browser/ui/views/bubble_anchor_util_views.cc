// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bubble_anchor_util_views.h"

#include <optional>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/bubble_anchor_util.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#include "chrome/browser/ui/views/frame/app_menu_button.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/picture_in_picture_browser_frame_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_view.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "components/payments/core/features.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_highlighter.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/interaction/element_tracker_views.h"

// This file contains the bubble_anchor_util implementation for a Views
// browser window (BrowserView).

namespace bubble_anchor_util {

namespace {

// Returns the permission prompt anchor for a `WebContents` whose location bar
// is overridden via `LocationBarOverrideData` (e.g. the Contextual Tasks side
// panel). Only `location_bar` is consulted; this never falls back to the
// hosting window's omnibox, since anchoring there would attribute the prompt
// to the wrong surface. `location_bar` is null if it was destroyed before the
// `WebContents`.
AnchorConfiguration GetOverrideAnchorConfiguration(LocationBar* location_bar) {
  if (!location_bar || !location_bar->IsDrawn()) {
    return {};
  }

  ChipController* chip_controller = location_bar->GetChipController();
  if (chip_controller && chip_controller->IsPermissionPromptChipVisible()) {
    if (std::optional<AnchorConfiguration> chip_anchor =
            location_bar->GetChipAnchor()) {
      return *chip_anchor;
    }
  }

  // Don't highlight by element ID here: IDs such as `kLocationIconElementId`
  // would resolve to the hosting window's omnibox, not the override surface.
  if (ui::TrackedElement* element = location_bar->GetAnchorOrNull()) {
    return {views::BubbleAnchor(element), std::nullopt,
            views::BubbleBorder::TOP_LEFT};
  }

  return {};
}

// Returns the window hosting `web_contents`: the tab's window, or for a non-tab
// WebUI (e.g. the omnibox popup or the Contextual Tasks side panel) the window
// it is embedded in.
BrowserWindowInterface* GetBrowserForWebContents(
    content::WebContents* web_contents) {
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents);
  return tab ? tab->GetBrowserWindowInterface()
             : webui::GetBrowserWindowInterface(web_contents);
}

// Returns a zero-size rect, in screen coordinates, at the top edge of
// `web_contents`, inset from its leading edge like `GetPageInfoAnchorRect()`
// is from the window's.
gfx::Rect GetContentsTopLeadingRect(content::WebContents* web_contents) {
  const gfx::Rect bounds = web_contents->GetContainerBounds();
  const int x = base::i18n::IsRTL() ? bounds.right() - kNoToolbarLeftOffset
                                    : bounds.x() + kNoToolbarLeftOffset;
  return gfx::Rect(x, bounds.y(), 0, 0);
}

}  // namespace

AnchorConfiguration GetPageInfoAnchorConfiguration(
    BrowserWindowInterface* browser,
    Anchor anchor) {
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  auto* location_bar_view =
      browser_view ? browser_view->GetLocationBarView() : nullptr;

  if (location_bar_view) {
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
    auto chip_anchor =
        BrowserWindow::FromBrowser(browser)->GetLocationBar()->GetChipAnchor();
    if (anchor == Anchor::kLocationBar && chip_anchor) {
      return *chip_anchor;
    }
  }

  if (anchor == Anchor::kLocationBar) {
    LocationBar* location_bar =
        browser_view ? browser_view->GetLocationBar() : nullptr;
    if (location_bar && location_bar->IsDrawn()) {
      if (ui::TrackedElement* element = location_bar->GetAnchorOrNull()) {
        return {views::BubbleAnchor(element), kLocationIconElementId,
                views::BubbleBorder::TOP_LEFT};
      }
    }
  }

  if (anchor == Anchor::kLocationBar && browser_view->GetIsFloatingType()) {
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
    content::WebContents* web_contents) {
  CHECK(web_contents);

  // A `WebContents` with an overridden location bar is hosted outside the
  // window's tab strip, so its prompts must anchor to that location bar only.
  // `webui::GetBrowserWindowInterface()` below would otherwise resolve to the
  // containing window and anchor to its omnibox.
  if (auto* override_data =
          location_bar::LocationBarOverrideData::FromWebContents(
              web_contents)) {
    return GetOverrideAnchorConfiguration(override_data->GetLocationBar());
  }

  BrowserWindowInterface* browser = GetBrowserForWebContents(web_contents);
  if (browser) {
    // Overridden location bars are handled above, so use the window's own
    // location bar. This includes non-tab WebUIs embedded in the window (e.g.
    // the omnibox popup), for which `GetLocationBarForWebContents()` returns
    // null.
    BrowserWindow* window = BrowserWindow::FromBrowser(browser);
    LocationBar* location_bar = window ? window->GetLocationBar() : nullptr;
    ChipController* chip_controller =
        location_bar ? location_bar->GetChipController() : nullptr;
    if (chip_controller && chip_controller->IsPermissionPromptChipVisible()) {
      if (auto chip_anchor = location_bar->GetChipAnchor()) {
        return *chip_anchor;
      }
    }
    // TODO(crbug.com/565828687): Clean up location bar anchor resolution so
    // there is a single source of truth between this and
    // `GetPageInfoAnchorConfiguration()`.
    return GetPageInfoAnchorConfiguration(browser);
  }

  // For for non-Browser hosted PiP windows anchor to the location icon.
  auto* pip_host = DocumentPipHost::FromChildWebContents(web_contents);
  if (pip_host && pip_host->GetWidget()) {
    views::View* location_icon =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kLocationIconElementId,
            views::ElementTrackerViews::GetContextForWidget(
                pip_host->GetWidget()));
    return {views::BubbleAnchor(location_icon), kLocationIconElementId,
            views::BubbleBorder::TOP_LEFT};
  }

  // For Payment Handler windows anchor to the header PageInfo icon.
  if (base::FeatureList::IsEnabled(
          payments::features::kPaymentHandlerCameraAccessUx)) {
    if (auto* payment_handler =
            payments::PaymentHandlerWebFlowViewController::FromWebContents(
                web_contents)) {
      if (views::View* page_info_icon =
              payment_handler->GetPageInfoIconView()) {
        return {
            views::BubbleAnchor(page_info_icon),
            payments::PaymentHandlerWebFlowViewController::kAppIconElementId,
            views::BubbleBorder::TOP_LEFT};
      }
    }
  }

  return {};
}

gfx::Rect GetPermissionPromptBubbleAnchorRect(
    content::WebContents* web_contents) {
  CHECK(web_contents);

  // As in `GetPermissionPromptBubbleAnchorConfiguration()`, never position a
  // prompt for an overridden location bar relative to the hosting window's
  // toolbar.
  if (location_bar::LocationBarOverrideData::FromWebContents(web_contents)) {
    return GetContentsTopLeadingRect(web_contents);
  }

  // `GetPageInfoAnchorRect()` requires a browser window. A `WebContents` that
  // isn't hosted in one (e.g. Omnibox Everywhere while the profile has no
  // browser window) is positioned relative to its own bounds instead.
  BrowserWindowInterface* browser = GetBrowserForWebContents(web_contents);
  if (!browser) {
    return GetContentsTopLeadingRect(web_contents);
  }
  return GetPageInfoAnchorRect(browser);
}

AnchorConfiguration GetAppMenuAnchorConfiguration(
    BrowserWindowInterface* browser) {
  return GetPageInfoAnchorConfiguration(browser, Anchor::kAppMenuButton);
}

gfx::Rect GetPageInfoAnchorRect(BrowserWindowInterface* browser) {
  // GetPageInfoAnchorConfiguration()'s anchor should be preferred if
  // available.
  CHECK(GetPageInfoAnchorConfiguration(browser).anchor.IsNull(),
        base::NotFatalUntil::M162);

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
