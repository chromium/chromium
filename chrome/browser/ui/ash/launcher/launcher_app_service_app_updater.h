// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_SERVICE_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_SERVICE_APP_UPDATER_H_

#include <set>
#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/launcher_app_updater.h"
#include "chrome/services/app_service/public/cpp/app_registry_cache.h"

namespace apps {
class AppUpdate;
}  // namespace apps

// LauncherAppServiceAppUpdater handles life cycle events for AppService Apps.
class LauncherAppServiceAppUpdater : public LauncherAppUpdater,
                                     public apps::AppRegistryCache::Observer {
 public:
  LauncherAppServiceAppUpdater(Delegate* delegate,
                               content::BrowserContext* browser_context);
  ~LauncherAppServiceAppUpdater() override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  std::set<std::string> installed_apps_;

  DISALLOW_COPY_AND_ASSIGN(LauncherAppServiceAppUpdater);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_LAUNCHER_APP_SERVICE_APP_UPDATER_H_
