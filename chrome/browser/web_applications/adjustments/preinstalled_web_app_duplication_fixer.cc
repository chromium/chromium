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

const char PreinstalledWebAppDuplicationFixer::
    kHistogramWebAppAbsentChromeAppAbsent[] =
        "WebApp.Preinstalled.MigratingWebAppAbsentChromeAppAbsent";
const char PreinstalledWebAppDuplicationFixer::
    kHistogramWebAppAbsentChromeAppPresent[] =
        "WebApp.Preinstalled.MigratingWebAppAbsentChromeAppPresent";
const char PreinstalledWebAppDuplicationFixer::
    kHistogramWebAppPresentChromeAppAbsent[] =
        "WebApp.Preinstalled.MigratingWebAppPresentChromeAppAbsent";
const char PreinstalledWebAppDuplicationFixer::
    kHistogramWebAppPresentChromeAppPresent[] =
        "WebApp.Preinstalled.MigratingWebAppPresentChromeAppPresent";
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
  web_apps_ready_ =
      app_registry_cache.IsAppTypeInitialized(apps::AppType::kWeb);
  chrome_apps_ready_ =
      app_registry_cache.IsAppTypeInitialized(apps::AppType::kChromeApp);
  if (web_apps_ready_ && chrome_apps_ready_) {
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
    apps::AppType app_type) {
  if (app_type == apps::AppType::kWeb)
    web_apps_ready_ = true;

  if (app_type == apps::AppType::kChromeApp)
    chrome_apps_ready_ = true;

  if (web_apps_ready_ && chrome_apps_ready_) {
    ScanForDuplication();
    scoped_observation_.Reset();
  }
}

void PreinstalledWebAppDuplicationFixer::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

void PreinstalledWebAppDuplicationFixer::ScanForDuplicationForTesting() {
  ScanForDuplication();
}

void PreinstalledWebAppDuplicationFixer::ScanForDuplication() {
  std::vector<std::string> installed_web_apps;
  std::vector<std::string> installed_chrome_apps;
  apps::AppServiceProxyFactory::GetForProfile(&profile_)
      ->AppRegistryCache()
      .ForAllApps([&installed_web_apps,
                   &installed_chrome_apps](const apps::AppUpdate& update) {
        if (update.Readiness() != apps::Readiness::kReady)
          return;

        if (update.AppType() == apps::AppType::kWeb)
          installed_web_apps.push_back(update.AppId());
        else if (update.AppType() == apps::AppType::kChromeApp)
          installed_chrome_apps.push_back(update.AppId());
      });

  size_t fix_count = 0;
  base::flat_set<std::string> installed_web_apps_set(
      std::move(installed_web_apps));
  base::flat_set<std::string> installed_chrome_apps_set(
      std::move(installed_chrome_apps));

  int installed_tally[2][2] = {};

  for (const PreinstalledWebAppMigration& migration :
       GetPreinstalledWebAppMigrations(profile_)) {
    bool web_app_installed =
        installed_web_apps_set.contains(migration.expected_web_app_id);
    bool chrome_app_installed =
        installed_chrome_apps_set.contains(migration.old_chrome_app_id);

    if (chrome_app_installed) {
      // Remove evidence of web app installation causing
      // PreinstalledWebAppManager::Synchronize() to reinstall the web
      // app and re-trigger migration.
      if (ExternallyInstalledWebAppPrefs(profile_.GetPrefs())
              .Remove(migration.install_url)) {
        ++fix_count;
      }
    }

    ++installed_tally[web_app_installed][chrome_app_installed];
  }

  base::UmaHistogramCounts100(kHistogramWebAppAbsentChromeAppAbsent,
                              installed_tally[false][false]);
  base::UmaHistogramCounts100(kHistogramWebAppAbsentChromeAppPresent,
                              installed_tally[false][true]);
  base::UmaHistogramCounts100(kHistogramWebAppPresentChromeAppAbsent,
                              installed_tally[true][false]);
  base::UmaHistogramCounts100(kHistogramWebAppPresentChromeAppPresent,
                              installed_tally[true][true]);

  base::UmaHistogramCounts100(kHistogramAppDuplicationFixApplied, fix_count);
}

}  // namespace web_app
