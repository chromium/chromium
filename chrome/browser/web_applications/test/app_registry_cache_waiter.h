// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_

#include <string>

#include "base/run_loop.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

class Profile;

namespace web_app {

class AppTypeInitializationWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppTypeInitializationWaiter(Profile* profile, apps::AppType app_type);
  ~AppTypeInitializationWaiter() override;

  void Await();

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const apps::AppType app_type_;
  base::RunLoop run_loop_;
};

class AppReadinessWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppReadinessWaiter(Profile* profile,
                     const std::string& app_id,
                     apps::Readiness readiness = apps::Readiness::kReady);
  ~AppReadinessWaiter() override;

  void Await();

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const std::string app_id_;
  const apps::Readiness readiness_;
  base::RunLoop run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_
