// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_service_impl.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using blink::mojom::SubAppsServiceListResult;
using blink::mojom::SubAppsServiceResult;

namespace web_app {

namespace {

WebAppProvider* GetWebAppProvider(content::RenderFrameHost* render_frame_host) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  return web_app::WebAppProvider::GetForWebContents(initiator_web_contents);
}

absl::optional<AppId> GetAppId(content::RenderFrameHost* render_frame_host) {
  GURL parent_url = render_frame_host->GetLastCommittedURL();
  WebAppRegistrar& web_app_registrar =
      GetWebAppProvider(render_frame_host)->registrar();
  return web_app_registrar.FindAppWithUrlInScope(parent_url);
}

// Resolve the install_path in the context of the calling app to get the full
// URL. This looks a bit convoluted to guarantee that the resulting URL
// *always* has the same *origin* as the calling app (normally the renderer
// should only send the path, but a compromised renderer might send a full URL
// instead and we guard against that here).
GURL ResolvePathWithOrigin(const std::string& path, const GURL& origin) {
  return origin.Resolve(origin.Resolve(path).PathForRequest());
}

void OnAdd(SubAppsServiceImpl::AddCallback result_callback,
           const AppId& app_id,
           webapps::InstallResultCode code) {
  if (code == webapps::InstallResultCode::kSuccessAlreadyInstalled ||
      code == webapps::InstallResultCode::kSuccessNewInstall) {
    std::move(result_callback).Run(SubAppsServiceResult::kSuccess);
  } else {
    std::move(result_callback).Run(SubAppsServiceResult::kFailure);
  }
}

}  // namespace

SubAppsServiceImpl::SubAppsServiceImpl(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsService> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsServiceImpl::~SubAppsServiceImpl() = default;

// static
void SubAppsServiceImpl::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsService> receiver) {
  // This class is created only on the primary main frame (this excludes
  // fenced frames and prerendered pages).
  DCHECK(render_frame_host->IsInPrimaryMainFrame());

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new SubAppsServiceImpl(render_frame_host, std::move(receiver));
}

void SubAppsServiceImpl::Add(const std::string& install_path,
                             AddCallback result_callback) {
  // Verify that the calling app is installed itself. This check is done here
  // and not in |CreateIfAllowed| because of a potential race between doing the
  // check there and then running the current function, and the parent app being
  // installed/uninstalled.
  absl::optional<AppId> parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id.has_value()) {
    return std::move(result_callback).Run(SubAppsServiceResult::kFailure);
  }

  // Resolve the install_path in the origin context of the calling app.
  GURL install_url = ResolvePathWithOrigin(install_path, origin().GetURL());

  GetWebAppProvider(render_frame_host())
      ->install_manager()
      .InstallSubApp(*parent_app_id, install_url,
                     base::BindOnce(&OnAdd, std::move(result_callback)));
}

void SubAppsServiceImpl::List(ListCallback result_callback) {
  // Verify that the calling app is installed itself (cf. |Add|).
  absl::optional<AppId> parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id.has_value()) {
    return std::move(result_callback)
        .Run(SubAppsServiceListResult::New(SubAppsServiceResult::kFailure,
                                           std::vector<std::string>()));
  }

  WebAppRegistrar& registrar =
      GetWebAppProvider(render_frame_host())->registrar();

  std::vector<std::string> sub_app_ids;
  for (const AppId& web_app_id : registrar.GetAllSubAppIds(*parent_app_id)) {
    const WebApp* web_app = registrar.GetAppById(web_app_id);
    sub_app_ids.push_back(
        GenerateAppIdUnhashed(web_app->manifest_id(), web_app->start_url()));
  }

  std::move(result_callback)
      .Run(SubAppsServiceListResult::New(SubAppsServiceResult::kSuccess,
                                         std::move(sub_app_ids)));
}

}  // namespace web_app
