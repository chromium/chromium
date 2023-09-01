// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_SHORTCUT_UPDATER_H_
#define CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_SHORTCUT_UPDATER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/ui/ash/shelf/shelf_app_updater.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_update.h"

class Profile;

// Responsible for triggering Shelf item updates when there is a shortcut
// update.
class ShelfAppServiceShortcutUpdater
    : public ShelfAppUpdater,
      public apps::ShortcutRegistryCache::Observer {
 public:
  ShelfAppServiceShortcutUpdater(Delegate* delegate, Profile* profile);

  ShelfAppServiceShortcutUpdater(const ShelfAppServiceShortcutUpdater&) =
      delete;
  ShelfAppServiceShortcutUpdater& operator=(
      const ShelfAppServiceShortcutUpdater&) = delete;

  ~ShelfAppServiceShortcutUpdater() override;

  // ShortcutRegistryCache::Observer overrides:
  void OnShortcutUpdated(const apps::ShortcutUpdate& update) override;
  void OnShortcutRemoved(const apps::ShortcutId& id) override;
  void OnShortcutRegistryCacheWillBeDestroyed(
      apps::ShortcutRegistryCache* cache) override;

 private:
  base::ScopedObservation<apps::ShortcutRegistryCache,
                          apps::ShortcutRegistryCache::Observer>
      shortcut_registry_cache_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ASH_SHELF_APP_SERVICE_SHELF_APP_SERVICE_SHORTCUT_UPDATER_H_
