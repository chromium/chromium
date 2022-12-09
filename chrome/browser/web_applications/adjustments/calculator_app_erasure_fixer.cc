// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/adjustments/calculator_app_erasure_fixer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/extension_status_utils.h"
#include "chrome/browser/web_applications/externally_installed_web_app_prefs.h"
#include "chrome/browser/web_applications/user_uninstalled_preinstalled_web_app_prefs.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/common/constants.h"

namespace web_app {

namespace {

bool IsAppInstalled(Profile* profile, const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  bool is_installed = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&](const apps::AppUpdate& app_update) {
        is_installed = apps_util::IsInstalled(app_update.Readiness());
      });
  return is_installed;
}
}  // namespace

BASE_FEATURE(kWebAppCalculatorAppErasureFixer,
             "WebAppCalculatorAppErasureFixer",
             base::FEATURE_ENABLED_BY_DEFAULT);

const char kWebAppCalculatorAppErasureFixAppliedPref[] =
    "web_app.calculator_app_erasure_fix_applied";

const char kHistogramWebAppCalculatorAppErasureScanResult[] =
    "WebApp.CalculatorAppErasureScanResult";

void CalculatorAppErasureFixer::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(kWebAppCalculatorAppErasureFixAppliedPref,
                                false);
}

CalculatorAppErasureFixer::CalculatorAppErasureFixer(Profile& profile)
    : profile_(profile) {
  DCHECK(base::FeatureList::IsEnabled(kWebAppCalculatorAppErasureFixer));
  // WebAppAdjustmentsFactory guarantees that AppServiceProxy exists.
  apps::AppRegistryCache& app_registry_cache =
      apps::AppServiceProxyFactory::GetForProfile(&*profile_)
          ->AppRegistryCache();
  web_apps_ready_ =
      app_registry_cache.IsAppTypeInitialized(apps::AppType::kWeb);
  chrome_apps_ready_ =
      app_registry_cache.IsAppTypeInitialized(apps::AppType::kChromeApp);
  if (web_apps_ready_ && chrome_apps_ready_) {
    ScanForCalculatorAppErasureAndEmitMetrics();
  } else {
    // Await OnAppTypeInitialized().
    scoped_observation_.Observe(&app_registry_cache);
  }
}

CalculatorAppErasureFixer::~CalculatorAppErasureFixer() = default;

void CalculatorAppErasureFixer::OnAppUpdate(const apps::AppUpdate& update) {}

void CalculatorAppErasureFixer::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == apps::AppType::kWeb) {
    web_apps_ready_ = true;
  }

  if (app_type == apps::AppType::kChromeApp) {
    chrome_apps_ready_ = true;
  }

  if (web_apps_ready_ && chrome_apps_ready_) {
    ScanForCalculatorAppErasureAndEmitMetrics();
    scoped_observation_.Reset();
  }
}

void CalculatorAppErasureFixer::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  scoped_observation_.Reset();
}

void CalculatorAppErasureFixer::ScanForCalculatorAppErasureAndEmitMetrics() {
  base::UmaHistogramEnumeration(kHistogramWebAppCalculatorAppErasureScanResult,
                                ScanForCalculatorAppErasure());
}

CalculatorAppErasureFixer::ScanResult
CalculatorAppErasureFixer::ScanForCalculatorAppErasure() {
  // The bug only triggers when web apps are in Ash, so we do not need to run
  // the fix if web apps are in Lacros.
  if (!WebAppProvider::GetForWebApps(&*profile_)) {
    return ScanResult::kNoWebAppsForProcess;
  }

  // Only run the fix if neither Calculator app is installed.
  bool web_app_installed =
      IsAppInstalled(&*profile_, web_app::kCalculatorAppId);
  bool chrome_app_installed =
      IsAppInstalled(&*profile_, extension_misc::kCalculatorAppId);
  if (web_app_installed && chrome_app_installed) {
    return ScanResult::kBothAppsInstalled;
  } else if (web_app_installed) {
    return ScanResult::kWebAppInstalled;
  } else if (chrome_app_installed) {
    return ScanResult::kChromeAppInstalled;
  }

  // Only run the fix if both Calculator apps are marked as user uninstalled.
  UserUninstalledPreinstalledWebAppPrefs web_app_prefs(profile_->GetPrefs());
  bool web_app_user_uninstalled =
      web_app_prefs.DoesAppIdExist(web_app::kCalculatorAppId);
  bool chrome_app_user_uninstalled = extensions::IsExternalExtensionUninstalled(
      &*profile_, extension_misc::kCalculatorAppId);
  if (!web_app_user_uninstalled && !chrome_app_user_uninstalled) {
    return ScanResult::kBothAppsNotUserUninstalled;
  } else if (!web_app_user_uninstalled) {
    return ScanResult::kWebAppNotUserUninstalled;
  } else if (!chrome_app_user_uninstalled) {
    return ScanResult::kChromeAppNotUserUninstalled;
  }

  // Fix conditions detected: The user has no Calculator apps and has both the
  // default web app and Chrome app marked as user uninstalled. Users get into
  // this state via the Calculator app duplication bug
  // (https://crbug.com/1393284) followed by manually uninstalling the Chrome
  // app, leading to the web app uninstalling itself (the web app is migrating
  // the Chrome app and overzealously tries to mimic the Chrome app user
  // uninstalled state).

  // Don't run this fix more than once; more than once is unexpected, just
  // record that the user got into this state and exit.
  if (profile_->GetPrefs()->GetBoolean(
          kWebAppCalculatorAppErasureFixAppliedPref)) {
    return ScanResult::kFixAlreadyAppliedAndWantedToApplyAgain;
  }

  // Apply the fix. By removing the "user uninstalled" prefs for the Calculator
  // app the default Calculator web app config will allow itself to be
  // installed.
  bool web_app_pref_removed =
      web_app_prefs.RemoveByAppId(web_app::kCalculatorAppId);
  DCHECK(web_app_pref_removed);
  bool chrome_app_pref_removed = extensions::ClearExternalExtensionUninstalled(
      &*profile_, extension_misc::kCalculatorAppId);
  DCHECK(chrome_app_pref_removed);

  // Record that the fix was applied to avoid unexpectedly applying it
  // multiple times.
  profile_->GetPrefs()->SetBoolean(kWebAppCalculatorAppErasureFixAppliedPref,
                                   true);

  return ScanResult::kFixApplied;
}

}  // namespace web_app
