// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_side_panel_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "components/lens/lens_features.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"

namespace lens {

// static
std::unique_ptr<content::NavigationThrottle>
LensOverlaySidePanelNavigationThrottle::MaybeCreateFor(
    content::NavigationHandle* handle) {
  // We only want to handle navigations within the side panel results frame, so
  // we can ignore all navigations to a primary main frame. We can also ignore
  // all navigations that don't occur one level down (e.g. children of iframes
  // in the WebUI).
  if (handle->IsInPrimaryMainFrame() || !handle->GetParentFrame() ||
      !handle->GetParentFrame()->IsInPrimaryMainFrame()) {
    return nullptr;
  }

  auto* controller = LensOverlayController::GetControllerFromWebViewWebContents(
      handle->GetWebContents());
  // Only create the navigation throttle for this handle if it equals the side
  // panel web contents and the side panel is showing the lens overlay results
  // entry.
  if (controller && controller->side_panel_coordinator() &&
      controller->side_panel_coordinator()->IsEntryShowing() &&
      (handle->GetWebContents() ==
       controller->side_panel_coordinator()->GetSidePanelWebContents())) {
    return base::WrapUnique(new LensOverlaySidePanelNavigationThrottle(handle));
  }

  return nullptr;
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
    content::NavigationHandle* navigation_handle)
    : NavigationThrottle(navigation_handle) {}

LensOverlaySidePanelNavigationThrottle::ThrottleCheckResult
LensOverlaySidePanelNavigationThrottle::HandleSidePanelRequest() {
  const auto& url = navigation_handle()->GetURL();
  auto params =
      content::OpenURLParams::FromNavigationHandle(navigation_handle());
  // All user clicks to a destination outside of the results search URL
  // should be handled by the side panel coordinator.
  if (!lens::IsValidSearchResultsUrl(url)) {
    return content::NavigationThrottle::CANCEL;
  }

  // The navigation is to a search URL. Get the text query from the URL and set
  // it as the input text on the searchbox.
  auto* controller = LensOverlayController::GetControllerFromWebViewWebContents(
      navigation_handle()->GetWebContents());
  const std::string text_query = GetTextQueryParameterValue(url);
  if (!text_query.empty()) {
    controller->SetSearchboxInputText(text_query);
  }

  // If this is a same-site navigation and search URL, we make sure that the URL
  // has the parameters needed to preserve lens overlay features (e.g. framing).
  // If no such parameters were needed, we can just proceed.
  if (lens::HasCommonSearchQueryParameters(url)) {
    // This is the only time we should add to the search query history stack for
    // a user navigating to a SRP. If the SRP url did not have the common search
    // query parameters, it will reload the frame and go through this flow
    // anyway.
    controller->AddQueryToHistory(std::move(text_query),
                                  navigation_handle()->GetURL());
    return content::NavigationThrottle::PROCEED;
  }

  // If this is a same site navigation and search URL that does not have common
  // search parameters, we need to append them to the URL and then load it
  // manually into the side panel frame.
  auto url_with_params = lens::AppendCommonSearchParametersToURL(url);
  controller->LoadURLInResultsFrame(url_with_params);
  return content::NavigationThrottle::CANCEL;
}
}  // namespace lens
