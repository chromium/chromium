// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_

#include "ash/public/cpp/message_center_ash.h"
#include "chrome/browser/ui/webui/app_management/app_management.mojom.h"
#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/mojom/app_notification_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace apps {
class AppServiceProxyChromeOs;
}  // namespace apps

namespace chromeos {
namespace settings {

class AppNotificationHandler
    : public app_notification::mojom::AppNotificationsHandler,
      public ash::MessageCenterAsh::Observer,
      public apps::AppRegistryCache::Observer {
 public:
  explicit AppNotificationHandler(
      apps::AppServiceProxyChromeOs* app_service_proxy);
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
  void NotifyPageReady() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void GetApps();

  bool in_quiet_mode_;

  mojo::RemoteSet<app_notification::mojom::AppNotificationsObserver>
      observer_list_;

  apps::AppServiceProxyChromeOs* app_service_proxy_;
  std::vector<app_notification::mojom::AppPtr> apps_;

  mojo::Receiver<app_notification::mojom::AppNotificationsHandler> receiver_{
      this};
};

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_OS_APPS_PAGE_APP_NOTIFICATION_HANDLER_H_
