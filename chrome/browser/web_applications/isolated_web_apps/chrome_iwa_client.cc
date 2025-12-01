// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"

#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_install_command_helper.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/pending_install_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/iwa_key_distribution_info_provider.h"
#include "components/webapps/isolated_web_apps/types/url_loading_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_contents.h"

namespace web_app {

namespace {

constexpr char kInstallPagePath[] = "/.well-known/_generated_install_page.html";
constexpr char kInstallPageContent[] = R"(
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8" />
        <meta http-equiv="Content-Security-Policy" content="default-src 'self'">
        <link rel="manifest" href="/.well-known/manifest.webmanifest" />
      </head>
    </html>
)";

base::expected<IwaSourceWithModeOrGeneratedResponse, std::string> GetIwaSource(
    base::WeakPtr<Profile> profile,
    const webapps::AppId& iwa_id) {
  if (!profile) {
    return base::unexpected("Profile has shut down.");
  }
  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile.get())->registrar_unsafe();
  const WebApp* iwa = registrar.GetAppById(iwa_id);
  if (!iwa || !iwa->isolation_data()) {
    return base::unexpected(
        base::StringPrintf("There's no matching Isolated Web App installed."));
  }
  if (iwa->isolation_data()->location().dev_mode() &&
      !IsIwaDevModeEnabled(profile.get())) {
    return base::unexpected(
        "Isolated Web App Developer Mode is not enabled or blocked by policy.");
  }

  return IwaSourceWithMode::FromStorageLocation(
      profile->GetPath(), iwa->isolation_data()->location());
}

void GetIwaSourceForRequestImpl(
    base::WeakPtr<Profile> profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& request,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node,
    base::OnceCallback<void(base::expected<IwaSourceWithModeOrGeneratedResponse,
                                           std::string>)> callback) {
  if (!profile) {
    std::move(callback).Run(base::unexpected("Profile has shut down."));
    return;
  }

  if (frame_tree_node) {
    auto* web_contents =
        content::WebContents::FromFrameTreeNodeId(*frame_tree_node);
    if (web_contents == nullptr) {
      // `web_contents` can be `nullptr` in certain edge cases, such as when
      // the browser window closes concurrently with an ongoing request (see
      // crbug.com/1477761). Return an error if that is the case, instead of
      // silently not querying `IsolatedWebAppPendingInstallInfo`. Should we
      // ever find a case where we _do_ want to continue request processing
      // even though the `WebContents` no longer exists, we can change the
      // below code to skip checking `IsolatedWebAppPendingInstallInfo`
      // instead of returning an error.
      std::move(callback).Run(base::unexpected(
          "Unable to find WebContents based on frame tree node id."));
      return;
    }
    if (const auto& source =
            IsolatedWebAppPendingInstallInfo::FromWebContents(*web_contents)
                .source()) {
      if (request.url.GetPath() == kInstallPagePath) {
        std::move(callback).Run(
            GeneratedResponse{.response_body = kInstallPageContent});
      } else {
        std::move(callback).Run(*source);
      }
      return;
    }
  }

  webapps::AppId iwa_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
          .app_id();
  auto* provider = WebAppProvider::GetForWebApps(profile.get());
  if (provider->iwa_update_manager().IsUpdateBeingApplied(iwa_id)) {
    // TODO(crbug.com/432676258): How likely is this case?
    provider->iwa_update_manager().PrioritizeUpdateAndWait(
        iwa_id,
        // We ignore whether or not the update was applied successfully - if it
        // succeeds, we send the request to the updated version. If it fails, we
        // send the request to the previous version and rely on the update
        // system to retry the update at a later point.
        base::IgnoreArgs<IsolatedWebAppApplyUpdateCommandResult>(
            base::BindOnce(&GetIwaSource, profile, iwa_id)
                .Then(std::move(callback))));
    return;
  }

  return std::move(callback).Run(GetIwaSource(profile, iwa_id));
}

}  // namespace

void ChromeIwaClient::CreateSingleton() {
  static base::NoDestructor<ChromeIwaClient> instance;
  instance.get();
}

base::expected<void, std::string> ChromeIwaClient::ValidateTrust(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    bool dev_mode) {
  return IsolatedWebAppTrustChecker::IsTrusted(
      *Profile::FromBrowserContext(browser_context), web_bundle_id, dev_mode);
}

void ChromeIwaClient::RunWhenAppCloses(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    base::OnceClosure callback) {
  WebAppProvider::GetForWebApps(Profile::FromBrowserContext(browser_context))
      ->ui_manager()
      .NotifyOnAllAppWindowsClosed(
          IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
              .app_id(),
          std::move(callback));
}

void ChromeIwaClient::GetIwaSourceForRequest(
    content::BrowserContext* browser_context,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& request,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node,
    base::OnceCallback<void(base::expected<IwaSourceWithModeOrGeneratedResponse,
                                           std::string>)> callback) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebAppProvider::GetForWebApps(profile)->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&GetIwaSourceForRequestImpl,
                                profile->GetWeakPtr(), web_bundle_id, request,
                                frame_tree_node, std::move(callback)));
}

IwaRuntimeDataProvider* ChromeIwaClient::GetRuntimeDataProvider() {
  return &IwaKeyDistributionInfoProvider::GetInstance();
}

}  // namespace web_app
