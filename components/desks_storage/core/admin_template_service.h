// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_
#define COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "components/desks_storage/core/desk_model_observer.h"
#include "components/desks_storage/core/local_desk_data_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace desks_storage {
class DeskModel;
class AdminTemplateModel;

// Service that provides AdminTemplateModelInstances
class AdminTemplateService : public KeyedService,
                             public desks_storage::DeskModelObserver,
                             public apps::AppRegistryCache::Observer,
                             public apps::AppRegistryCacheWrapper::Observer {
 public:
  // Standard constructor used in instances where we dont want to introduce
  // creates the sub-directory "app_launch_automation/" in the users' data
  // directory.
  AdminTemplateService(const base::FilePath& user_data_dir_path,
                       const AccountId& account_id,
                       PrefService* pref_service_);
  AdminTemplateService(const AdminTemplateService&) = delete;
  AdminTemplateService& operator=(const AdminTemplateService&) = delete;
  ~AdminTemplateService() override;

  // Returns the intended admin model. This method can return nullptr.
  virtual AdminTemplateModel* GetAdminModel();

  // Returns a full desk model, this should only be used by tests and the
  // storage backend when receiving a policy.  This method can return nullptr.
  virtual DeskModel* GetFullDeskModel();

  // Lets the caller know if the underlying storage backend is ready to be used.
  bool IsReady();

  // DeskModelObserver
  void DeskModelLoaded() override;

  // AppRegistryCache::Observer
  void OnAppRegistryCacheWillBeDestroyed(
      apps::AppRegistryCache* cache) override;
  void OnAppTypeInitialized(apps::AppType app_type) override;

  // AppRegistryCacheWrapper::Observer
  void OnAppRegistryCacheAdded(const AccountId& account_id) override;

  // Logic for updating the desk model. Reads from `pref_service_` and updates
  // the model with the contents within.
  void UpdateModelWithPolicy();

 private:
  bool WillAppRegistryCacheResolveAppIds();

  // Account ID used for assigning apps_cache to this service.
  AccountId account_id_;

  // Storage backend.
  std::unique_ptr<LocalDeskDataManager> data_manager_;

  // Pref service used to monitor preference updates when new policies are
  // uploaded to the user.
  raw_ptr<PrefService> pref_service_ = nullptr;

  // apps_cache pointer used to verify readiness before model updates.
  raw_ptr<apps::AppRegistryCache> apps_cache_ = nullptr;

  // Preference Change Registrar updates the storage backend when a new policy
  // has been downloaded.
  PrefChangeRegistrar pref_change_registrar_;

  // scoped Observations
  base::ScopedObservation<LocalDeskDataManager,
                          desks_storage::DeskModelObserver>
      model_obs_{this};
  base::ScopedObservation<apps::AppRegistryCache,
                          apps::AppRegistryCache::Observer>
      app_cache_obs_{this};

  base::ScopedObservation<apps::AppRegistryCacheWrapper,
                          apps::AppRegistryCacheWrapper::Observer>
      app_cache_wrapper_obs_{this};

  // Tells us whether or not the apps cache is ready.
  bool is_cache_ready_ = false;

  // Weak ptr for using `this` on scheduled tasks.
  base::WeakPtrFactory<AdminTemplateService> weak_ptr_factory_{this};
};

}  // namespace desks_storage

#endif  // COMPONENTS_DESKS_STORAGE_CORE_ADMIN_TEMPLATE_SERVICE_H_
