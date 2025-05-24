// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_WRAPPER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_WRAPPER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"

class AccountId;

namespace apps {

class AppRegistryCache;

// Wraps AppRegistryCache to get all AppRegistryCaches independently. Provides
// the method to get the AppRegistryCache per |account_id|.
class COMPONENT_EXPORT(APP_UPDATE) AppRegistryCacheWrapper {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnAppRegistryCacheAdded(const AccountId& account_id) {}
  };

  // Returns the global AppRegistryCacheWrapper object.
  static AppRegistryCacheWrapper& Get();

  AppRegistryCacheWrapper();
  ~AppRegistryCacheWrapper();

  AppRegistryCacheWrapper(const AppRegistryCacheWrapper&) = delete;
  AppRegistryCacheWrapper& operator=(const AppRegistryCacheWrapper&) = delete;

  // Returns AppRegistryCache for the given |account_id|, or return null if
  // AppRegistryCache doesn't exist.
  AppRegistryCache* GetAppRegistryCache(const AccountId& account_id);

  // Adds the AppRegistryCache for the given |account_id|.
  void AddAppRegistryCache(const AccountId& account_id,
                           AppRegistryCache* cache);

  // Removes the |cache| in |app_registry_caches_|.
  void RemoveAppRegistryCache(AppRegistryCache* cache);

  void AddObserver(Observer* observer);

  void RemoveObserver(Observer* observer);

 private:
  std::map<AccountId, raw_ptr<AppRegistryCache, CtnExperimental>>
      app_registry_caches_;

  base::ObserverList<Observer> observers_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_REGISTRY_CACHE_WRAPPER_H_
