// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/optional_ref.h"
#include "chrome/browser/ui/webui/iwa_dev/iwa_dev.mojom.h"
#include "chrome/browser/web_applications/isolated_web_apps/commands/isolated_web_app_apply_update_command.h"
#include "chrome/browser/web_applications/isolated_web_apps/install/isolated_web_app_dev_install_manager.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_check_and_prepare_task.h"
#include "chrome/browser/web_applications/isolated_web_apps/update/isolated_web_app_update_manager.h"
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
class IwaDevPageHandler
    : public iwa_dev::mojom::PageHandler,
      public web_app::WebAppInstallManagerObserver,
      public web_app::IsolatedWebAppUpdateManager::Observer {
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
  void SelectAndUpdateAppFromLocalWebBundle(
      const std::string& app_id,
      SelectAndUpdateAppFromLocalWebBundleCallback callback) override;
  void UninstallApp(const std::string& app_id,
                    UninstallAppCallback callback) override;
  void UpdateDevProxyInstalledApp(
      const std::string& app_id,
      UpdateDevProxyInstalledAppCallback callback) override;
  void UpdateManifestInstalledApp(
      const std::string& app_id,
      UpdateManifestInstalledAppCallback callback) override;

  // web_app::WebAppInstallManagerObserver:
  void OnWebAppInstalled(const webapps::AppId& app_id) override;
  void OnWebAppManifestUpdated(const webapps::AppId& app_id) override;
  void OnWebAppWillBeUninstalled(const webapps::AppId& app_id) override;
  void OnWebAppInstallManagerDestroyed() override;

  // web_app::IsolatedWebAppUpdateManager::Observer:
  void OnUpdateDiscoverAndPrepareTaskCompleted(
      const webapps::AppId& app_id,
      web_app::IwaUpdateCheckAndPrepareResult status) override;
  void OnUpdateApplyTaskCompleted(
      const webapps::AppId& app_id,
      web_app::IsolatedWebAppApplyUpdateCommandResult status) override;

 private:
  class LocalBundleSelectListener;

  using SelectLocalBundleResult =
      base::expected<base::FilePath, mojo_base::mojom::ErrorPtr>;

  void SelectLocalWebBundle(
      base::OnceCallback<void(SelectLocalBundleResult)> callback);

  void OnLocalBundleSelectedForInstall(
      SelectAndInstallAppFromLocalWebBundleCallback callback,
      SelectLocalBundleResult path);
  void OnLocalBundleSelectedForUpdate(
      const std::string& app_id,
      SelectAndUpdateAppFromLocalWebBundleCallback callback,
      SelectLocalBundleResult path);

  void ApplyDevModeUpdate(
      const std::string& app_id,
      base::optional_ref<const web_app::IwaSourceDevModeWithFileOp> location,
      base::OnceCallback<
          void(base::expected<std::monostate, mojo_base::mojom::ErrorPtr>)>
          callback);

  std::optional<UpdateManifestInstalledAppCallback> TakeManifestUpdateRequest(
      const webapps::AppId& app_id);

  base::expected<const web_app::WebApp*, mojo_base::mojom::ErrorPtr>
  GetInstalledAppById(const std::string& app_id);

  const raw_ref<Profile> profile_;
  const raw_ref<web_app::WebAppProvider> provider_;
  const raw_ref<content::WebContents> web_contents_;
  mojo::Receiver<iwa_dev::mojom::PageHandler> receiver_;
  mojo::Remote<iwa_dev::mojom::Page> page_;
  base::ScopedObservation<web_app::WebAppInstallManager,
                          web_app::WebAppInstallManagerObserver>
      install_observation_{this};
  base::ScopedObservation<web_app::IsolatedWebAppUpdateManager,
                          web_app::IsolatedWebAppUpdateManager::Observer>
      update_observation_{this};

  // Maps in-flight update requests for manifest-installed apps by AppId to
  // their Mojo completion callbacks. Multiple parallel requests for the same
  // app are rejected.
  base::flat_map<webapps::AppId, UpdateManifestInstalledAppCallback>
      manifest_update_requests_;

  base::WeakPtrFactory<IwaDevPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_WEBUI_IWA_DEV_IWA_DEV_PAGE_HANDLER_H_
