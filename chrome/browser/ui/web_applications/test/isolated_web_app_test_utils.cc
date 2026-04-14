// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/test/isolated_web_app_test_utils.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/containers/map_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/chrome_browser_main.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/install_isolated_web_app_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_install_source.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/iwa_permissions_policy_cache.h"
#include "chrome/browser/web_applications/isolated_web_apps/test/key_distribution/test_utils.h"
#include "chrome/browser/web_applications/test/web_app_test_utils.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_utils.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_registry_update.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "components/webapps/isolated_web_apps/types/iwa_origin.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/navigation_simulator.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/permissions_policy/permissions_policy_declaration.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/common/permissions_policy/policy_helper_public.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "content/public/common/content_features.h"
#endif  // !BUILDFLAG(IS_CHROMEOS)

namespace web_app {
namespace {
// Normally this is done in the `content::IsolatedWebAppThrottle`.
// `NavigationSimulator` however does not run throttles.
void PopulatePermissionsPolicyCache(
    web_app::IwaPermissionsPolicyCache* permissions_policy_cache,
    const IwaOrigin iwa_origin) {
  base::test::TestFuture<bool> future;
  permissions_policy_cache->ObtainManifestAndCache(iwa_origin,
                                                   future.GetCallback());
  CHECK(future.Take());
}

void CommitNavigation(
    std::unique_ptr<content::NavigationSimulator> simulator,
    const web_app::IwaPermissionsPolicyCache::CacheEntry* permissions_policy,
    const url::Origin& origin) {
  // We need to inject the COI headers here because they're normally injected
  // by IsolatedWebAppURLLoader, which is skipped when simulating navigations.
  simulator->SetResponseHeaders(
      net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200 OK")
          .AddHeader("Cross-Origin-Opener-Policy", "same-origin")
          .AddHeader("Cross-Origin-Embedder-Policy", "require-corp")
          .AddHeader("Cross-Origin-Resource-Policy", "same-origin")
          .Build());

  // Normally, parsing IWA permissions policies and combining them with headers
  // is done in the renderer. Here however we essentially simulate the renderer
  // without starting it, so this is a very limited reimplementation of the
  // parsing for tests.
  if (!!permissions_policy) {
    const auto& permission_policy_to_feature_map =
        blink::GetPermissionsPolicyNameToFeatureMap();
    network::ParsedPermissionsPolicy parsed_policy;
    for (const auto& entry : *permissions_policy) {
      if (entry.allowed_origins.empty()) {
        continue;
      }
      const auto* mapping =
          base::FindOrNull(permission_policy_to_feature_map, entry.feature);
      if (!mapping) {
        continue;
      }

      // This is a very basic implementation that doesn't take headers or
      // allowlists into consideration, just picks every policy mentioned in the
      // manifest and sets its allowlist to *. It's good enough for the tests
      // that use this currently, and new tests that set detailed allowlists
      // should prioritize using normal navigation.
      parsed_policy.emplace_back(
          *mapping, std::vector<network::OriginWithPossibleWildcards>{}, origin,
          true, false);
    }
    simulator->SetPermissionsPolicyHeader(std::move(parsed_policy));
  }

  simulator->Commit();
}

}  // namespace

IsolatedWebAppBrowserTestHarness::IsolatedWebAppBrowserTestHarness() {
  // Note: We cannot enable blink::features::kControlledFrame here since there
  // are tests that inherit from this class which depend on being able to start
  // without kControlledFrame in their feature list.
  iwa_scoped_feature_list_.InitWithFeatures(
      {
#if !BUILDFLAG(IS_CHROMEOS)
          features::kIsolatedWebApps,
#endif  // !BUILDFLAG(IS_CHROMEOS)
          features::kIsolatedWebAppDevMode},
      {});
}

IsolatedWebAppBrowserTestHarness::~IsolatedWebAppBrowserTestHarness() = default;

std::unique_ptr<net::EmbeddedTestServer>
IsolatedWebAppBrowserTestHarness::CreateAndStartServer(
    base::FilePath::StringViewType chrome_test_data_relative_root) {
  return CreateAndStartDevServer(chrome_test_data_relative_root);
}

IsolatedWebAppUrlInfo
IsolatedWebAppBrowserTestHarness::InstallDevModeProxyIsolatedWebApp(
    const url::Origin& origin) {
  return web_app::InstallDevModeProxyIsolatedWebApp(profile(), origin);
}

Browser* IsolatedWebAppBrowserTestHarness::GetBrowserFromFrame(
    content::RenderFrameHost* frame) {
  Browser* browser = chrome::FindBrowserWithTab(
      content::WebContents::FromRenderFrameHost(frame));
  EXPECT_TRUE(browser);
  return browser;
}

content::RenderFrameHost* IsolatedWebAppBrowserTestHarness::OpenApp(
    const webapps::AppId& app_id,
    std::optional<std::string_view> path) {
  return OpenIsolatedWebApp(profile(), app_id, path);
}

content::RenderFrameHost*
IsolatedWebAppBrowserTestHarness::NavigateToURLInNewTab(
    Browser* window,
    const GURL& url,
    WindowOpenDisposition disposition) {
  auto new_contents = content::WebContents::Create(
      content::WebContents::CreateParams(browser()->profile()));
  window->tab_strip_model()->AppendWebContents(std::move(new_contents),
                                               /*foreground=*/true);
  return ui_test_utils::NavigateToURLWithDisposition(
      window, url, disposition, ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
}

UpdateDiscoveryTaskResultWaiter::UpdateDiscoveryTaskResultWaiter(
    WebAppProvider& provider,
    const webapps::AppId expected_app_id,
    TaskResultCallback callback)
    : expected_app_id_(expected_app_id),
      callback_(std::move(callback)),
      provider_(provider) {
  observation_.Observe(&provider.isolated_web_app_update_manager());
}

UpdateDiscoveryTaskResultWaiter::~UpdateDiscoveryTaskResultWaiter() = default;

// IsolatedWebAppUpdateManager::Observer:
void UpdateDiscoveryTaskResultWaiter::OnUpdateDiscoveryTaskCompleted(
    const webapps::AppId& app_id,
    IsolatedWebAppUpdateDiscoveryTask::CompletionStatus status) {
  if (app_id != expected_app_id_) {
    return;
  }
  std::move(callback_).Run(status);
  observation_.Reset();
}

UpdateApplyTaskResultWaiter::UpdateApplyTaskResultWaiter(
    WebAppProvider& provider,
    const webapps::AppId expected_app_id,
    TaskResultCallback callback)
    : expected_app_id_(expected_app_id),
      callback_(std::move(callback)),
      provider_(provider) {
  observation_.Observe(&provider.isolated_web_app_update_manager());
}

UpdateApplyTaskResultWaiter::~UpdateApplyTaskResultWaiter() = default;

// IsolatedWebAppUpdateManager::Observer:
void UpdateApplyTaskResultWaiter::OnUpdateApplyTaskCompleted(
    const webapps::AppId& app_id,
    IsolatedWebAppApplyUpdateCommandResult status) {
  if (app_id != expected_app_id_) {
    return;
  }
  std::move(callback_).Run(status);
  observation_.Reset();
}

std::unique_ptr<net::EmbeddedTestServer> CreateAndStartDevServer(
    base::FilePath::StringViewType chrome_test_data_relative_root) {
  base::FilePath server_root =
      base::FilePath(FILE_PATH_LITERAL("chrome/test/data"))
          .Append(chrome_test_data_relative_root);
  auto server = std::make_unique<net::EmbeddedTestServer>();
  server->AddDefaultHandlers(server_root);
  CHECK(server->Start());
  return server;
}

IsolatedWebAppUrlInfo InstallDevModeProxyIsolatedWebApp(
    Profile* profile,
    const url::Origin& proxy_origin) {
  base::test::TestFuture<base::expected<InstallIsolatedWebAppCommandSuccess,
                                        InstallIsolatedWebAppCommandError>>
      future;

  auto url_info = IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(
      web_package::SignedWebBundleId::CreateRandomForProxyMode());
  WebAppProvider::GetForWebApps(profile)->scheduler().InstallIsolatedWebApp(
      url_info,
      IsolatedWebAppInstallSource::FromDevUi(IwaSourceProxy(proxy_origin)),
      /*expected_version=*/std::nullopt,
      /*optional_keep_alive=*/nullptr,
      /*optional_profile_keep_alive=*/nullptr, future.GetCallback());

  CHECK(future.Get().has_value()) << future.Get().error();

  return url_info;
}

content::RenderFrameHost* OpenIsolatedWebApp(
    Profile* profile,
    const webapps::AppId& app_id,
    std::optional<std::string_view> path) {
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto url = [&]() -> std::optional<GURL> {
    if (!path) {
      return std::nullopt;
    }
    return provider->registrar_unsafe().GetAppStartUrl(app_id).Resolve(*path);
  }();

  base::test::TestFuture<content::WebContents*> future;
  provider->scheduler().LaunchApp(
      app_id, url,
      base::BindOnce([](base::WeakPtr<Browser>,
                        base::WeakPtr<content::WebContents> web_contents,
                        apps::LaunchContainer) {
        return web_contents.get();
      }).Then(future.GetCallback()));

  auto* web_contents = future.Get();
  content::WaitForLoadStop(web_contents);
  return web_contents->GetPrimaryMainFrame();
}

void CreateIframe(content::RenderFrameHost* parent_frame,
                  const std::string& iframe_id,
                  const GURL& url,
                  const std::string& permissions_policy) {
  EXPECT_EQ(true, content::EvalJs(
                      parent_frame,
                      content::JsReplace(R"(
            new Promise(resolve => {
              let f = document.createElement('iframe');
              f.id = $1;
              f.src = $2;
              f.allow = $3;
              f.addEventListener('load', () => resolve(true));
              document.body.appendChild(f);
            });
        )",
                                         iframe_id, url, permissions_policy),
                      content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

void SimulateIsolatedWebAppNavigation(content::WebContents* web_contents,
                                      const GURL& url) {
  ASSERT_TRUE(base::FeatureList::IsEnabled(features::kIsolatedWebApps))
      << "Navigation to an IWA in a test that doesn't have IWAs enabled!";
  auto navigation =
      content::NavigationSimulator::CreateBrowserInitiated(url, web_contents);
  navigation->SetTransition(ui::PAGE_TRANSITION_TYPED);

  auto* permissions_policy_cache =
      web_app::IwaPermissionsPolicyCacheFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  auto iwa_origin = IwaOrigin::Create(url).value();
  PopulatePermissionsPolicyCache(permissions_policy_cache, iwa_origin);

  CommitNavigation(std::move(navigation),
                   permissions_policy_cache->GetPolicy(iwa_origin),
                   iwa_origin.origin());
}

void CommitPendingIsolatedWebAppNavigation(content::WebContents* web_contents) {
  content::NavigationController& controller = web_contents->GetController();
  const auto* pending_entry = controller.GetPendingEntry();
  if (!pending_entry) {
    return;
  }

  auto* permissions_policy_cache =
      web_app::IwaPermissionsPolicyCacheFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents->GetBrowserContext()));
  auto iwa_origin = IwaOrigin::Create(pending_entry->GetURL()).value();
  PopulatePermissionsPolicyCache(permissions_policy_cache, iwa_origin);

  CommitNavigation(content::NavigationSimulator::CreateFromPending(controller),
                   permissions_policy_cache->GetPolicy(iwa_origin),
                   iwa_origin.origin());
}

}  // namespace web_app
