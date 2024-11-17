// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_WRAPPER_H_
#define COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_WRAPPER_H_

#include <map>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"

class AccountId;

namespace apps {

class AppCapabilityAccessCache;

// Wraps AppCapabilityAccessCache to get all AppCapabilityAccessCaches
// independently. Provides the method to get the AppCapabilityAccessCache per
// |account_id|.
class COMPONENT_EXPORT(APP_UPDATE) AppCapabilityAccessCacheWrapper {
 public:
  // Returns the global AppCapabilityAccessCacheWrapper object.
  static AppCapabilityAccessCacheWrapper& Get();

  AppCapabilityAccessCacheWrapper();
  ~AppCapabilityAccessCacheWrapper();

  AppCapabilityAccessCacheWrapper(const AppCapabilityAccessCacheWrapper&) =
      delete;
  AppCapabilityAccessCacheWrapper& operator=(
      const AppCapabilityAccessCacheWrapper&) = delete;

  // Returns AppCapabilityAccessCache for the given |account_id|, or return null
  // if AppCapabilityAccessCache doesn't exist.
  AppCapabilityAccessCache* GetAppCapabilityAccessCache(
      const AccountId& account_id);

  // Adds the AppCapabilityAccessCache for the given |account_id|.
  void AddAppCapabilityAccessCache(const AccountId& account_id,
                                   AppCapabilityAccessCache* cache);

  // Removes the |cache| in |app_capability_access_caches_|.
  void RemoveAppCapabilityAccessCache(AppCapabilityAccessCache* cache);

 private:
  std::map<AccountId, raw_ptr<AppCapabilityAccessCache, CtnExperimental>>
      app_capability_access_caches_;
};

}  // namespace apps

#endif  // COMPONENTS_SERVICES_APP_SERVICE_PUBLIC_CPP_APP_CAPABILITY_ACCESS_CACHE_WRAPPER_H_
