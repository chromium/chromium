// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/web_install_service_impl.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_utils.h"
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

void WebInstallServiceImpl::InstallCurrentDocument(
    const GURL& manifest_id,
    InstallCurrentDocumentCallback callback) {
  auto* rfh = content::RenderFrameHost::FromID(frame_routing_id_);
  if (!rfh) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  GURL current_url = rfh->GetLastCommittedURL();

  // Do not allow installation of file:// or chrome:// urls.
  if (!current_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // TODO(333795265): Queue a WebInstallCommand to prompt for installation.
  std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                          GURL());
}

void WebInstallServiceImpl::InstallBackgroundDocument(
    const GURL& manifest_id,
    const GURL& install_url,
    InstallBackgroundDocumentCallback callback) {
  // Do not allow installation of file:// or chrome:// urls.
  if (!install_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                            GURL());
    return;
  }

  // TODO(333795265): Queue a WebInstallCommand to prompt for installation.
  std::move(callback).Run(blink::mojom::WebInstallServiceResult::kAbortError,
                          GURL());
  return;
}

}  // namespace web_app
