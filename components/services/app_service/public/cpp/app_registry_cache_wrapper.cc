// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"

#include "base/no_destructor.h"
#include "components/account_id/account_id.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"

namespace apps {

// static
AppRegistryCacheWrapper& AppRegistryCacheWrapper::Get() {
  static base::NoDestructor<AppRegistryCacheWrapper> instance;
  return *instance;
}

AppRegistryCacheWrapper::AppRegistryCacheWrapper() = default;

AppRegistryCacheWrapper::~AppRegistryCacheWrapper() = default;

AppRegistryCache* AppRegistryCacheWrapper::GetAppRegistryCache(
    const AccountId& account_id) {
  auto it = app_registry_caches_.find(account_id);
  if (it == app_registry_caches_.end()) {
    return nullptr;
  }
  return it->second;
}

void AppRegistryCacheWrapper::AddAppRegistryCache(const AccountId& account_id,
                                                  AppRegistryCache* cache) {
  app_registry_caches_[account_id] = cache;

  for (Observer& obs : observers_) {
    obs.OnAppRegistryCacheAdded(account_id);
  }
}

void AppRegistryCacheWrapper::RemoveAppRegistryCache(AppRegistryCache* cache) {
  for (auto& it : app_registry_caches_) {
    if (it.second == cache) {
      app_registry_caches_.erase(it.first);
      return;
    }
  }
}

void AppRegistryCacheWrapper::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void AppRegistryCacheWrapper::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace apps
