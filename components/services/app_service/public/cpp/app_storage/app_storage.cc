// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace apps {

AppStorage::AppStorage(const base::FilePath& base_path,
                       apps::AppRegistryCache& app_registry_cache)
    : app_registry_cache_(app_registry_cache) {
  file_handler_ = base::MakeRefCounted<AppStorageFileHandler>(base_path);
  app_registry_cache_observer_.Observe(&app_registry_cache);

  // Read the app info from the AppStorage file.
  file_handler_->owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AppStorageFileHandler::ReadFromFile, file_handler_.get()),
      base::BindOnce(&AppStorage::OnGetAppInfoData,
                     weak_factory_.GetWeakPtr()));
}

AppStorage::~AppStorage() = default;

void AppStorage::OnAppUpdate(const apps::AppUpdate& update) {
  // TODO(crbug.com/1385932): Add the implementation.
}

void AppStorage::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppStorage::OnGetAppInfoData(std::vector<AppPtr> apps) {
  app_registry_cache_->OnApps(std::move(apps), AppType::kUnknown,
                              /*should_notify_initialized=*/false);

  // TODO(crbug.com/1385932): Get the app types to set initialized for app
  // types.
}

}  // namespace apps
