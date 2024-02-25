// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_test_util.h"

#include "components/app_constants/constants.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace desks_storage {

namespace desk_test_util {

apps::AppPtr MakeApp(const char* app_id,
                     const char* name,
                     apps::AppType app_type) {
  apps::AppPtr app = std::make_unique<apps::App>(app_type, app_id);
  app->readiness = apps::Readiness::kReady;
  app->name = name;
  return app;
}

void PopulateAppRegistryCache(AccountId account_id,
                              apps::AppRegistryCache* cache) {
  std::vector<apps::AppPtr> deltas;

  deltas.push_back(
      MakeApp(kTestPwaAppId, "Test PWA App", apps::AppType::kChromeApp));
  // chromeAppId returns kExtension in the real Apps cache.
  deltas.push_back(MakeApp(app_constants::kChromeAppId, "Ash Chrome Browser",
                           apps::AppType::kChromeApp));
  deltas.push_back(MakeApp(app_constants::kLacrosAppId, "Lacros Chrome Browser",
                           apps::AppType::kStandaloneBrowser));
  deltas.push_back(
      MakeApp(kTestChromeAppId, "Test Chrome App", apps::AppType::kChromeApp));
  deltas.push_back(MakeApp(kTestArcAppId, "Arc app", apps::AppType::kArc));
  deltas.push_back(
      MakeApp(kTestPwaAppId1, "Test PWA App 2", apps::AppType::kChromeApp));
  deltas.push_back(MakeApp(kTestChromeAppId1, "Test Chrome App 2",
                           apps::AppType::kChromeApp));
  deltas.push_back(
      MakeApp(kTestSwaAppId, "Test System Web App 1", apps::AppType::kWeb));
  deltas.push_back(MakeApp(kTestUnsupportedAppId, "Test Supported App 1",
                           apps::AppType::kPluginVm));
  deltas.push_back(MakeApp(kTestLacrosChromeAppId, "Test Chrome App",
                           apps::AppType::kStandaloneBrowserChromeApp));

  cache->OnAppsForTesting(std::move(deltas), apps::AppType::kUnknown,
                          /*should_notify_initialized=*/false);

  cache->SetAccountId(account_id);

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id, cache);
}

void PopulateAdminTestAppRegistryCache(AccountId account_id,
                                       apps::AppRegistryCache* cache) {
  std::vector<apps::AppPtr> ash_delta;

  ash_delta.push_back(MakeApp(app_constants::kChromeAppId, "Ash Chrome Browser",
                              apps::AppType::kChromeApp));
  cache->OnAppsForTesting(std::move(ash_delta), apps::AppType::kChromeApp,
                          /*should_notify_initialized=*/true);

  std::vector<apps::AppPtr> lacros_delta;

  lacros_delta.push_back(MakeApp(app_constants::kLacrosAppId,
                                 "Lacros Chrome Browser",
                                 apps::AppType::kStandaloneBrowser));
  cache->OnAppsForTesting(std::move(lacros_delta),
                          apps::AppType::kStandaloneBrowser,
                          /*should_notify_initialized=*/true);

  cache->SetAccountId(account_id);
}

void PopulateFloatingWorkspaceAppRegistryCache(AccountId account_id,
                                               apps::AppRegistryCache* cache) {
  PopulateAdminTestAppRegistryCache(account_id, cache);
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(
      MakeApp(kTestSwaAppId, "Test System Web App 1", apps::AppType::kWeb));
  cache->OnAppsForTesting(std::move(deltas), apps::AppType::kWeb,
                          /*should_notify_initialized=*/true);
  cache->SetAccountId(account_id);
}

void AddAppIdToAppRegistryCache(AccountId account_id,
                                apps::AppRegistryCache* cache,
                                const char* app_id) {
  std::vector<apps::AppPtr> deltas;

  // We need to add the app as any type that's not a `apps::AppType::kChromeApp`
  // since there's a default hard coded string for that type, which will merge
  // all app_id to it.
  deltas.push_back(MakeApp(app_id, "Arc app", apps::AppType::kArc));

  cache->OnAppsForTesting(std::move(deltas), apps::AppType::kUnknown,
                          /*should_notify_initialized=*/false);

  cache->SetAccountId(account_id);

  apps::AppRegistryCacheWrapper::Get().AddAppRegistryCache(account_id, cache);
}

}  // namespace desk_test_util

}  // namespace desks_storage
