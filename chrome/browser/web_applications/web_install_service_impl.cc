// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/permissions/permission_request.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_request_description.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/permissions/permission_utils.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace web_app {

namespace {

using PermissionStatus = blink::mojom::PermissionStatus;

}  // namespace

WebInstallServiceImpl::WebInstallServiceImpl(
    content::RenderFrameHost& render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver)
    : content::DocumentService<blink::mojom::WebInstallService>(
          render_frame_host,
          std::move(receiver)),
      frame_routing_id_(render_frame_host.GetGlobalId()) {}

WebInstallServiceImpl::~WebInstallServiceImpl() = default;

// static
void WebInstallServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::WebInstallService> receiver) {
  CHECK(render_frame_host);

  // This class is created only on the primary main frame.
  if (!render_frame_host->IsInPrimaryMainFrame()) {
    receiver.reset();
    return;
  }

  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    receiver.reset();
    return;
  }

  if (!render_frame_host->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    receiver.reset();
    return;
  }

  new WebInstallServiceImpl(*render_frame_host, std::move(receiver));
}

void WebInstallServiceImpl::Install(blink::mojom::InstallOptionsPtr options,
                                    InstallCallback callback) {
  GURL install_target;
  const GURL current_url = render_frame_host().GetLastCommittedURL();

  // `options` is null if the 0-parameter signature was called.
  if (!options) {
    // No parameters means we want to install the current document.
    install_target = current_url;
  } else {
    install_target = options->install_url;
  }

  // Do not allow installation of file:// or chrome:// urls.
  if (!install_target.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // Initiate installation of the current document.
  if (install_target == current_url) {
    // TODO(crbug.com/381214488): Implement 0-param install. Queue a web app
    // command against the current document. For the time being, stub out the
    // callback to prevent the calls from hanging for zero parameters given.
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kSuccess,
                            GURL());

    // Current document installs don't require the permissions checking code.
    return;
  }

  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // Verify that the calling app has the Web Install permissions policy set.
  if (!rfh->IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kWebAppInstallation)) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  RequestWebInstallPermission(
      base::BindOnce(&WebInstallServiceImpl::OnPermissionDecided,
                     weak_ptr_factory_.GetWeakPtr(), install_target,
                     options->manifest_id, std::move(callback)));
}

void WebInstallServiceImpl::RequestWebInstallPermission(
    base::OnceCallback<void(const std::vector<PermissionStatus>&)> callback) {
  content::BrowserContext* const browser_context =
      render_frame_host().GetBrowserContext();
  if (!browser_context) {
    // TODO(crbug.com/381282538): Technically this isn't correct since
    // permission wasn't denied. Same for the if check below. Update to a more
    // appropriate error.
    std::move(callback).Run(
        std::vector<PermissionStatus>({PermissionStatus::DENIED}));
    return;
  }

  content::PermissionController* const permission_controller =
      browser_context->GetPermissionController();
  if (!permission_controller) {
    std::move(callback).Run(
        std::vector<PermissionStatus>({PermissionStatus::DENIED}));
    return;
  }

  // Check if the permission status is already set.
  std::vector<blink::PermissionType> permission_requests;
  content::PermissionResult permission_status =
      permission_controller->GetPermissionResultForCurrentDocument(
          blink::PermissionType::WEB_APP_INSTALLATION, &render_frame_host());
  switch (permission_status.status) {
    case PermissionStatus::GRANTED:
      std::move(callback).Run(
          std::vector<PermissionStatus>({PermissionStatus::GRANTED}));
      return;
    case PermissionStatus::DENIED:
      std::move(callback).Run(
          std::vector<PermissionStatus>({PermissionStatus::DENIED}));
      return;
    case PermissionStatus::ASK:
      break;
  }
  permission_requests.push_back(blink::PermissionType::WEB_APP_INSTALLATION);

  GURL requesting_origin =
      render_frame_host().GetLastCommittedOrigin().GetURL();
  // User gesture requirement is enforced in NavigatorWebInstall::InstallImpl.
  permission_controller->RequestPermissionsFromCurrentDocument(
      &render_frame_host(),
      content::PermissionRequestDescription(
          permission_requests, /*user_gesture=*/true, requesting_origin),
      std::move(callback));
}

void WebInstallServiceImpl::OnPermissionDecided(
    const GURL& install_target,
    const std::optional<GURL>& manifest_id,
    InstallCallback callback,
    const std::vector<PermissionStatus>& permission_status) {
  // TODO(crbug.com/381282538): Throw different error if permission not granted.
  CHECK_EQ(permission_status.size(), 1u);
  if (permission_status[0] != PermissionStatus::GRANTED) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }
  auto* profile =
      Profile::FromBrowserContext(render_frame_host().GetBrowserContext());

  auto* provider = WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallAppFromUrl(
      install_target, manifest_id,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebInstallServiceImpl::OnAppInstalled(InstallCallback callback,
                                           const GURL& manifest_id,
                                           webapps::InstallResultCode code) {
  // TODO(crbug.com/381282538): Add error types for additional granularity.
  blink::mojom::WebInstallServiceResult result =
      blink::mojom::WebInstallServiceResult::kAbortError;

  if (webapps::IsSuccess(code)) {
    result = blink::mojom::WebInstallServiceResult::kSuccess;
  }

  std::move(callback).Run(result, manifest_id);
}

}  // namespace web_app
