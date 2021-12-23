// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_renderer_host.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/check.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using blink::mojom::SubAppsProviderResult;

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
GURL ResolvePathWithOrigin(const std::string& path, GURL origin) {
  return origin.Resolve(origin.Resolve(path).PathForRequest());
}

void OnAdd(SubAppsRendererHost::AddCallback result_callback,
           const AppId& app_id,
           InstallResultCode code) {
  if (code == InstallResultCode::kSuccessAlreadyInstalled ||
      code == InstallResultCode::kSuccessNewInstall) {
    std::move(result_callback).Run(SubAppsProviderResult::kSuccess);
  } else {
    std::move(result_callback).Run(SubAppsProviderResult::kFailure);
  }
}

}  // namespace

SubAppsRendererHost::SubAppsRendererHost(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsProvider> receiver)
    : DocumentService(render_frame_host, std::move(receiver)) {}

SubAppsRendererHost::~SubAppsRendererHost() = default;

// static
void SubAppsRendererHost::CreateIfAllowed(
    content::RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::SubAppsProvider> receiver) {
  // This class is created only on the main frame.
  DCHECK(!render_frame_host->GetParent());

  // Bail if Web Apps aren't enabled on current profile.
  if (!AreWebAppsEnabled(Profile::FromBrowserContext(
          content::WebContents::FromRenderFrameHost(render_frame_host)
              ->GetBrowserContext()))) {
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new SubAppsRendererHost(render_frame_host, std::move(receiver));
}

void SubAppsRendererHost::Add(const std::string& install_path,
                              AddCallback result_callback) {
  // Verify that the calling app is installed itself. This check is done here
  // and not in |CreateIfAllowed| because of a potential race between doing the
  // check there and then running the current function, and the parent app being
  // installed/uninstalled.
  absl::optional<AppId> parent_app_id = GetAppId(render_frame_host());
  if (!parent_app_id.has_value()) {
    return std::move(result_callback).Run(SubAppsProviderResult::kFailure);
  }

  // Resolve the install_path in the origin context of the calling app.
  GURL install_url = ResolvePathWithOrigin(install_path, origin().GetURL());

  GetWebAppProvider(render_frame_host())
      ->install_manager()
      .InstallSubApp(*parent_app_id, install_url,
                     base::BindOnce(&OnAdd, std::move(result_callback)));
}

}  // namespace web_app
