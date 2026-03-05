// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search/chrome_search_navigation_throttle.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle_registry.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

ChromeSearchNavigationThrottle::ChromeSearchNavigationThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

ChromeSearchNavigationThrottle::~ChromeSearchNavigationThrottle() = default;

const char* ChromeSearchNavigationThrottle::GetNameForLogging() {
  return "ChromeSearchNavigationThrottle";
}

content::NavigationThrottle::ThrottleCheckResult
ChromeSearchNavigationThrottle::WillStartRequest() {
  Profile* profile = Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  InstantService* instant_service =
      InstantServiceFactory::GetForProfile(profile);
  CHECK(instant_service);
  content::SiteInstance* source = navigation_handle()->GetSourceSiteInstance();
  content::RenderProcessHost* initiator_process = source->GetProcess();
  if (!initiator_process || !instant_service->IsInstantProcess(
                                initiator_process->GetDeprecatedID())) {
    return ThrottleCheckResult(BLOCK_REQUEST, net::ERR_BLOCKED_BY_CLIENT);
  }
  return PROCEED;
}

// static
void ChromeSearchNavigationThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  content::NavigationHandle& handle = registry.GetNavigationHandle();
  // Only add the throttle when we might need to block renderer-initiated
  // navigation to chrome-search with a profile that has InstantService.
  // This avoids running the throttle for every navigation.
  if (!handle.GetURL().SchemeIs(chrome::kChromeSearchScheme) ||
      !handle.IsRendererInitiated()) {
    return;
  }
  Profile* profile =
      Profile::FromBrowserContext(handle.GetWebContents()->GetBrowserContext());
  InstantService* instant_service =
      profile ? InstantServiceFactory::GetForProfile(profile) : nullptr;
  if (!instant_service) {
    return;
  }
  registry.AddThrottle(
      std::make_unique<ChromeSearchNavigationThrottle>(registry));
}
