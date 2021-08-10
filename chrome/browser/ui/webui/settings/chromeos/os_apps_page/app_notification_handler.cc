// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"

#include "ash/public/cpp/message_center_ash.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace chromeos {
namespace settings {

namespace {
app_notification::mojom::AppPtr CreateAppPtr(const apps::AppUpdate& update) {
  apps::mojom::PermissionPtr permission_copy;
  for (const auto& permission : update.Permissions()) {
    if ((static_cast<app_management::mojom::PwaPermissionType>(
             permission->permission_id) ==
         app_management::mojom::PwaPermissionType::NOTIFICATIONS) ||
        (static_cast<app_management::mojom::ArcPermissionType>(
             permission->permission_id) ==
         app_management::mojom::ArcPermissionType::NOTIFICATIONS)) {
      permission_copy = permission->Clone();
      break;
    }
  }

  auto app = app_notification::mojom::App::New();
  app->id = update.AppId();
  app->title = update.Name();
  app->notification_permission = std::move(permission_copy);

  return app;
}
}  // namespace

using app_notification::mojom::AppNotificationsHandler;
using app_notification::mojom::AppNotificationsObserver;

AppNotificationHandler::AppNotificationHandler(
    apps::AppServiceProxyChromeOs* app_service_proxy)
    : app_service_proxy_(app_service_proxy) {
  if (ash::MessageCenterAsh::Get()) {
    ash::MessageCenterAsh::Get()->AddObserver(this);
  }
  // app_service_proxy_ = apps::AppServiceProxyFactory::GetForProfile(profile_);
  Observe(&app_service_proxy_->AppRegistryCache());
}

AppNotificationHandler::~AppNotificationHandler() {
  if (ash::MessageCenterAsh::Get()) {
    ash::MessageCenterAsh::Get()->RemoveObserver(this);
  }
}

void AppNotificationHandler::AddObserver(
    mojo::PendingRemote<app_notification::mojom::AppNotificationsObserver>
        observer) {
  observer_list_.Add(std::move(observer));
}

void AppNotificationHandler::BindInterface(
    mojo::PendingReceiver<app_notification::mojom::AppNotificationsHandler>
        receiver) {
  if (receiver_.is_bound())
    receiver_.reset();
  receiver_.Bind(std::move(receiver));
}

void AppNotificationHandler::OnQuietModeChanged(bool in_quiet_mode) {
  in_quiet_mode_ = in_quiet_mode;
  for (const auto& observer : observer_list_) {
    observer->OnQuietModeChanged(in_quiet_mode);
  }
}

void AppNotificationHandler::SetQuietMode(bool in_quiet_mode) {
  ash::MessageCenterAsh::Get()->SetQuietMode(in_quiet_mode);
}

void AppNotificationHandler::GetApps() {
  std::vector<app_notification::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (update.ShowInManagement() != apps::mojom::OptionalBool::kTrue ||
            !apps_util::IsInstalled(update.Readiness())) {
          return;
        }

        // This statement only adds apps to the list if they are
        // of app_type kArc or kWeb.
        if (update.AppType() == apps::mojom::AppType::kArc) {
          for (const auto& permission : update.Permissions()) {
            if (static_cast<app_management::mojom::ArcPermissionType>(
                    permission->permission_id) ==
                app_management::mojom::ArcPermissionType::NOTIFICATIONS) {
              apps.push_back(CreateAppPtr(update));
              break;
            }
          }
        } else if (update.AppType() == apps::mojom::AppType::kWeb) {
          for (const auto& permission : update.Permissions()) {
            if (static_cast<app_management::mojom::PwaPermissionType>(
                    permission->permission_id) ==
                app_management::mojom::PwaPermissionType::NOTIFICATIONS) {
              apps.push_back(CreateAppPtr(update));
              break;
            }
          }
        }
      });
  apps_ = std::move(apps);
}

void AppNotificationHandler::OnAppUpdate(const apps::AppUpdate& update) {
  // Each time an update is observed the entire list of apps is refetched.
  // 'update' is a required parameter from the AppRegistryCache::Observer,
  // but is not used in this implementation.
  apps_.clear();
  GetApps();
}

void AppNotificationHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

void AppNotificationHandler::NotifyPageReady() {
  OnQuietModeChanged(ash::MessageCenterAsh::Get()->IsQuietMode());
}

}  // namespace settings
}  // namespace chromeos
