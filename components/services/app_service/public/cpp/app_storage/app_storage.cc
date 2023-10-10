// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include <map>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace apps {

AppStorage::AppStorage(const base::FilePath& base_path,
                       apps::AppRegistryCache& app_registry_cache)
    : app_registry_cache_(app_registry_cache) {
  file_handler_ = base::MakeRefCounted<AppStorageFileHandler>(base_path);
  app_registry_cache_observer_.Observe(&app_registry_cache);

  // Set `io_in_progress_` as true, because we will read the app info data from
  // the AppStorage file during the initialize stage. This can block the writing
  // process and writing won't start until the reading is done to prevent
  // overwriting the existing AppStorage file.
  io_in_progress_ = true;

  // Read the app info from the AppStorage file.
  file_handler_->owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&AppStorageFileHandler::ReadFromFile, file_handler_.get()),
      base::BindOnce(&AppStorage::OnGetAppInfoData,
                     weak_factory_.GetWeakPtr()));
}

AppStorage::~AppStorage() = default;

void AppStorage::OnAppUpdate(const apps::AppUpdate& update) {
  // If OnApps is in progress for the apps saved in the AppStorage file,
  // we can skip these updates, because the app info has been saved in the
  // AppStorage file.
  if (onapps_in_progress_ || !IsAppChanged(update)) {
    return;
  }

  // There are some changes on the app info, so set `should_save_app_info_`
  // as true, to write the change to the AppStorage file.
  should_save_app_info_ = true;

  MaybeSaveAppInfo();
}

void AppStorage::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppStorage::OnGetAppInfoData(std::vector<AppPtr> apps) {
  onapps_in_progress_ = true;

  app_registry_cache_->OnApps(std::move(apps), AppType::kUnknown,
                              /*should_notify_initialized=*/false);

  onapps_in_progress_ = false;

  // As the reading process is done, set io_in_progress_` as false, and call
  // MaybeSaveAppInfo to write the pending app info to the AppStorage file if
  // there are some app info changes.
  io_in_progress_ = false;
  MaybeSaveAppInfo();

  // TODO(crbug.com/1385932): Get the app types to set initialized for app
  // types.
}

bool AppStorage::IsAppChanged(const apps::AppUpdate& update) {
  if (!update.Delta()) {
    return false;
  }

  auto app = update.Delta()->Clone();
  CHECK(app);

  std::map<std::string, AppPtr>& saved_app_info = app_registry_cache_->states_;
  auto it = saved_app_info.find(app->app_id);

  // If the app is removed, check whether the app status in `saved_app_info`.
  if (app->readiness != Readiness::kUnknown &&
      !apps_util::IsInstalled(app->readiness)) {
    // If the app is installed, return true. Otherwise, return false.
    return it != saved_app_info.end() &&
           apps_util::IsInstalled(it->second->readiness);
  }

  // If the app doesn't exist in `saved_app_info`, return true;
  if (it == saved_app_info.end()) {
    return true;
  }

  // Ideally this should not happen quite often. However, if due to new
  // feature/requirements, etc, the app type could be modified, e.g.
  // kChromeApp->kStandaloneBrowserChromeApp, so we allow the app type changed
  // to align with the latest requirements.
  if (app->app_type != it->second->app_type) {
    return true;
  }

  if (app->readiness != Readiness::kUnknown &&
      app->readiness != it->second->readiness) {
    return true;
  }

  if (app->name.has_value() &&
      (!it->second->name.has_value() ||
       app->name.value() != it->second->name.value())) {
    return true;
  }

  // TODO(crbug.com/1385932): Add other files in the App structure.
  return false;
}

void AppStorage::MaybeSaveAppInfo() {
  // If the writing is in progress, we need to wait the current writing process
  // to finish, then start the new writing.
  if (io_in_progress_) {
    return;
  }

  // If there is no change, we don't need to write the file.
  if (!should_save_app_info_) {
    return;
  }

  // Get the latest app info to write to the file.
  std::vector<AppPtr> apps;
  app_registry_cache_->ForEachApp([&apps](const AppUpdate& update) {
    if (!apps_util::IsInstalled(update.Readiness())) {
      return;
    }

    if (!update.State()) {
      apps.push_back(update.Delta()->Clone());
      return;
    }

    auto app = update.State()->Clone();
    AppUpdate::Merge(app.get(), update.Delta());
    apps.push_back(std::move(app));
  });

  // Set `io_in_progress_` as true, to block other writing.
  io_in_progress_ = true;

  // Write the app info to the AppStorage file.
  file_handler_->owning_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AppStorageFileHandler::WriteToFile, file_handler_.get(),
                     std::move(apps)),
      base::BindOnce(&AppStorage::OnSaveFinished, weak_factory_.GetWeakPtr()));

  should_save_app_info_ = false;
}

void AppStorage::OnSaveFinished() {
  io_in_progress_ = false;

  // Call MaybeSaveAppInfo to check whether there are more app info data to be
  // written. If yes, start a new writing process to write the app info to the
  // AppStorage file.
  MaybeSaveAppInfo();
}

}  // namespace apps
