// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_throttle.h"

#include "base/memory/ptr_util.h"
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

    security_interstitials::SecurityInterstitialPage* blocking_page = nullptr;
#if !BUILDFLAG(IS_ANDROID)
    if (resource.threat_type ==
        SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_WARN) {
      blocking_page =
          manager_->blocking_page_factory()->CreateEnterpriseWarnPage(
              manager_, handle->GetWebContents(), handle->GetURL(), {resource});

      manager_->ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
          handle->GetWebContents(), handle->GetURL(), "ENTERPRISE_WARNED_SEEN",
          resource.rt_lookup_response);
    } else if (resource.threat_type ==
               SBThreatType::SB_THREAT_TYPE_MANAGED_POLICY_BLOCK) {
      blocking_page =
          manager_->blocking_page_factory()->CreateEnterpriseBlockPage(
              manager_, handle->GetWebContents(), handle->GetURL(), {resource});

      manager_->ForwardUrlFilteringInterstitialExtensionEventToEmbedder(
          handle->GetWebContents(), handle->GetURL(), "ENTERPRISE_BLOCKED_SEEN",
          resource.rt_lookup_response);
    } else {
      blocking_page = manager_->blocking_page_factory()->CreateSafeBrowsingPage(
          manager_, handle->GetWebContents(), handle->GetURL(), {resource},
          true);

      manager_->ForwardSecurityInterstitialShownExtensionEventToEmbedder(
          handle->GetWebContents(), handle->GetURL(),
          SafeBrowsingUIManager::GetThreatTypeStringForInterstitial(
              resource.threat_type),
          /*net_error_code=*/0);
    }

#else

    blocking_page = manager_->blocking_page_factory()->CreateSafeBrowsingPage(
        manager_, handle->GetWebContents(), handle->GetURL(), {resource}, true);

    manager_->ForwardSecurityInterstitialShownExtensionEventToEmbedder(
        handle->GetWebContents(), handle->GetURL(),
        SafeBrowsingUIManager::GetThreatTypeStringForInterstitial(
            resource.threat_type),
        /*net_error_code=*/0);
#endif

    std::string error_page_content = blocking_page->GetHTMLContents();
    security_interstitials::SecurityInterstitialTabHelper::
        AssociateBlockingPage(handle, base::WrapUnique(blocking_page));

    return content::NavigationThrottle::ThrottleCheckResult(
        CANCEL, net::ERR_BLOCKED_BY_CLIENT, error_page_content);
  }

  return content::NavigationThrottle::PROCEED;
}

}  // namespace safe_browsing
