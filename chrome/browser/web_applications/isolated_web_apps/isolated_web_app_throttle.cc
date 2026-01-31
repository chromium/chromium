// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/url_loading/url_loader_factory.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"
#include "url/origin.h"

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

  ChromeIwaRuntimeDataProvider& key_distribution_info_provider =
      ChromeIwaRuntimeDataProvider::GetInstance();
  WebAppProvider& provider =
      CHECK_DEREF(WebAppProvider::GetForWebApps(profile()));

  if (provider.is_registry_ready() &&
      key_distribution_info_provider.OnBestEffortRuntimeDataReady()
          .is_signaled()) {
    if (NeedsManifestFetch()) {
      const auto iwa_origin = IwaOrigin::Create(navigation_handle()->GetURL());
      // This is checked already in NeedsManifestFetch.
      CHECK(iwa_origin.has_value());
      IwaPermissionsPolicyCacheFactory::GetForProfile(profile())
          ->ObtainManifestAndCache(
              *iwa_origin,
              base::BindOnce(&IsolatedWebAppThrottle::OnCachePopulated,
                             weak_ptr_factory_.GetWeakPtr()));
      return DEFER;
    }
    return PROCEED;
  }

  auto barrier = base::BarrierClosure(
      2u, base::BindOnce(&IsolatedWebAppThrottle::OnComponentsReady,
                         weak_ptr_factory_.GetWeakPtr()));
  provider.on_registry_ready().Post(FROM_HERE, barrier);
  key_distribution_info_provider.OnBestEffortRuntimeDataReady().Post(FROM_HERE,
                                                                     barrier);
  return DEFER;
}

void IsolatedWebAppThrottle::OnComponentsReady() {
  if (NeedsManifestFetch()) {
    IwaPermissionsPolicyCacheFactory::GetForProfile(profile())
        ->ObtainManifestAndCache(
            *IwaOrigin::Create(navigation_handle()->GetURL()),
            base::BindOnce(&IsolatedWebAppThrottle::OnCachePopulated,
                           weak_ptr_factory_.GetWeakPtr()));
  } else {
    Resume();
  }
}

bool IsolatedWebAppThrottle::NeedsManifestFetch() const {
  const auto iwa_origin = IwaOrigin::Create(navigation_handle()->GetURL());
  // There are navigations involved in processing the bundle data for
  // installations/updates/metadata reading, and caching the manifest then
  // would not be a good idea. In particular, this is not a good place for
  // catching manifest-related issues during the installation.
  return iwa_origin.has_value() &&
         !NonInstalledBundleInspectionContext::FromWebContents(
             navigation_handle()->GetWebContents()) &&
         !IwaPermissionsPolicyCacheFactory::GetForProfile(profile())->GetPolicy(
             *iwa_origin);
}

void IsolatedWebAppThrottle::OnCachePopulated(bool success) {
  if (success) {
    Resume();
  } else {
    // Those are essentially cases of the malformed manifest. They shouldn't
    // happen ever—else the IWA shouldn't even get installed in the first
    // place—and would most likely signify a corrupted installation.
    CancelDeferredNavigation(content::NavigationThrottle::CANCEL);
  }
}

Profile* IsolatedWebAppThrottle::profile() const {
  return Profile::FromBrowserContext(
      navigation_handle()->GetWebContents()->GetBrowserContext());
}

bool IsolatedWebAppThrottle::is_isolated_web_app_navigation() const {
  return content::SiteIsolationPolicy::ShouldUrlUseApplicationIsolationLevel(
      profile(), navigation_handle()->GetURL());
}

const char* IsolatedWebAppThrottle::GetNameForLogging() {
  return "IsolatedWebAppThrottle";
}

}  // namespace web_app
