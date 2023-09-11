// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include "components/services/app_service/public/cpp/app_update.h"

namespace apps {

AppStorage::AppStorage(apps::AppRegistryCache& app_registry_cache) {
  app_registry_cache_observer_.Observe(&app_registry_cache);
}

AppStorage::~AppStorage() = default;

void AppStorage::OnAppUpdate(const apps::AppUpdate& update) {
  // TODO(crbug.com/1385932): Add the implementation.
}

void AppStorage::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

}  // namespace apps
