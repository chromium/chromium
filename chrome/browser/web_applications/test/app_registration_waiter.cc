// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/test/app_registration_waiter.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

namespace web_app {

AppRegistrationWaiter::AppRegistrationWaiter(Profile* profile,
                                             const AppId& app_id)
    : app_id_(app_id) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  Observe(&cache);
  if (cache.ForOneApp(app_id, [](const apps::AppUpdate&) {}))
    run_loop_.Quit();
}
AppRegistrationWaiter::~AppRegistrationWaiter() = default;

void AppRegistrationWaiter::Await() {
  run_loop_.Run();
}

void AppRegistrationWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_)
    run_loop_.Quit();
}
void AppRegistrationWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  Observe(nullptr);
}

}  // namespace web_app
