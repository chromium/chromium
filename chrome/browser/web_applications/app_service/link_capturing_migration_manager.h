// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LINK_CAPTURING_MIGRATION_MANAGER_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LINK_CAPTURING_MIGRATION_MANAGER_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace web_app {

// This class ensures web apps have their automatic link capturing user
// preference set to enabled if they were using the "capture_links" manifest
// API:
// https://github.com/WICG/sw-launch/blob/main/declarative_link_capturing.md
//
// These web apps are exempt from the user preference opt in model we have now
// as they would have been installed prior to the opt in model coming into
// effect.
class LinkCapturingMigrationManager : public apps::AppRegistryCache::Observer {
 public:
  explicit LinkCapturingMigrationManager(Profile& profile);
  ~LinkCapturingMigrationManager() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  void ObserveAppRegistryCache();

  Profile& profile_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      scoped_observation_{this};

  base::WeakPtrFactory<LinkCapturingMigrationManager> weak_factory_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_APP_SERVICE_LINK_CAPTURING_MIGRATION_MANAGER_H_
