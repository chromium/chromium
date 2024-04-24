// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/apps/app_parental_controls_handler.h"

#include <utility>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/webui/ash/settings/pages/apps/mojom/app_parental_controls_handler.mojom.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace ash::settings {

namespace {
app_parental_controls::mojom::AppPtr CreateAppPtr(
    const apps::AppUpdate& update) {
  auto app = app_parental_controls::mojom::App::New();
  app->id = update.AppId();
  app->title = update.Name();
  return app;
}

bool ShouldIncludeApp(const apps::AppUpdate& update) {
  // Only apps shown in the App Management page should be shown.
  return update.ShowInManagement().value_or(false) &&
         update.AppType() == apps::AppType::kArc;
}
}  // namespace

AppParentalControlsHandler::AppParentalControlsHandler(
    apps::AppServiceProxy* app_service_proxy)
    : app_service_proxy_(app_service_proxy) {
  app_registry_cache_observer_.Observe(&app_service_proxy_->AppRegistryCache());
}

AppParentalControlsHandler::~AppParentalControlsHandler() = default;

void AppParentalControlsHandler::BindInterface(
    mojo::PendingReceiver<
        app_parental_controls::mojom::AppParentalControlsHandler> receiver) {
  if (receiver_.is_bound()) {
    receiver_.reset();
  }
  receiver_.Bind(std::move(receiver));
}

void AppParentalControlsHandler::GetApps(GetAppsCallback callback) {
  std::move(callback).Run(GetAppList());
}

std::vector<app_parental_controls::mojom::AppPtr>
AppParentalControlsHandler::GetAppList() {
  std::vector<app_parental_controls::mojom::AppPtr> apps;
  app_service_proxy_->AppRegistryCache().ForEachApp(
      [&apps](const apps::AppUpdate& update) {
        if (ShouldIncludeApp(update) &&
            apps_util::IsInstalled(update.Readiness())) {
          apps.push_back(CreateAppPtr(update));
        }
      });
  return apps;
}

void AppParentalControlsHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

}  // namespace ash::settings
