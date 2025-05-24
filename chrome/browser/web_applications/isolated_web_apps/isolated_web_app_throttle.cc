// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

// static
void IsolatedWebAppThrottle::MaybeCreateAndAdd(
    content::NavigationThrottleRegistry& registry) {
  if (content::AreIsolatedWebAppsEnabled(registry.GetNavigationHandle()
                                             .GetWebContents()
                                             ->GetBrowserContext())) {
    registry.AddThrottle(std::make_unique<IsolatedWebAppThrottle>(registry));
  }
}

IsolatedWebAppThrottle::IsolatedWebAppThrottle(
    content::NavigationThrottleRegistry& registry)
    : content::NavigationThrottle(registry) {}

IsolatedWebAppThrottle::~IsolatedWebAppThrottle() = default;

IsolatedWebAppThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::WillStartRequest() {
  if (!is_isolated_web_app_navigation()) {
    return PROCEED;
  }

  IwaKeyDistributionInfoProvider& key_distribution_info_provider =
      CHECK_DEREF(IwaKeyDistributionInfoProvider::GetInstance());
  WebAppProvider& provider =
      CHECK_DEREF(WebAppProvider::GetForWebApps(profile()));
  if (provider.is_registry_ready() &&
      key_distribution_info_provider.OnMaybeDownloadedComponentDataReady()
          .is_signaled()) {
    return PROCEED;
  }

  auto initialized_components_barrier =
      base::BarrierClosure(2u, base::BindOnce(&IsolatedWebAppThrottle::Resume,
                                              weak_ptr_factory_.GetWeakPtr()));
  provider.on_registry_ready().Post(FROM_HERE, initialized_components_barrier);
  key_distribution_info_provider.OnMaybeDownloadedComponentDataReady().Post(
      FROM_HERE, initialized_components_barrier);
  return DEFER;
}

Profile* IsolatedWebAppThrottle::profile() {
  return Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
}

bool IsolatedWebAppThrottle::is_isolated_web_app_navigation() {
  return content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      profile(), navigation_handle()->GetURL());
}

const char* IsolatedWebAppThrottle::GetNameForLogging() {
  return "IsolatedWebAppThrottle";
}

}  // namespace web_app
