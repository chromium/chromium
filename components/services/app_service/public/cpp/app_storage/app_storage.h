// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_types.h"

namespace base {
class FilePath;
}

namespace apps {

class FakeAppStorage;

using AppInfo = AppStorageFileHandler::AppInfo;

// AppStorage is responsible for reading and writing the app information on
// disk.
class COMPONENT_EXPORT(APP_UPDATE) AppStorage
    : public AppRegistryCache::Observer {
 public:
  explicit AppStorage(const base::FilePath& base_path,
                      AppRegistryCache& app_registry_cache,
                      base::OnceCallback<void()> on_get_app_info_callback);

  AppStorage(const AppStorage&) = delete;
  AppStorage& operator=(const AppStorage&) = delete;

  ~AppStorage() override;

 private:
  friend class FakeAppStorage;

  // AppRegistryCache::Observer overrides:
  void OnAppUpdate(const AppUpdate& update) override;
  void OnAppTypePublishing(const std::vector<AppPtr>& deltas,
                           apps::AppType app_type) override;
  void OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) override;

  // Invoked when reading the app info data from the AppStorage file is
  // finished.
  virtual void OnGetAppInfoData(base::OnceCallback<void()> callback,
                                std::unique_ptr<AppInfo> app_info);

  // Returns true if the app info is changed compared with the app info saved in
  // the AppStorage file.
  bool IsAppChanged(const AppUpdate& update);

  // Writes the app info to the AppStorage file if there is no reading or
  // writing in progress, and there are some app info changes.
  void MaybeSaveAppInfo();

  // Invoked when writing to the file operation is finished.
  virtual void OnSaveFinished();

  raw_ref<AppRegistryCache> app_registry_cache_;

  scoped_refptr<AppStorageFileHandler> file_handler_;

  // True if there are some app info changes haven't been written to the
  // AppStorage file during the AppStorage file writing time. Once the
  // file writing is done, start a new writing process to re-write the new app
  // info to the AppStorage file.
  bool should_save_app_info_ = false;

  // Records whether there is any reading or writing in progress.
  bool io_in_progress_;

  // Records OnApps is in progress for the apps saved in the AppStorage file.
  // The OnAppUpdate callback should skip the update when onapps_in_progress_ is
  // true, as those updates have been written in the AppStorage file.
  bool onapps_in_progress_ = false;

  base::ScopedObservation<AppRegistryCache, AppRegistryCache::Observer>
      app_registry_cache_observer_{this};

  base::WeakPtrFactory<AppStorage> weak_factory_{this};
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_STORAGE_APP_STORAGE_H_
