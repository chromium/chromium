// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_notification_handler.h"

#include <utility>

#include "ash/public/cpp/message_center_ash.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_type_mojom_traits.h"
#include "chrome/common/url_constants.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace ash::settings {

namespace {
app_notification::mojom::AppPtr CreateAppPtr(const apps::AppUpdate& update) {
  auto app = app_notification::mojom::App::New();
  app->id = update.AppId();
  app->title = update.Name();
  app->readiness = update.Readiness();

  for (const auto& permission : update.Permissions()) {
    if (permission->permission_type == apps::PermissionType::kNotifications) {
      app->notification_permission = permission->Clone();
      break;
    }
  }

  return app;
}

bool ShouldIncludeApp(const apps::AppUpdate& update) {
  // Only apps that can be shown in management are supported.
  if (!update.ShowInManagement().value_or(false)) {
    return false;
  }

  // Only kArc and kWeb apps are supported.
  if (update.AppType() == apps::AppType::kArc ||
      update.AppType() == apps::AppType::kWeb) {
    for (const auto& permission : update.Permissions()) {
      if (permission->permission_type == apps::PermissionType::kNotifications) {
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
    apps::AppServiceProxy* app_service_proxy)
    : app_service_proxy_(app_service_proxy) {
  if (ash::MessageCenterAsh::Get()) {
    ash::MessageCenterAsh::Get()->AddObserver(this);
  }
  app_registry_cache_observer_.Observe(&app_service_proxy_->AppRegistryCache());
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
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
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
    apps::PermissionPtr permission) {
  app_service_proxy_->SetPermission(app_id, std::move(permission));
}

void AppNotificationHandler::OpenBrowserNotificationSettings() {
  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(chrome::kAppNotificationsBrowserSettingsURL),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void AppNotificationHandler::GetApps(GetAppsCallback callback) {
  std::move(callback).Run(GetAppList());
}

void AppNotificationHandler::GetQuietMode(GetQuietModeCallback callback) {
  std::move(callback).Run(ash::MessageCenterAsh::Get()->IsQuietMode());
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
  app_registry_cache_observer_.Reset();
}

}  // namespace ash::settings
