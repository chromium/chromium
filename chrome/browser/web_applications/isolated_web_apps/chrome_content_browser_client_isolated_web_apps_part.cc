// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_content_browser_client_isolated_web_apps_part.h"

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace web_app {

namespace {

using PermissionsPolicyCacheEntry = IwaPermissionsPolicyCache::CacheEntry;
using PermissionPolicyEntryPtr =
    blink::mojom::IsolatedAppPermissionPolicyEntryPtr;
using PermissionPolicyEntry = blink::mojom::IsolatedAppPermissionPolicyEntry;

}  // namespace

ChromeContentBrowserClientIsolatedWebAppsPart::
    ChromeContentBrowserClientIsolatedWebAppsPart() = default;
ChromeContentBrowserClientIsolatedWebAppsPart::
    ~ChromeContentBrowserClientIsolatedWebAppsPart() = default;

// static
std::vector<PermissionPolicyEntryPtr>
ChromeContentBrowserClientIsolatedWebAppsPart::
    GetBaselinePermissionsPolicyForIsolatedWebApp(
        content::BrowserContext* browser_context,
        const url::Origin& iwa_origin) {
  if (!content::AreIsolatedWebAppsEnabled(browser_context)) {
    return {};
  }

  ASSIGN_OR_RETURN(
      IwaOrigin origin, IwaOrigin::Create(iwa_origin.GetURL()),
      [](auto) { return std::vector<PermissionPolicyEntryPtr>(); });

  Profile* profile = Profile::FromBrowserContext(browser_context);

  const PermissionsPolicyCacheEntry* policy =
      IwaPermissionsPolicyCacheFactory::GetForProfile(profile)->GetPolicy(
          origin);
  if (!policy) {
    return {};
  }

  return base::ToVector(*policy, [](const auto& entry) {
    return PermissionPolicyEntry::New(entry.feature, entry.allowed_origins);
  });
}

// static
bool ChromeContentBrowserClientIsolatedWebAppsPart::AreIsolatedWebAppsEnabled(
    content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  if (!AreWebAppsEnabled(profile) || profile->IsGuestSession() ||
      profile->IsOffTheRecord()) {
    return false;
  }

#if BUILDFLAG(IS_CHROMEOS)
  // IWAs should be enabled for ShimlessRMA app profile.
  if (ash::IsShimlessRmaAppBrowserContext(browser_context)) {
    return true;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  if (base::FeatureList::IsEnabled(features::kIsolatedWebApps)) {
    return true;
  }

  return false;
}

void ChromeContentBrowserClientIsolatedWebAppsPart::
    AppendExtraRendererCommandLineSwitches(
        base::CommandLine* command_line,
        content::RenderProcessHost& process) {
  if (!content::AreIsolatedWebAppsEnabled(process.GetBrowserContext())) {
    return;
  }
  command_line->AppendSwitch(switches::kEnableIsolatedWebAppsInRenderer);
}

}  // namespace web_app
