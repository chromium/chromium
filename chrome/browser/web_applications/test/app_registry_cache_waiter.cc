// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/app_registry_cache_waiter.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"

namespace web_app {

AppTypeInitializationWaiter::AppTypeInitializationWaiter(Profile* profile,
                                                         apps::AppType app_type)
    : app_type_(app_type) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);

  if (cache.IsAppTypeInitialized(app_type))
    run_loop_.Quit();
}

AppTypeInitializationWaiter::~AppTypeInitializationWaiter() = default;

void AppTypeInitializationWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void AppTypeInitializationWaiter::OnAppUpdate(const apps::AppUpdate& update) {}

void AppTypeInitializationWaiter::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == app_type_)
    run_loop_.Quit();
}

void AppTypeInitializationWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

AppReadinessWaiter::AppReadinessWaiter(
    Profile* profile,
    const std::string& app_id,
    base::RepeatingCallback<bool(apps::Readiness)> readiness_predicate)
    : app_id_(app_id), readiness_predicate_(std::move(readiness_predicate)) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);
  cache.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    if (readiness_predicate_.Run(update.Readiness())) {
      run_loop_.Quit();
    }
  });
}

AppReadinessWaiter::AppReadinessWaiter(Profile* profile,
                                       const std::string& app_id,
                                       apps::Readiness readiness)
    : AppReadinessWaiter(profile,
                         app_id,
                         base::BindRepeating(
                             [](apps::Readiness expected_readiness,
                                apps::Readiness readiness) {
                               return readiness == expected_readiness;
                             },
                             readiness)) {}

AppReadinessWaiter::~AppReadinessWaiter() = default;

void AppReadinessWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void AppReadinessWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_ &&
      readiness_predicate_.Run(update.Readiness())) {
    run_loop_.Quit();
  }
}
void AppReadinessWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

WebAppScopeWaiter::WebAppScopeWaiter(Profile* profile,
                                     const std::string& app_id,
                                     GURL scope)
    : app_id_(app_id), scope_(std::move(scope)) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);
  cache.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    if (ContainsExpectedIntentFilter(update))
      run_loop_.Quit();
  });
}

WebAppScopeWaiter::~WebAppScopeWaiter() = default;

void WebAppScopeWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void WebAppScopeWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_ && ContainsExpectedIntentFilter(update)) {
    run_loop_.Quit();
  }
}

void WebAppScopeWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

bool WebAppScopeWaiter::ContainsExpectedIntentFilter(
    const apps::AppUpdate& update) const {
  apps::IntentFilterPtr expected =
      apps_util::MakeIntentFilterForUrlScope(scope_);
  for (auto& intent_filter : update.IntentFilters()) {
    DCHECK(!intent_filter->IsBrowserFilter());
    if (*intent_filter == *expected)
      return true;
  }
  return false;
}

AppWindowModeWaiter::AppWindowModeWaiter(Profile* profile,
                                         const std::string& app_id,
                                         apps::WindowMode window_mode)
    : app_id_(app_id), window_mode_(window_mode) {
  DCHECK_NE(window_mode_, apps::WindowMode::kUnknown);
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);
  cache.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    if (HasExpectedWindowMode(update))
      run_loop_.Quit();
  });
}

AppWindowModeWaiter::~AppWindowModeWaiter() = default;

void AppWindowModeWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void AppWindowModeWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_ && HasExpectedWindowMode(update)) {
    run_loop_.Quit();
  }
}

void AppWindowModeWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

bool AppWindowModeWaiter::HasExpectedWindowMode(
    const apps::AppUpdate& update) const {
  return update.WindowMode() == window_mode_;
}

}  // namespace web_app
