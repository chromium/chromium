// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/chromeos/os_apps_page/app_notification_handler.h"

#include <utility>

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
    if (permission->permission_type ==
        apps::mojom::PermissionType::kNotifications) {
      permission_copy = permission->Clone();
      break;
    }
  }

  auto app = app_notification::mojom::App::New();
  app->id = update.AppId();
  app->title = update.Name();
  app->notification_permission = std::move(permission_copy);
  app->readiness = update.Readiness();

  return app;
}

std::vector<app_notification::mojom::AppPtr> Clone(
    const std::vector<app_notification::mojom::AppPtr>& apps) {
  std::vector<app_notification::mojom::AppPtr> cloned_apps;
  for (const auto& app : apps) {
    cloned_apps.push_back(app.Clone());
  }
  return cloned_apps;
}

bool ShouldIncludeApp(const apps::AppUpdate& update) {
  // Only apps that can be shown in management are supported.
  if (update.ShowInManagement() != apps::mojom::OptionalBool::kTrue) {
    return false;
  }

  // Only kArc and kWeb apps are supported.
  if (update.AppType() == apps::mojom::AppType::kArc ||
      update.AppType() == apps::mojom::AppType::kWeb) {
    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type ==
          apps::mojom::PermissionType::kNotifications) {
        return true;
      }
    }
  }

  return false;
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
  for (const auto& observer : observer_list_) {
    observer->OnQuietModeChanged(in_quiet_mode);
  }
}

void AppNotificationHandler::SetQuietMode(bool in_quiet_mode) {
  ash::MessageCenterAsh::Get()->SetQuietMode(in_quiet_mode);
}

void AppNotificationHandler::SetNotificationPermission(
    const std::string& app_id,
    apps::mojom::PermissionPtr permission) {
  app_service_proxy_->SetPermission(app_id, std::move(permission));
}

void AppNotificationHandler::GetApps(GetAppsCallback callback) {
  std::move(callback).Run(GetAppList());
}

void AppNotificationHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (ShouldIncludeApp(update)) {
    // Uninstalled apps are allowed to be sent as an update.
    NotifyAppChanged(CreateAppPtr(update));
  }
}

std::vector<app_notification::mojom::AppPtr>
AppNotificationHandler::GetAppList() {
  std::vector<app_notification::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (ShouldIncludeApp(update) &&
            apps_util::IsInstalled(update.Readiness())) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

void AppNotificationHandler::NotifyAppChanged(
    app_notification::mojom::AppPtr app) {
  for (const auto& observer : observer_list_) {
    observer->OnNotificationAppChanged(app.Clone());
  }
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
