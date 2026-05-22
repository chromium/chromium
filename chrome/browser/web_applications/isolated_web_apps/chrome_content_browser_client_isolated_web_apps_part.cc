// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_content_browser_client_isolated_web_apps_part.h"

#include "base/command_line.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/public/header_utils.h"
#include "components/webapps/isolated_web_apps/scheme.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "components/webapps/isolated_web_apps/types/source.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/content_security_policy/content_security_policy.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/cross_origin_embedder_policy.mojom-shared.h"
#include "services/network/public/mojom/cross_origin_opener_policy.mojom-shared.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/browser_context_helper/browser_context_types.h"
#endif

namespace web_app {

namespace {

BASE_FEATURE(kIsolatedWebAppEnsureRequiredHeadersOnNavigationAndWorkers,
             base::FEATURE_ENABLED_BY_DEFAULT);

std::optional<IwaSourceWithMode> GetIwaSource(
    content::BrowserContext* browser_context,
    const GURL& url) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  auto info = IsolatedWebAppUrlInfo::Create(url);
  if (!info.has_value()) {
    return std::nullopt;
  }

  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile)->registrar_unsafe();
  const WebApp* iwa =
      registrar.GetAppById(info->app_id(), WebAppFilter::IsIsolatedApp());
  if (!iwa) {
    return std::nullopt;
  }

  return IwaSourceWithMode::FromStorageLocation(
      profile->GetPath(), iwa->isolation_data()->location());
}

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
void ChromeContentBrowserClientIsolatedWebAppsPart::
    EnsureRequiredHeadersForIsolatedApp(
        content::BrowserContext* browser_context,
        const GURL& url,
        network::mojom::URLResponseHead* response_head,
        const std::optional<content::FrameTreeNodeId>& frame_tree_node) {
  if (!content::AreIsolatedWebAppsEnabled(browser_context)) {
    return;
  }
  if (!base::FeatureList::IsEnabled(
          kIsolatedWebAppEnsureRequiredHeadersOnNavigationAndWorkers)) {
    return;
  }

  std::optional<web_app::IwaSourceWithMode> iwa_source;

  if (frame_tree_node) {
    if (auto* web_contents =
            content::WebContents::FromFrameTreeNodeId(*frame_tree_node)) {
      if (auto* inspection_context =
              NonInstalledBundleInspectionContext::FromWebContents(
                  web_contents)) {
        if (url.GetPath() == kInstallPagePath) {
          // IWA installation page does not need headers.
          return;
        }
        // Take IWA source from NonInstalledBundleInspectionContext,
        // since look up via WebAppRegistrar would fail for non yet installed
        // IWA.
        iwa_source = inspection_context->source();
      }
    }
  }

  if (!iwa_source) {
    iwa_source = GetIwaSource(browser_context, url);
  }

  // Modify raw headers.
  // Mostly values are taken from parsed headers, but sometimes
  // headers also accessed, thus they are modified too for consistency.
  if (auto* headers = response_head->headers.get()) {
    web_app::iwa::SetRequiredHeadersForIsolatedApp(iwa_source, *headers);
  }

  // Modify parsed headers.
  if (auto* parsed_headers = response_head->parsed_headers.get()) {
    web_app::iwa::SetRequiredParsedHeadersForIsolatedApp(iwa_source,
                                                         *parsed_headers, url);
  }
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
