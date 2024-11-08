// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/web_install_from_url_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace web_app {

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
  if (!options) {
    install_target = render_frame_host().GetLastCommittedURL();
  } else {
    install_target = options->install_url;
  }

  // Do not allow installation of file:// or chrome:// urls.
  if (!install_target.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  if (!options) {
    // TODO(crbug.com/333795265): Queue a web app command against the current
    // document. For the time being, stub out the callback to prevent the calls
    // from hanging.
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kSuccess,
                            GURL());
  }

  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  auto* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());
  if (!profile) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  auto* provider = WebAppProvider::GetForWebApps(profile);
  provider->scheduler().InstallAppFromUrl(
      options->manifest_id, install_target,
      base::BindOnce(&WebInstallServiceImpl::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void WebInstallServiceImpl::OnAppInstalled(InstallCallback callback,
                                           const GURL& manifest_id,
                                           webapps::InstallResultCode code) {
  blink::mojom::WebInstallServiceResult result =
      blink::mojom::WebInstallServiceResult::kAbortError;

  if (webapps::IsSuccess(code)) {
    result = blink::mojom::WebInstallServiceResult::kSuccess;
  }

  std::move(callback).Run(result, manifest_id);
}

}  // namespace web_app
