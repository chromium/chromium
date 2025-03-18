// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_throttle.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/site_isolation_mode.h"
#include "content/public/browser/site_isolation_policy.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

std::unique_ptr<content::NavigationThrottle>
IsolatedWebAppThrottle::MaybeCreateThrottleFor(
    content::NavigationHandle* handle) {
  if (content::AreIsolatedWebAppsEnabled(
          handle->GetWebContents()->GetBrowserContext())) {
    return std::make_unique<IsolatedWebAppThrottle>(handle);
  }
  return nullptr;
}

IsolatedWebAppThrottle::IsolatedWebAppThrottle(
    content::NavigationHandle* handle)
    : content::NavigationThrottle(handle) {}

IsolatedWebAppThrottle::~IsolatedWebAppThrottle() = default;

IsolatedWebAppThrottle::ThrottleCheckResult
IsolatedWebAppThrottle::WillStartRequest() {
  if (!is_isolated_web_app_navigation()) {
    return PROCEED;
  }

  WebAppProvider* provider = WebAppProvider::GetForWebApps(profile());
  if (provider->is_registry_ready()) {
    return PROCEED;
  }

  provider->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&IsolatedWebAppThrottle::Resume,
                                weak_ptr_factory_.GetWeakPtr()));
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
