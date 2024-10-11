// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/privacy/app_permission_handler.h"

#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/new_window_delegate.h"
#include "ash/webui/projector_app/public/cpp/projector_app_constants.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/eche_app/app_id.h"
#include "chrome/common/url_constants.h"
#include "components/services/app_service/public/cpp/permission.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "url/gurl.h"

namespace ash::settings {

namespace {

// Returns true if `permission` is one of {apps::PermissionType::kCamera,
// apps::PermissionType::kLocation, apps::PermissionType::kMicrophone}.
bool IsPermissionTypeRelevant(const apps::PermissionPtr& permission) {
  return (permission->permission_type == apps::PermissionType::kCamera ||
          permission->permission_type == apps::PermissionType::kLocation ||
          permission->permission_type == apps::PermissionType::kMicrophone);
}

bool HasRelevantPermission(const apps::AppUpdate& update) {
  return base::ranges::any_of(update.Permissions(), &IsPermissionTypeRelevant);
}

app_permission::mojom::AppPtr CreateAppPtr(const apps::AppUpdate& update) {
  auto app = app_permission::mojom::App::New();
  app->id = update.AppId();
  app->name = update.Name();
  app->type = update.AppType();

  for (const auto& permission : update.Permissions()) {
    if (IsPermissionTypeRelevant(permission)) {
      app->permissions[permission->permission_type] = permission->Clone();
    }
  }
  return app;
}

// Returns true if the system app with ID `app_id` uses camera.
bool SystemAppUsesCamera(const std::string& app_id) {
  return app_id == web_app::kCameraAppId ||
         app_id == web_app::kPersonalizationAppId ||
         app_id == ash::kChromeUIUntrustedProjectorSwaAppId;
}

// Returns true if the system app with ID `app_id` uses microphone.
bool SystemAppUsesMicrophone(const std::string& app_id) {
  return app_id == web_app::kCameraAppId ||
         app_id == ash::eche_app::kEcheAppId ||
         app_id == ash::kChromeUIUntrustedProjectorSwaAppId;
}

}  // namespace

using app_permission::mojom::AppPermissionsHandler;
using app_permission::mojom::AppPermissionsObserver;

AppPermissionHandler::AppPermissionHandler(
    apps::AppServiceProxy* app_service_proxy)
    : app_service_proxy_(app_service_proxy) {
  app_registry_cache_observer_.Observe(&app_service_proxy_->AppRegistryCache());
}

AppPermissionHandler::~AppPermissionHandler() {}

void AppPermissionHandler::AddObserver(
    mojo::PendingRemote<app_permission::mojom::AppPermissionsObserver>
        observer) {
  observer_list_.Add(std::move(observer));
}

void AppPermissionHandler::BindInterface(
    mojo::PendingReceiver<app_permission::mojom::AppPermissionsHandler>
        receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void AppPermissionHandler::GetApps(
    base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
        callback) {
  std::move(callback).Run(GetAppList());
}

void AppPermissionHandler::GetSystemAppsThatUseCamera(
    base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
        callback) {
  std::move(callback).Run(GetSystemAppListThatUsesCamera());
}

void AppPermissionHandler::GetSystemAppsThatUseMicrophone(
    base::OnceCallback<void(std::vector<app_permission::mojom::AppPtr>)>
        callback) {
  std::move(callback).Run(GetSystemAppListThatUsesMicrophone());
}

void AppPermissionHandler::OpenBrowserPermissionSettings(
    apps::PermissionType permission_type) {
  GURL url;

  switch (permission_type) {
    case apps::PermissionType::kCamera:
      url = GURL(chrome::kBrowserCameraPermissionsSettingsURL);
      break;
    case apps::PermissionType::kLocation:
      url = GURL(chrome::kBrowserLocationPermissionsSettingsURL);
      break;
    case apps::PermissionType::kMicrophone:
      url = GURL(chrome::kBrowserMicrophonePermissionsSettingsURL);
      break;
    case apps::PermissionType::kUnknown:
    case apps::PermissionType::kNotifications:
    case apps::PermissionType::kContacts:
    case apps::PermissionType::kStorage:
    case apps::PermissionType::kPrinting:
    case apps::PermissionType::kFileHandling:
      NOTREACHED();
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      url, ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kSwitchToTab);
}

void AppPermissionHandler::OpenNativeSettings(const std::string& app_id) {
  app_service_proxy_->OpenNativeSettings(app_id);
}

void AppPermissionHandler::SetPermission(const std::string& app_id,
                                         apps::PermissionPtr permission) {
  app_service_proxy_->SetPermission(app_id, std::move(permission));
}

void AppPermissionHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (!HasRelevantPermission(update)) {
    // This app is irrelevant for the Privacy controls sensor subpages.
    return;
  }

  if (!apps_util::IsInstalled(update.Readiness()) ||
      !update.ShowInManagement().value_or(true)) {
    for (const auto& observer : observer_list_) {
      observer->OnAppRemoved(update.AppId());
    }
  } else {
    for (const auto& observer : observer_list_) {
      observer->OnAppUpdated(CreateAppPtr(update));
    }
  }
}

std::vector<app_permission::mojom::AppPtr> AppPermissionHandler::GetAppList() {
  std::vector<app_permission::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (update.ShowInManagement().value_or(false) &&
            apps_util::IsInstalled(update.Readiness()) &&
            HasRelevantPermission(update)) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

std::vector<app_permission::mojom::AppPtr>
AppPermissionHandler::GetSystemAppListThatUsesCamera() {
  std::vector<app_permission::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness()) &&
            SystemAppUsesCamera(update.AppId())) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

std::vector<app_permission::mojom::AppPtr>
AppPermissionHandler::GetSystemAppListThatUsesMicrophone() {
  std::vector<app_permission::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (apps_util::IsInstalled(update.Readiness()) &&
            SystemAppUsesMicrophone(update.AppId())) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

void AppPermissionHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

}  // namespace ash::settings
