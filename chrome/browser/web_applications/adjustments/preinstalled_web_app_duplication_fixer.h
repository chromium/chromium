// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_PREINSTALLED_WEB_APP_DUPLICATION_FIXER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_PREINSTALLED_WEB_APP_DUPLICATION_FIXER_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "url/gurl.h"

class Profile;

namespace apps {
class AppUpdate;
namespace mojom {
enum class AppType;
}
}  // namespace apps

namespace web_app {

class PreinstalledWebAppDuplicationFixer
    : public apps::AppRegistryCache::Observer {
 public:
  static const char kHistogramWebAppAbsentChromeAppAbsent[];
  static const char kHistogramWebAppAbsentChromeAppPresent[];
  static const char kHistogramWebAppPresentChromeAppAbsent[];
  static const char kHistogramWebAppPresentChromeAppPresent[];
  static const char kHistogramAppDuplicationFixApplied[];

  static void SkipStartupForTesting();

  explicit PreinstalledWebAppDuplicationFixer(Profile& profile);
  ~PreinstalledWebAppDuplicationFixer() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  void ScanForDuplicationForTesting();

 private:
  void ObserveAppRegistryCache();

  void ScanForDuplication();

  bool RemoveInstallUrlForPreinstalledApp(GURL url);

  const raw_ref<Profile> profile_;

  bool web_apps_ready_ = false;
  bool chrome_apps_ready_ = false;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      scoped_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_PREINSTALLED_WEB_APP_DUPLICATION_FIXER_H_
