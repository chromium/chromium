// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_capability_access_cache_wrapper.h"

#include "base/no_destructor.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_capability_access_cache.h"

namespace apps {

// static
AppCapabilityAccessCacheWrapper& AppCapabilityAccessCacheWrapper::Get() {
  static base::NoDestructor<AppCapabilityAccessCacheWrapper> instance;
  return *instance;
}

AppCapabilityAccessCacheWrapper::AppCapabilityAccessCacheWrapper() = default;

AppCapabilityAccessCacheWrapper::~AppCapabilityAccessCacheWrapper() = default;

AppCapabilityAccessCache*
AppCapabilityAccessCacheWrapper::GetAppCapabilityAccessCache(
    const AccountId& account_id) {
  auto it = app_capability_access_caches_.find(account_id);
  if (it == app_capability_access_caches_.end()) {
    return nullptr;
  }
  return it->second;
}

void AppCapabilityAccessCacheWrapper::AddAppCapabilityAccessCache(
    const AccountId& account_id,
    AppCapabilityAccessCache* cache) {
  app_capability_access_caches_[account_id] = cache;
}

void AppCapabilityAccessCacheWrapper::RemoveAppCapabilityAccessCache(
    AppCapabilityAccessCache* cache) {
  for (auto& it : app_capability_access_caches_) {
    if (it.second == cache) {
      app_capability_access_caches_.erase(it.first);
      return;
    }
  }
}

}  // namespace apps
