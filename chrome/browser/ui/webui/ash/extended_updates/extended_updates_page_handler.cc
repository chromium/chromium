// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates_page_handler.h"

#include <algorithm>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/extended_updates/extended_updates_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/extended_updates/extended_updates.mojom.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::extended_updates {

ExtendedUpdatesPageHandler::ExtendedUpdatesPageHandler(
    mojo::PendingRemote<ash::extended_updates::mojom::Page> page,
    mojo::PendingReceiver<ash::extended_updates::mojom::PageHandler> receiver,
    content::WebUI* web_ui,
    base::OnceClosure close_dialog_callback)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      web_ui_(web_ui),
      close_dialog_callback_(std::move(close_dialog_callback)) {}

ExtendedUpdatesPageHandler::~ExtendedUpdatesPageHandler() = default;

void ExtendedUpdatesPageHandler::OptInToExtendedUpdates(
    OptInToExtendedUpdatesCallback callback) {
  auto* profile = Profile::FromWebUI(web_ui_);
  auto* controller = ash::ExtendedUpdatesController::Get();
  if (controller->IsOptInEligible(profile)) {
    std::move(callback).Run(controller->OptIn(profile));
    return;
  }
  std::move(callback).Run(false);
}

void ExtendedUpdatesPageHandler::CloseDialog() {
  if (close_dialog_callback_) {
    std::move(close_dialog_callback_).Run();
  }
}

void ExtendedUpdatesPageHandler::GetInstalledAndroidApps(
    GetInstalledAndroidAppsCallback callback) {
  std::vector<mojom::AppPtr> apps;
  apps::AppServiceProxyFactory::GetForProfile(Profile::FromWebUI(web_ui_))
      ->AppRegistryCache()
      .ForEachApp([&apps](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kArc &&
            update.Readiness() == apps::Readiness::kReady) {
          apps.push_back(mojom::App::New(update.AppId(), update.ShortName()));
        }
      });
  std::sort(apps.begin(), apps.end(),
            [](const mojom::AppPtr& lhs, const mojom::AppPtr& rhs) {
              return lhs->title < rhs->title;
            });
  std::move(callback).Run(std::move(apps));
}

}  // namespace ash::extended_updates
