// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_CALCULATOR_APP_ERASURE_FIXER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_CALCULATOR_APP_ERASURE_FIXER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "url/gurl.h"

class Profile;

namespace user_prefs {
class PrefRegistrySyncable;
}
namespace apps {
class AppUpdate;
}  // namespace apps

namespace web_app {

// Feature which controls whether the calculator app fix runs.
BASE_DECLARE_FEATURE(kWebAppCalculatorAppErasureFixer);

// Tracks whether the calculator app erasure fix has already been applied.
extern const char kWebAppCalculatorAppErasureFixAppliedPref[];

// Histogram recorded with a `ScanResult` value whenever
// `CalculatorAppErasureFixer` runs.
extern const char kHistogramWebAppCalculatorAppErasureScanResult[];

// This is a targeted fix for users affected by: https://crbug.com/1393284
class CalculatorAppErasureFixer : public apps::AppRegistryCache::Observer {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ScanResult {
    kNoWebAppsForProcess = 0,
    kBothAppsInstalled = 1,
    kWebAppInstalled = 2,
    kChromeAppInstalled = 3,
    kBothAppsNotUserUninstalled = 4,
    kWebAppNotUserUninstalled = 5,
    kChromeAppNotUserUninstalled = 6,
    kFixAlreadyAppliedAndWantedToApplyAgain = 7,
    kFixApplied = 8,
    kMaxValue = kFixApplied,
  };

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  explicit CalculatorAppErasureFixer(Profile& profile);
  ~CalculatorAppErasureFixer() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  void ScanForCalculatorAppErasureAndEmitMetrics();
  ScanResult ScanForCalculatorAppErasure();

  const raw_ref<Profile> profile_;

  bool web_apps_ready_ = false;
  bool chrome_apps_ready_ = false;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      scoped_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_CALCULATOR_APP_ERASURE_FIXER_H_
