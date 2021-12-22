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
#include "chrome/browser/web_applications/web_app_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "url/origin.h"

using blink::mojom::SubAppsProviderResult;

namespace web_app {

namespace {

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

  // Bail if the calling app isn't installed itself.
  absl::optional<AppId> parent_app_id =
      web_app::WebAppProvider::GetForWebContents(
          content::WebContents::FromRenderFrameHost(render_frame_host))
          ->registrar()
          .FindAppWithUrlInScope(render_frame_host->GetLastCommittedURL());
  if (!parent_app_id.has_value()) {
    return;
  }

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See DocumentService for details.
  new SubAppsRendererHost(render_frame_host, std::move(receiver));
}

void SubAppsRendererHost::Add(const std::string& install_path,
                              AddCallback result_callback) {
  auto* const initiator_web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host());

  auto* const web_app_provider =
      web_app::WebAppProvider::GetForWebContents(initiator_web_contents);
  GURL parent_url = render_frame_host()->GetLastCommittedURL();
  auto& web_app_registrar = web_app_provider->registrar();
  absl::optional<AppId> parent_app_id =
      web_app_registrar.FindAppWithUrlInScope(parent_url);

  // Verify that the calling app is installed itself. This is needed because the
  // parent app may have been uninstalled by the time we receive this Mojo
  // message.
  if (!parent_app_id.has_value()) {
    return std::move(result_callback).Run(SubAppsProviderResult::kFailure);
  }

  auto& install_manager =
      web_app::WebAppProvider::GetForWebContents(initiator_web_contents)
          ->install_manager();
  // Tack the install_path onto the origin of the calling app to get the full
  // URL.
  GURL::Replacements replacements;
  replacements.SetPathStr(install_path);
  GURL gurl = origin().GetURL().ReplaceComponents(replacements);

  install_manager.InstallSubApp(
      *parent_app_id, gurl, base::BindOnce(&OnAdd, std::move(result_callback)));
}

}  // namespace web_app
