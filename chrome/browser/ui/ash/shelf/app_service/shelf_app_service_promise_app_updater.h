// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_PROMISE_APP_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_PROMISE_APP_UPDATER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ui/ash/shelf/shelf_app_updater.h"
#include "components/services/app_service/public/cpp/package_id.h"

class Profile;

// Responsible for triggering Shelf item updates when there is a promise app
// update.
class ShelfPromiseAppUpdater : public ShelfAppUpdater,
                               public apps::PromiseAppRegistryCache::Observer {
 public:
  ShelfPromiseAppUpdater(Delegate* delegate, Profile* profile);

  ShelfPromiseAppUpdater(const ShelfPromiseAppUpdater&) = delete;
  ShelfPromiseAppUpdater& operator=(const ShelfPromiseAppUpdater&) = delete;

  ~ShelfPromiseAppUpdater() override;

  // PromiseAppRegistryCache::Observer overrides:
  void OnPromiseAppUpdate(const apps::PromiseAppUpdate& promise_app) override;
  void OnPromiseAppRemoved(const apps::PackageId& package_id) override;
  void OnPromiseAppRegistryCacheWillBeDestroyed(
      apps::PromiseAppRegistryCache* cache) override;

 private:
  base::ScopedObservation<apps::PromiseAppRegistryCache,
                          apps::PromiseAppRegistryCache::Observer>
      promise_app_registry_cache_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_PROMISE_APP_UPDATER_H_
