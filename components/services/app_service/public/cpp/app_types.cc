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

  if (icon_key.has_value()) {
    app->icon_key = apps::IconKey(icon_key->timeline, icon_key->resource_id,
                                  icon_key->icon_effects);
  }
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

  if (mojom_app->icon_key) {
    app->icon_key = apps::IconKey(mojom_app->icon_key->timeline,
                                  mojom_app->icon_key->resource_id,
                                  mojom_app->icon_key->icon_effects);
  }
  return app;
}

}  // namespace apps
