// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_throttle.h"

#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_functions.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page.h"
#include "components/safe_browsing/content/browser/safe_browsing_blocking_page_factory.h"
#include "components/safe_browsing/content/browser/ui_manager.h"
#include "components/security_interstitials/content/security_interstitial_tab_helper.h"
#include "components/security_interstitials/core/unsafe_resource.h"
#include "content/public/browser/navigation_handle.h"

namespace safe_browsing {

// static
std::unique_ptr<content::NavigationThrottle>
SafeBrowsingNavigationThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle,
    SafeBrowsingUIManager* ui_manager) {
  if (!ui_manager)
    return nullptr;

  // Only outer-most main frames show the interstitial through the navigation
  // throttle. In other cases, the interstitial is shown via
  // BaseUIManager::DisplayBlockingPage.
  if (!handle->IsInPrimaryMainFrame() && !handle->IsInPrerenderedMainFrame())
    return nullptr;

  return base::WrapUnique(
      new SafeBrowsingNavigationThrottle(handle, ui_manager));
}

SafeBrowsingNavigationThrottle::SafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle,
    SafeBrowsingUIManager* manager)
    : content::NavigationThrottle(handle), manager_(manager) {}

const char* SafeBrowsingNavigationThrottle::GetNameForLogging() {
  return "SafeBrowsingNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
SafeBrowsingNavigationThrottle::WillFailRequest() {
  DCHECK(manager_);

  // Goes over |RedirectChain| to get the severest threat information
  security_interstitials::UnsafeResource resource;
  content::NavigationHandle* handle = navigation_handle();
  ThreatSeverity severity =
      manager_->GetSeverestThreatForNavigation(handle, resource);

  // Unsafe resource will show a blocking page
  if (severity != std::numeric_limits<ThreatSeverity>::max() &&
      resource.threat_type != SBThreatType::SB_THREAT_TYPE_SAFE) {
    // Subframes and nested frame trees will show an interstitial directly
    // from BaseUIManager::DisplayBlockingPage.
    DCHECK(handle->IsInPrimaryMainFrame() ||
           handle->IsInPrerenderedMainFrame());

    // blocked_page_shown_timestamp is set to nullopt because this blocking
    // page is triggered through navigation throttle, so the blocked page is
    // never shown.
    security_interstitials::SecurityInterstitialPage* blocking_page =
        manager_->CreateBlockingPage(
            handle->GetWebContents(), handle->GetURL(), {resource},
            /*forward_extension_event=*/true,
            /*blocked_page_shown_timestamp=*/std::nullopt);
    std::string error_page_content = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, base::WrapUnique(blocking_page));

    base::UmaHistogramBoolean("SafeBrowsing.NavigationThrottle.IsSameURL",
                              handle->GetURL() == resource.url);

    return content::NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace safe_browsing
