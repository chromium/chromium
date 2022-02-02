// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/preinstalled_web_app_duplication_fixer.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/post_task.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/preinstalled_app_install_features.h"
#include "chrome/browser/web_applications/preinstalled_web_apps/preinstalled_web_apps.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_registrar.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace web_app {

namespace {
bool g_skip_startup_for_testing = false;
}

const char
    PreinstalledWebAppDuplicationFixer::kHistogramAppDuplicationFixApplied[] =
        "WebApp.Preinstalled.AppDuplicationFixApplied";

void PreinstalledWebAppDuplicationFixer::SkipStartupForTesting() {
  g_skip_startup_for_testing = true;
}

PreinstalledWebAppDuplicationFixer::PreinstalledWebAppDuplicationFixer(
    Profile& profile)
    : profile_(profile) {
  if (g_skip_startup_for_testing)
    return;
  // WebAppAdjustmentsFactory guarantees that AppServiceProxy exists.
  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(&profile_)
          ->AppRegistryCache();
  if (app_registry_cache.IsAppTypeInitialized(
          apps::mojom::AppType::kChromeApp)) {
    ScanForDuplication();
  } else {
    // Await OnAppTypeInitialized().
    scoped_observation_.Observe(&app_registry_cache);
  }
}

PreinstalledWebAppDuplicationFixer::~PreinstalledWebAppDuplicationFixer() =
    default;

void PreinstalledWebAppDuplicationFixer::OnAppUpdate(
    const apps::AppUpdate& update) {}

void PreinstalledWebAppDuplicationFixer::OnAppTypeInitialized(
    apps::mojom::AppType app_type) {
  if (app_type != apps::mojom::AppType::kChromeApp)
    return;

  ScanForDuplication();
  scoped_observation_.Reset();
}

void PreinstalledWebAppDuplicationFixer::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

void PreinstalledWebAppDuplicationFixer::ScanForDuplicationForTesting() {
  ScanForDuplication();
}

void PreinstalledWebAppDuplicationFixer::ScanForDuplication() {
  std::vector<PreinstalledWebAppMigration> migrations =
      GetPreinstalledWebAppMigrations(profile_);
  size_t fix_count = 0;
  apps::AppServiceProxyFactory::GetForProfile(&profile_)
      ->AppRegistryCache()
      .ForEachApp(
          [this, &migrations, &fix_count](const apps::AppUpdate& update) {
            if (update.AppType() != apps::mojom::AppType::kChromeApp)
              return;

            if (update.Readiness() != apps::mojom::Readiness::kReady)
              return;

            for (const PreinstalledWebAppMigration& migration : migrations) {
              if (update.AppId() != migration.old_chrome_app_id)
                continue;
              // Remove evidence of web app installation causing
              // PreinstalledWebAppManager::Synchronize() to reinstall the web
              // app and re-trigger migration.
              fix_count += ExternallyInstalledWebAppPrefs(profile_.GetPrefs())
                               .Remove(migration.install_url);
            }
          });

  base::UmaHistogramCounts100(kHistogramAppDuplicationFixApplied, fix_count);
}

}  // namespace web_app
