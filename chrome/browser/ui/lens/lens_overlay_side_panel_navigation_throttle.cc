// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"

namespace lens {

// static
void LensOverlaySidePanelNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry,
    ThemeService* theme_service) {
  // We only want to handle navigations within the side panel results frame, we
  // can ignore all navigations that don't occur one level down (e.g. children
  // of iframes in the WebUI). However, since the top level frame hosts the
  // WebUI, we should also handle those navigations within this throttle to
  // prevent breakages.
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  if (!handle.IsInPrimaryMainFrame() &&
      (!handle.GetParentFrame() ||
       !handle.GetParentFrame()->IsInPrimaryMainFrame())) {
    return;
  }

  LensSearchController* controller =
      LensSearchController::FromWebUIWebContents(handle.GetWebContents());
  if (!controller || controller->IsOff()) {
    return;
  }

  auto* lens_overlay_side_panel_coordinator =
      controller->lens_overlay_side_panel_coordinator();
  // Only create the navigation throttle for this handle if it equals the side
  // panel web contents and the side panel web contents is not null. The entry
  // does not need to be showing as it's possible a new tab was opened that hid
  // the side panel. In that case, we still want to be able to show the correct
  // URL in the side panel should the user return.
  if (lens_overlay_side_panel_coordinator->GetSidePanelWebContents() &&
      (handle.GetWebContents() ==
       lens_overlay_side_panel_coordinator->GetSidePanelWebContents())) {
    registry.AddThrottle(base::WrapUnique(
        new LensOverlaySidePanelNavigationThrottle(registry, theme_service)));
  }
}

LensOverlaySidePanelNavigationThrottle::ThrottleCheckResult
LensOverlaySidePanelNavigationThrottle::WillStartRequest() {
  return HandleSidePanelRequest();
}

LensOverlaySidePanelNavigationThrottle::ThrottleCheckResult
LensOverlaySidePanelNavigationThrottle::WillRedirectRequest() {
  return HandleSidePanelRequest();
}

const char* LensOverlaySidePanelNavigationThrottle::GetNameForLogging() {
  return "LensOverlaySidePanelNavigationThrottle";
}

LensOverlaySidePanelNavigationThrottle::LensOverlaySidePanelNavigationThrottle(
    content::NavigationThrottleRegistry& registry,
    ThemeService* theme_service)
    : NavigationThrottle(registry), theme_service_(theme_service) {}

LensOverlaySidePanelNavigationThrottle::ThrottleCheckResult
LensOverlaySidePanelNavigationThrottle::HandleSidePanelRequest() {
  const auto& url = navigation_handle()->GetURL();
  auto params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());

  LensSearchController* controller = LensSearchController::FromWebUIWebContents(
      navigation_handle()->GetWebContents());
  if (!controller || controller->IsOff()) {
    return content::NavigationThrottle::CANCEL;
  }

  auto* lens_overlay_side_panel_coordinator =
      controller->lens_overlay_side_panel_coordinator();
  // If the URL is a redirect to a search URL, we want to load it directly in
  // the side panel.
  GURL redirect_url = lens::GetSearchResultsUrlFromRedirectUrl(url);
  if (!redirect_url.is_empty()) {
    lens_overlay_side_panel_coordinator->LoadURLInResultsFrame(redirect_url);
    return content::NavigationThrottle::CANCEL;
  }

  // All user clicks to a destination outside of the results search URL
  // should be handled by the side panel coordinator, or if the search URL is
  // not supported in the side panel it should also be handled by the side panel
  // coordinator so it can open in a new tab without changing the
  // loading/offline state.
  auto* const profile =
      controller->GetTabInterface()->GetBrowserWindowInterface()->GetProfile();
  if (!lens::IsValidSearchResultsUrl(url) ||
      ShouldOpenSearchURLInNewTab(url, lens::IsAimM3Enabled(profile))) {
    return content::NavigationThrottle::CANCEL;
  }

  // If the URL is a valid search results URL and has a text directive, then
  // the side panel coordinator should handle the navigation and open it either
  // in a new tab or highlight the text in the current tab if the URL is already
  // open.
  if (lens_overlay_side_panel_coordinator->MaybeHandleTextDirectives(url)) {
    return content::NavigationThrottle::CANCEL;
  }

  // If this is a same-site navigation and search URL, we make sure that the URL
  // has the parameters needed to preserve lens overlay features (e.g. framing).
  // If no such parameters were needed, we can just proceed.
  if (lens::HasCommonSearchQueryParameters(url)) {
    // This is the only time a query is guaranteed to end up in the side panel
    // for a user navigation.If the SRP url did not have the common search query
    // parameters, it will reload the frame and go through this flow anyway.
    const std::string text_query = ExtractTextQueryParameterValue(url);
    lens_overlay_side_panel_coordinator->NotifyNewQueryLoaded(
        std::move(text_query), navigation_handle()->GetURL());
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a same site navigation and search URL that does not have common
  // search parameters, we need to append them to the URL and then load it
  // manually into the side panel frame.
  auto url_with_params = lens::AppendCommonSearchParametersToURL(
      url, lens::LensOverlayShouldUseDarkMode(theme_service_));
  lens_overlay_side_panel_coordinator->LoadURLInResultsFrame(url_with_params);
  return content::NavigationThrottle::CANCEL;
}
}  // namespace lens
