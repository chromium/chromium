// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_

#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace web_app {

class WebInstallServiceImpl
    : public content::DocumentService<blink::mojom::WebInstallService> {
 public:
  WebInstallServiceImpl(const WebInstallServiceImpl&) = delete;
  WebInstallServiceImpl& operator=(const WebInstallServiceImpl&) = delete;

  static void CreateIfAllowed(
      content::RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);

  // blink::mojom::WebInstallService implementation:
  void InstallCurrentDocument(const GURL& manifest_id,
                              InstallCurrentDocumentCallback callback) override;

  void InstallBackgroundDocument(
      const GURL& manifest_id,
      const GURL& install_url,
      InstallBackgroundDocumentCallback callback) override;

 private:
  WebInstallServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);
  ~WebInstallServiceImpl() override;

  const content::GlobalRenderFrameHostId frame_routing_id_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
