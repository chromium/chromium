// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace base {
class FilePath;
}

namespace apps {

class AppStorageFileHandler;

// AppStorage is responsible for reading and writing the app information on
// disk.
class COMPONENT_EXPORT(APP_UPDATE) AppStorage
    : public apps::AppRegistryCache::Observer {
 public:
  explicit AppStorage(const base::FilePath& base_path,
                      apps::AppRegistryCache& app_registry_cache);

  AppStorage(const AppStorage&) = delete;
  AppStorage& operator=(const AppStorage&) = delete;

  ~AppStorage() override;

 private:
  // apps::AppRegistryCache::Observer overrides:
  void OnAppUpdate(const apps::AppUpdate& update) override;
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;

  // Invoked when reading the app info data from the AppStorage file is
  // finished.
  void OnGetAppInfoData(std::vector<AppPtr> apps);

  raw_ref<apps::AppRegistryCache> app_registry_cache_;

  scoped_refptr<AppStorageFileHandler> file_handler_;

  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<AppStorage> weak_factory_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_
