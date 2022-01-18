// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_types.h"

namespace apps {

App::App(AppType app_type, const std::string& app_id)
    : app_type(app_type), app_id(app_id) {}

App::~App() = default;

std::unique_ptr<App> App::Clone() const {
  std::unique_ptr<App> app = std::make_unique<App>(app_type, app_id);

  app->readiness = readiness;
  app->name = name;
  app->short_name = short_name;
  app->publisher_id = publisher_id;
  app->description = description;
  app->version = version;
  app->additional_search_terms = additional_search_terms;

  if (icon_key.has_value()) {
    app->icon_key = apps::IconKey(icon_key->timeline, icon_key->resource_id,
                                  icon_key->icon_effects);
  }

  app->last_launch_time = last_launch_time;
  app->install_time = install_time;
  app->permissions = ClonePermissions(permissions);
  app->install_reason = install_reason;
  app->install_source = install_source;

  app->policy_id = policy_id;

  return app;
}

AppType ConvertMojomAppTypToAppType(apps::mojom::AppType mojom_app_type) {
  switch (mojom_app_type) {
    case apps::mojom::AppType::kUnknown:
      return AppType::kUnknown;
    case apps::mojom::AppType::kArc:
      return AppType::kArc;
    case apps::mojom::AppType::kBuiltIn:
      return AppType::kBuiltIn;
    case apps::mojom::AppType::kCrostini:
      return AppType::kCrostini;
    case apps::mojom::AppType::kChromeApp:
      return AppType::kChromeApp;
    case apps::mojom::AppType::kWeb:
      return AppType::kWeb;
    case apps::mojom::AppType::kMacOs:
      return AppType::kMacOs;
    case apps::mojom::AppType::kPluginVm:
      return AppType::kPluginVm;
    case apps::mojom::AppType::kStandaloneBrowser:
      return AppType::kStandaloneBrowser;
    case apps::mojom::AppType::kRemote:
      return AppType::kRemote;
    case apps::mojom::AppType::kBorealis:
      return AppType::kBorealis;
    case apps::mojom::AppType::kSystemWeb:
      return AppType::kSystemWeb;
    case apps::mojom::AppType::kStandaloneBrowserChromeApp:
      return AppType::kStandaloneBrowserChromeApp;
    case apps::mojom::AppType::kExtension:
      return AppType::kExtension;
  }
}

mojom::AppType ConvertAppTypeToMojomAppType(AppType app_type) {
  switch (app_type) {
    case AppType::kUnknown:
      return apps::mojom::AppType::kUnknown;
    case AppType::kArc:
      return apps::mojom::AppType::kArc;
    case AppType::kBuiltIn:
      return apps::mojom::AppType::kBuiltIn;
    case AppType::kCrostini:
      return apps::mojom::AppType::kCrostini;
    case AppType::kChromeApp:
      return apps::mojom::AppType::kChromeApp;
    case AppType::kWeb:
      return apps::mojom::AppType::kWeb;
    case AppType::kMacOs:
      return apps::mojom::AppType::kMacOs;
    case AppType::kPluginVm:
      return apps::mojom::AppType::kPluginVm;
    case AppType::kStandaloneBrowser:
      return apps::mojom::AppType::kStandaloneBrowser;
    case AppType::kRemote:
      return apps::mojom::AppType::kRemote;
    case AppType::kBorealis:
      return apps::mojom::AppType::kBorealis;
    case AppType::kSystemWeb:
      return apps::mojom::AppType::kSystemWeb;
    case AppType::kStandaloneBrowserChromeApp:
      return apps::mojom::AppType::kStandaloneBrowserChromeApp;
    case AppType::kExtension:
      return apps::mojom::AppType::kExtension;
  }
}

Readiness ConvertMojomReadinessToReadiness(
    apps::mojom::Readiness mojom_readiness) {
  switch (mojom_readiness) {
    case apps::mojom::Readiness::kUnknown:
      return Readiness::kUnknown;
    case apps::mojom::Readiness::kReady:
      return Readiness::kReady;
    case apps::mojom::Readiness::kDisabledByBlocklist:
      return Readiness::kDisabledByBlocklist;
    case apps::mojom::Readiness::kDisabledByPolicy:
      return Readiness::kDisabledByPolicy;
    case apps::mojom::Readiness::kDisabledByUser:
      return Readiness::kDisabledByUser;
    case apps::mojom::Readiness::kTerminated:
      return Readiness::kTerminated;
    case apps::mojom::Readiness::kUninstalledByUser:
      return Readiness::kUninstalledByUser;
    case apps::mojom::Readiness::kRemoved:
      return Readiness::kRemoved;
    case apps::mojom::Readiness::kUninstalledByMigration:
      return Readiness::kUninstalledByMigration;
  }
}

InstallReason ConvertMojomInstallReasonToInstallReason(
    apps::mojom::InstallReason mojom_install_reason) {
  switch (mojom_install_reason) {
    case apps::mojom::InstallReason::kUnknown:
      return InstallReason::kUnknown;
    case apps::mojom::InstallReason::kSystem:
      return InstallReason::kSystem;
    case apps::mojom::InstallReason::kPolicy:
      return InstallReason::kPolicy;
    case apps::mojom::InstallReason::kOem:
      return InstallReason::kOem;
    case apps::mojom::InstallReason::kDefault:
      return InstallReason::kDefault;
    case apps::mojom::InstallReason::kSync:
      return InstallReason::kSync;
    case apps::mojom::InstallReason::kUser:
      return InstallReason::kUser;
    case apps::mojom::InstallReason::kSubApp:
      return InstallReason::kSubApp;
  }
}

InstallSource ConvertMojomInstallSourceToInstallSource(
    apps::mojom::InstallSource mojom_install_source) {
  switch (mojom_install_source) {
    case apps::mojom::InstallSource::kUnknown:
      return InstallSource::kUnknown;
    case apps::mojom::InstallSource::kSystem:
      return InstallSource::kSystem;
    case apps::mojom::InstallSource::kSync:
      return InstallSource::kSync;
    case apps::mojom::InstallSource::kPlayStore:
      return InstallSource::kPlayStore;
    case apps::mojom::InstallSource::kChromeWebStore:
      return InstallSource::kChromeWebStore;
    case apps::mojom::InstallSource::kBrowser:
      return InstallSource::kBrowser;
  }
}

std::unique_ptr<App> ConvertMojomAppToApp(
    const apps::mojom::AppPtr& mojom_app) {
  DCHECK(mojom_app);
  std::unique_ptr<App> app = std::make_unique<App>(
      ConvertMojomAppTypToAppType(mojom_app->app_type), mojom_app->app_id);

  app->readiness = ConvertMojomReadinessToReadiness(mojom_app->readiness);
  app->name = mojom_app->name;
  app->short_name = mojom_app->short_name;
  app->publisher_id = mojom_app->publisher_id;
  app->description = mojom_app->description;
  app->version = mojom_app->version;
  app->additional_search_terms = mojom_app->additional_search_terms;

  if (mojom_app->icon_key) {
    app->icon_key = apps::IconKey(mojom_app->icon_key->timeline,
                                  mojom_app->icon_key->resource_id,
                                  mojom_app->icon_key->icon_effects);
  }

  app->last_launch_time = mojom_app->last_launch_time;
  app->install_time = mojom_app->install_time;

  for (const auto& mojom_permission : mojom_app->permissions) {
    auto permission = ConvertMojomPermissionToPermission(mojom_permission);
    if (permission) {
      app->permissions.push_back(std::move(permission));
    }
  }

  app->install_reason =
      ConvertMojomInstallReasonToInstallReason(mojom_app->install_reason);
  app->install_source =
      ConvertMojomInstallSourceToInstallSource(mojom_app->install_source);

  app->policy_id = mojom_app->policy_id;

  return app;
}

}  // namespace apps
