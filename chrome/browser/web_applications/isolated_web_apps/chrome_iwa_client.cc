// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/isolated_web_apps/chrome_iwa_client.h"

#include <variant>

#include "base/strings/stringprintf.h"
#include "base/types/expected_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/non_installed_bundle_inspection_context.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_features.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_trust_checker.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolated_web_apps/runtime_data/chrome_iwa_runtime_data_provider.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/url_constants.h"
#include "components/web_package/signed_web_bundles/signed_web_bundle_id.h"
#include "components/webapps/isolated_web_apps/client.h"
#include "components/webapps/isolated_web_apps/types/url_loading_types.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/isolated_web_apps_policy.h"
#include "content/public/browser/web_contents.h"
#include "net/base/net_errors.h"

namespace web_app {

namespace {

using SourceRequestError = IwaClient::SourceRequestError;

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

base::expected<IwaSourceWithModeOrGeneratedResponse, SourceRequestError>
GetIwaSourceWithTrustCheck(base::WeakPtr<Profile> profile,
                           const web_package::SignedWebBundleId& web_bundle_id,
                           const webapps::AppId& iwa_id) {
  if (!profile) {
    return base::unexpected(
        SourceRequestError{.net_error = net::ERR_CONTEXT_SHUT_DOWN,
                           .error_description = "Profile has shut down."});
  }
  WebAppRegistrar& registrar =
      WebAppProvider::GetForWebApps(profile.get())->registrar_unsafe();
  const WebApp* iwa =
      registrar.GetAppById(iwa_id, WebAppFilter::IsIsolatedApp());
  if (!iwa) {
    return base::unexpected(SourceRequestError{
        .net_error = net::ERR_FAILED,
        .error_description = base::StringPrintf(
            "There's no matching Isolated Web App installed.")});
  }
  RETURN_IF_ERROR(IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
                      *profile, web_bundle_id, *iwa)
                      .transform_error([](const std::string& error) {
                        return SourceRequestError{
                            .net_error = net::ERR_INVALID_WEB_BUNDLE,
                            .error_description = error};
                      }));
  return IwaSourceWithMode::FromStorageLocation(
      profile->GetPath(), iwa->isolation_data()->location());
}

void GetIwaSourceForRequestImpl(
    base::WeakPtr<Profile> profile,
    const web_package::SignedWebBundleId& web_bundle_id,
    const network::ResourceRequest& request,
    const std::optional<content::FrameTreeNodeId>& frame_tree_node,
    base::OnceCallback<void(base::expected<IwaSourceWithModeOrGeneratedResponse,
                                           SourceRequestError>)> callback) {
  if (!profile) {
    std::move(callback).Run(base::unexpected(
        SourceRequestError{.net_error = net::ERR_CONTEXT_SHUT_DOWN,
                           .error_description = "Profile has shut down."}));
    return;
  }

  if (frame_tree_node) {
    auto* web_contents =
        content::WebContents::FromFrameTreeNodeId(*frame_tree_node);
    if (!web_contents) {
      // `web_contents` can be `nullptr` in certain edge cases, such as when
      // the browser window closes concurrently with an ongoing request (see
      // crbug.com/1477761). Return an error if that is the case, instead of
      // silently not querying `NonInstalledBundleInspectionContext`. Should we
      // ever find a case where we _do_ want to continue request processing
      // even though the `WebContents` no longer exists, we can change the
      // below code to skip checking `NonInstalledBundleInspectionContext`
      // instead of returning an error.
      std::move(callback).Run(base::unexpected(SourceRequestError{
          .net_error = net::ERR_CONTEXT_SHUT_DOWN,
          .error_description =
              "Unable to find WebContents based on frame tree node id."}));
      return;
    }
    if (const auto* inspection_context =
            NonInstalledBundleInspectionContext::FromWebContents(
                web_contents)) {
      RETURN_IF_ERROR(
          IsolatedWebAppTrustChecker::IsResourceLoadingAllowed(
              *profile, web_bundle_id, *inspection_context),
          [&](const std::string& error) {
            std::move(callback).Run(base::unexpected(
                SourceRequestError{.net_error = net::ERR_INVALID_WEB_BUNDLE,
                                   .error_description = error}));
          });
      if (request.url.GetPath() == kInstallPagePath) {
        std::move(callback).Run(
            GeneratedResponse{.response_body = kInstallPageContent});
      } else {
        std::move(callback).Run(inspection_context->source());
      }
      return;
    }
  }

  webapps::AppId iwa_id =
      IsolatedWebAppUrlInfo::CreateFromSignedWebBundleId(web_bundle_id)
          .app_id();
  auto* provider = WebAppProvider::GetForWebApps(profile.get());

  if (provider->isolated_web_app_update_manager().IsUpdateBeingApplied(
          iwa_id)) {
    // TODO(crbug.com/432676258): How likely is this case?
    provider->isolated_web_app_update_manager().PrioritizeUpdateAndWait(
        iwa_id,
        // We ignore whether or not the update was applied successfully - if
        // it succeeds, we send the request to the updated version. If it
        // fails, we send the request to the previous version and rely on the
        // update system to retry the update at a later point.
        base::IgnoreArgs<IsolatedWebAppApplyUpdateCommandResult>(
            base::BindOnce(&GetIwaSourceWithTrustCheck, profile, web_bundle_id,
                           iwa_id))
            .Then(std::move(callback)));
    return;
  }

  return std::move(callback).Run(
      GetIwaSourceWithTrustCheck(profile, web_bundle_id, iwa_id));
}

}  // namespace

void ChromeIwaClient::CreateSingleton() {
  static base::NoDestructor<ChromeIwaClient> instance;
  instance.get();
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
                                           SourceRequestError>)> callback) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  WebAppProvider::GetForWebApps(profile)->on_registry_ready().Post(
      FROM_HERE, base::BindOnce(&GetIwaSourceForRequestImpl,
                                profile->GetWeakPtr(), web_bundle_id, request,
                                frame_tree_node, std::move(callback)));
}

IwaRuntimeDataProvider* ChromeIwaClient::GetRuntimeDataProvider() {
  return &ChromeIwaRuntimeDataProvider::GetInstance();
}

}  // namespace web_app
