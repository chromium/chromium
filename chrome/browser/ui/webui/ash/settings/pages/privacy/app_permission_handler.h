// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_APP_PERMISSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_APP_PERMISSION_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/privacy/mojom/app_permission_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::settings {

class AppPermissionHandler
    : public app_permission::mojom::AppPermissionsHandler,
      public apps::AppRegistryCache::Observer {
 public:
  explicit AppPermissionHandler(apps::AppServiceProxy* app_service_proxy);
  ~AppPermissionHandler() override;

  // app_permission::mojom::AppPermissionsHandler:
  void AddObserver(
      mojo::PendingRemote<app_permission::mojom::AppPermissionsObserver>
          observer) override;

  void BindInterface(
      mojo::PendingReceiver<app_permission::mojom::AppPermissionsHandler>
          receiver);

 private:
  friend class AppPermissionHandlerTest;

  // settings::mojom::AppPermissionsHandler:
  void GetApps(
      base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
          callback) override;
  void GetSystemAppsThatUseCamera(
      base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
          callback) override;
  void GetSystemAppsThatUseMicrophone(
      base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
          callback) override;
  void OpenBrowserPermissionSettings(
      apps::PermissionType permission_type) override;
  void OpenNativeSettings(const std::string& app_id) override;
  void SetPermission(const std::string& app_id,
                     apps::PermissionPtr permission) override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  std::vector<app_permission::mojom::AppPtr> GetAppList();
  std::vector<app_permission::mojom::AppPtr> GetSystemAppListThatUsesCamera();
  std::vector<app_permission::mojom::AppPtr>
  GetSystemAppListThatUsesMicrophone();

  mojo::RemoteSet<app_permission::mojom::AppPermissionsObserver> observer_list_;

  raw_ptr<apps::AppServiceProxy> app_service_proxy_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  mojo::Receiver<app_permission::mojom::AppPermissionsHandler> receiver_{this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_PRIVACY_APP_PERMISSION_HANDLER_H_
