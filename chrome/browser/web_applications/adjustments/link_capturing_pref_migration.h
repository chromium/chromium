// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_LINK_CAPTURING_PREF_MIGRATION_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_LINK_CAPTURING_PREF_MIGRATION_H_

#include "base/memory/raw_ref.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

class Profile;

namespace web_app {

// This class ensures web apps have their automatic link capturing user
// preference set to enabled if they were using the "capture_links" manifest
// API:
// https://github.com/WICG/web-app-launch/blob/main/declarative_link_capturing.md
//
// These web apps are exempt from the user preference opt in model we have now
// as they would have been installed prior to the opt in model coming into
// effect.
// TODO(crbug.com/1312844): Remove after 2022-09.
class LinkCapturingPrefMigration : public apps::AppRegistryCache::Observer {
 public:
  explicit LinkCapturingPrefMigration(Profile& profile);
  ~LinkCapturingPrefMigration() override;

  // apps::AppRegistryCache::Observer:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  const raw_ref<Profile> profile_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      scoped_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ADJUSTMENTS_LINK_CAPTURING_PREF_MIGRATION_H_
