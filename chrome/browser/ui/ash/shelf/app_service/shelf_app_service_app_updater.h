// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_APP_UPDATER_H_

#include <set>
#include <string>

#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/shelf/shelf_app_updater.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace apps {
class AppUpdate;
}  // namespace apps

// ShelfAppServiceAppUpdater handles life cycle events for AppService Apps.
class ShelfAppServiceAppUpdater : public ShelfAppUpdater,
                                  public apps::AppRegistryCache::Observer {
 public:
  ShelfAppServiceAppUpdater(Delegate* delegate,
                            content::BrowserContext* browser_context);

  ShelfAppServiceAppUpdater(const ShelfAppServiceAppUpdater&) = delete;
  ShelfAppServiceAppUpdater& operator=(const ShelfAppServiceAppUpdater&) =
      delete;

  ~ShelfAppServiceAppUpdater() override;

  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  void OnShowInShelfChangedForAppDisabledByPolicy(const std::string& app_id,
                                                  bool show_in_shelf);
  std::set<std::string> installed_apps_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_APP_UPDATER_H_
