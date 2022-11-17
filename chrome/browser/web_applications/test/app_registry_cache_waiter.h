// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_

#include <string>

#include "base/location.h"
#include "base/run_loop.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "url/gurl.h"

class Profile;

namespace web_app {

class AppTypeInitializationWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppTypeInitializationWaiter(Profile* profile, apps::AppType app_type);
  ~AppTypeInitializationWaiter() override;

  void Await(const base::Location& location = base::Location::Current());

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

  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const std::string app_id_;
  const apps::Readiness readiness_;
  base::RunLoop run_loop_;
};

// Waits for the web app's scope in the App Service app cache to match the
// expected |scope|.
class WebAppScopeWaiter : public apps::AppRegistryCache::Observer {
 public:
  WebAppScopeWaiter(Profile* profile, const std::string& app_id, GURL scope);
  ~WebAppScopeWaiter() override;

  // Waits for the web app's scope in the App Service app cache to match the
  // expected scope. Returns immediately if the app already has the expected
  // scope.
  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  bool ContainsExpectedIntentFilter(const apps::AppUpdate& update) const;

  const std::string app_id_;
  const GURL scope_;
  base::RunLoop run_loop_;
};

// Waits for the app's window mode in the App Service app cache to match the
// expected |window_mode|.
class AppWindowModeWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppWindowModeWaiter(Profile* profile,
                      const std::string& app_id,
                      apps::WindowMode window_mode);
  ~AppWindowModeWaiter() override;

  // Returns immediately if the app already has the expected window mode.
  void Await(const base::Location& location = base::Location::Current());

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  bool HasExpectedWindowMode(const apps::AppUpdate& update) const;

  const std::string app_id_;
  const apps::WindowMode window_mode_;
  base::RunLoop run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRY_CACHE_WAITER_H_
