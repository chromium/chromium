// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager_observer.h"
#include "components/webapps/isolated_web_apps/types/update_channel.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "url/gurl.h"

class Profile;

namespace content {
class WebUI;
class WebContents;
}  // namespace content

namespace web_app {
class WebAppInstallManager;
class WebAppProvider;
}  // namespace web_app

// Handles API requests from chrome://iwa-dev page by implementing
// iwa_dev::mojom::PageHandler.
class IwaDevPageHandler : public iwa_dev::mojom::PageHandler,
                          public web_app::WebAppInstallManagerObserver {
 public:
  IwaDevPageHandler(
      content::WebUI* web_ui,
      mojo::PendingRemote<iwa_dev::mojom::Page> page,
      mojo::PendingReceiver<iwa_dev::mojom::PageHandler> receiver);

  IwaDevPageHandler(const IwaDevPageHandler&) = delete;
  IwaDevPageHandler& operator=(const IwaDevPageHandler&) = delete;

  ~IwaDevPageHandler() override;

  // iwa_dev::mojom::PageHandler:
  void GetInstalledAppsInfo(GetInstalledAppsInfoCallback callback) override;
  void InstallAppFromDevProxy(const GURL& url,
                              InstallAppFromDevProxyCallback callback) override;
  void InstallAppFromUpdateManifest(
      const GURL& web_bundle_url,
      iwa_dev::mojom::UpdateInfoPtr update_info,
      InstallAppFromUpdateManifestCallback callback) override;
  void ParseUpdateManifestFromUrl(
      const GURL& update_manifest_url,
      ParseUpdateManifestFromUrlCallback callback) override;
  void SelectAndInstallAppFromLocalWebBundle(
      SelectAndInstallAppFromLocalWebBundleCallback callback) override;
  void UninstallApp(const std::string& app_id,
                    UninstallAppCallback callback) override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

 private:
  class LocalBundleSelectListener;

  void OnLocalBundleSelected(
      SelectAndInstallAppFromLocalWebBundleCallback callback,
      std::optional<base::FilePath> path);

  const raw_ref<Profile> profile_;
  const raw_ref<web_app::WebAppProvider> provider_;
  const raw_ref<content::WebContents> web_contents_;
  mojo::Receiver<iwa_dev::mojom::PageHandler> receiver_;
  mojo::Remote<iwa_dev::mojom::Page> page_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_observation_{this};
  base::WeakPtrFactory<IwaDevPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
