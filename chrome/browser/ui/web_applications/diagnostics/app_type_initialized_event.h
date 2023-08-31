// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_APP_TYPE_INITIALIZED_EVENT_H_
#define CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_APP_TYPE_INITIALIZED_EVENT_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

class Profile;

namespace web_app {

// Helper class to wait for |app_type| to be initialized with the app service.
class AppTypeInitializedEvent : public apps::AppRegistryCache::Observer {
 public:
  explicit AppTypeInitializedEvent(Profile* profile, apps::AppType app_type);
  ~AppTypeInitializedEvent() override;

  // Returns false if the app service is not enabled for |profile|.
  // |callback| will be asynchronously called if |app_type| is already
  // initialized otherwise it will be called after initialization has happened.
  bool Post(base::OnceClosure callback);

  // apps::AppRegistryCache::Observer:
  void OnAppTypeInitialized(apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

 private:
  raw_ptr<Profile> profile_ = nullptr;
  apps::AppType app_type_;

  base::OnceClosure callback_;
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      scoped_observation_{this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_UI_WEB_APPLICATIONS_DIAGNOSTICS_APP_TYPE_INITIALIZED_EVENT_H_
