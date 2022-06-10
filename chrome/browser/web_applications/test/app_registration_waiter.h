// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRATION_WAITER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRATION_WAITER_H_

#include "base/run_loop.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

class Profile;

namespace web_app {

class AppRegistrationWaiter : public apps::AppRegistryCache::Observer {
 public:
  AppRegistrationWaiter(Profile* profile,
                        const AppId& app_id,
                        apps::Readiness readiness = apps::Readiness::kReady);
  ~AppRegistrationWaiter() override;

  void Await();

 private:
  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  const AppId app_id_;
  const apps::Readiness readiness_;
  base::RunLoop run_loop_;
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_APP_REGISTRATION_WAITER_H_
