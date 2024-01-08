// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_NOTIFICATION_HANDLER_H_

#include "ash/public/cpp/message_center_ash.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy_forward.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_notification_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash::settings {

class AppNotificationHandler
    : public app_notification::mojom::AppNotificationsHandler,
      public ash::MessageCenterAsh::Observer,
      public apps::AppRegistryCache::Observer {
 public:
  explicit AppNotificationHandler(apps::AppServiceProxy* app_service_proxy);
  ~AppNotificationHandler() override;

  // app_notification::mojom::AppNotificationHandler:
  void AddObserver(
      mojo::PendingRemote<app_notification::mojom::AppNotificationsObserver>
          observer) override;

  void BindInterface(
      mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
          receiver);

 private:
  friend class AppNotificationHandlerTest;

  // MessageCenterAsh::Observer override:
  void OnQuietModeChanged(bool in_quiet_mode) override;

  // settings::mojom::AppNotificationHandler:
  void SetQuietMode(bool in_quiet_mode) override;
  void SetNotificationPermission(const std::string& app_id,
                                 apps::PermissionPtr permission) override;
  void GetApps(GetAppsCallback callback) override;
  void GetQuietMode(GetQuietModeCallback callback) override;
  void OpenBrowserNotificationSettings() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  std::vector<app_notification::mojom::AppPtr> GetAppList();
  void NotifyAppChanged(app_notification::mojom::AppPtr app);

  mojo::RemoteSet<app_notification::mojom::AppNotificationsObserver>
      observer_list_;

  raw_ptr<apps::AppServiceProxy> app_service_proxy_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  mojo::Receiver<app_notification::mojom::AppNotificationsHandler> receiver_{
      this};
};

}  // namespace ash::settings

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_SETTINGS_PAGES_APPS_APP_NOTIFICATION_HANDLER_H_
