// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace webapps {
enum class InstallResultCode;
}
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
  void Install(blink::mojom::InstallOptionsPtr options,
               InstallCallback callback) override;

 private:
  WebInstallServiceImpl(
      content::RenderFrameHost& render_frame_host,
      mojo::PendingReceiver<blink::mojom::WebInstallService> receiver);
  ~WebInstallServiceImpl() override;

  void OnAppInstalled(InstallCallback callback,
                      const GURL& manifest_id,
                      webapps::InstallResultCode code);

  const content::GlobalRenderFrameHostId frame_routing_id_;

  base::WeakPtrFactory<WebInstallServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
