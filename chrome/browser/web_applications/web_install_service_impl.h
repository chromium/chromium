// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_

#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/document_service.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom-forward.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"
#include "third_party/blink/public/mojom/web_install/web_install.mojom.h"
#include "url/gurl.h"

namespace webapps {
enum class InstallResultCode;
enum class InstallableStatusCode;
}
namespace web_app {
class WebAppProvider;

// Service side implementation for the Blink Web Install API. Takes the
// parameters from the API call in the form of `InstallOptionsPtr`, and decides
// whether to install the current document or a background document.
// Background document installs will prompt for approval/denial of the Web app
// installation permission for the calling origin.
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

  void TryInstallCurrentDocument(InstallCallback callback);

  void OnDidRetrieveManifestForCurrentDocumentInstall(
      InstallCallback callback,
      WebAppProvider* provider,
      blink::mojom::ManifestPtr opt_manifest,
      bool valid_manifest_for_web_app,
      webapps::InstallableStatusCode error_code);

  void RequestWebInstallPermission(
      base::OnceCallback<
          void(const std::vector<blink::mojom::PermissionStatus>&)> callback);

  void OnPermissionDecided(
      const GURL& install_target,
      const std::optional<GURL>& manifest_id,
      InstallCallback callback,
      const std::vector<blink::mojom::PermissionStatus>& permission_status);

  // Used by web app install dialog code as the WebAppInstalledCallback.
  // Reports install success or failure back to Blink via `callback`.
  void OnAppInstalled(InstallCallback callback,
                      const webapps::AppId& app_id,
                      webapps::InstallResultCode code);

  blink::mojom::InstallOptionsPtr install_options_;
  const content::GlobalRenderFrameHostId frame_routing_id_;
  base::WeakPtrFactory<web_app::WebInstallServiceImpl> weak_ptr_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_WEB_INSTALL_SERVICE_IMPL_H_
