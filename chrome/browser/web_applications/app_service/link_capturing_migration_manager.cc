// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/app_service/link_capturing_migration_manager.h"

#include "base/task/post_task.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace web_app {

LinkCapturingMigrationManager::LinkCapturingMigrationManager(Profile& profile)
    : profile_(profile) {
  // Defer this to be an async operation as we are constructed during
  // AppServiceProxy's construction and thus cannot read the AppServiceProxy out
  // of AppServiceProxyFactory yet.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&LinkCapturingMigrationManager::ObserveAppRegistryCache,
                     weak_factory_.GetWeakPtr()));
}

LinkCapturingMigrationManager::~LinkCapturingMigrationManager() = default;

void LinkCapturingMigrationManager::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppType() != apps::mojom::AppType::kWeb)
    return;

  if (apps_util::IsInstalled(update.PriorReadiness()))
    return;

  if (!apps_util::IsInstalled(update.Readiness()))
    return;

  // The DLC API was never used by system web apps.
  const WebAppProvider* provider = WebAppProvider::GetForWebApps(&profile_);
  if (!provider)
    return;

  const WebApp* web_app = provider->registrar().GetAppById(update.AppId());
  if (!web_app)
    return;

  switch (web_app->capture_links()) {
    case blink::mojom::CaptureLinks::kUndefined:
    case blink::mojom::CaptureLinks::kNone:
      return;

    case blink::mojom::CaptureLinks::kNewClient:
    case blink::mojom::CaptureLinks::kExistingClientNavigate:
      apps::AppServiceProxyFactory::GetInstance()
          ->GetForProfile(&profile_)
          ->SetSupportedLinksPreference(update.AppId());
      break;
  }
}

void LinkCapturingMigrationManager::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

void LinkCapturingMigrationManager::ObserveAppRegistryCache() {
  scoped_observation_.Observe(&apps::AppServiceProxyFactory::GetInstance()
                                   ->GetForProfile(&profile_)
                                   ->AppRegistryCache());
}

}  // namespace web_app
