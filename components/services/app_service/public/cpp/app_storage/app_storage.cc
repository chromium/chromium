// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/app_storage/app_storage.h"

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_runner.h"
#include "components/services/app_service/public/cpp/app.h"
#include "components/services/app_service/public/cpp/app_storage/app_storage_file_handler.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/icon_effects.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace apps {

#define IS_APP_VALUE_CHANGED(FIELD)                                \
  if (app->FIELD.has_value() && app->FIELD != it->second->FIELD) { \
    return true;                                                   \
  }

#define IS_APP_VALUE_CHANGED_FOR_ENUM(FIELD, DEFAULT_VALUE)             \
  if (app->FIELD != DEFAULT_VALUE && app->FIELD != it->second->FIELD) { \
    return true;                                                        \
  }

AppStorage::AppStorage(const base::FilePath& base_path,
                       AppRegistryCache& app_registry_cache,
                       base::OnceCallback<void()> on_get_app_info_callback)
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
      base::BindOnce(&AppStorage::OnGetAppInfoData, weak_factory_.GetWeakPtr(),
                     std::move(on_get_app_info_callback)));
}

AppStorage::~AppStorage() = default;

void AppStorage::OnAppUpdate(const AppUpdate& update) {
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

void AppStorage::OnAppTypePublishing(const std::vector<AppPtr>& deltas,
                                     apps::AppType app_type) {
  // If OnApps is in progress for the apps saved in the AppStorage file,
  // we can skip it, because `deltas` is come from the AppStorage file.
  if (onapps_in_progress_) {
    return;
  }

  std::set<std::string> app_ids;
  for (const auto& app : deltas) {
    app_ids.insert(app->app_id);
  }

  // Checks whether the apps of `app_type` in AppRegistryCache are included in
  // `deltas`. If not, uninstall the app. This is used to remove the redundant
  // apps saved in AppStorage, as the apps could be removed from other devices
  // via sync.
  std::vector<AppPtr> removed_apps;
  app_registry_cache_->ForEachApp([&](const apps::AppUpdate& update) {
    if (update.AppType() != app_type ||
        base::Contains(app_ids, update.AppId())) {
      return;
    }

    auto app = std::make_unique<App>(app_type, update.AppId());
    app->readiness = Readiness::kUninstalledByNonUser;
    removed_apps.push_back(std::move(app));
  });

  if (!removed_apps.empty()) {
    app_registry_cache_->OnApps(std::move(removed_apps), AppType::kUnknown,
                                /*should_notify_initialized=*/false);
  }
}

void AppStorage::OnAppRegistryCacheWillBeDestroyed(AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

void AppStorage::OnGetAppInfoData(base::OnceCallback<void()> callback,
                                  std::unique_ptr<AppInfo> app_info) {
  // As the reading process is done, set io_in_progress_` as false to unblock
  // the writing process.
  io_in_progress_ = false;

  if (app_info) {
    onapps_in_progress_ = true;

    app_registry_cache_->OnApps(std::move(app_info->apps));

    // Init app types.
    for (auto app_type : app_info->app_types) {
      app_registry_cache_->InitApps(app_type);
    }

    onapps_in_progress_ = false;
  }

  if (!callback.is_null()) {
    std::move(callback).Run();
  }
}

bool AppStorage::IsAppChanged(const AppUpdate& update) {
  if (!update.Delta()) {
    return false;
  }

  auto app = update.Delta()->Clone();
  CHECK(app);

  std::map<std::string, AppPtr, std::less<>>& saved_app_info =
      app_registry_cache_->states_;
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

  IS_APP_VALUE_CHANGED_FOR_ENUM(readiness, Readiness::kUnknown)

  IS_APP_VALUE_CHANGED(name);
  IS_APP_VALUE_CHANGED(short_name);
  IS_APP_VALUE_CHANGED(publisher_id);
  IS_APP_VALUE_CHANGED(installer_package_id);
  IS_APP_VALUE_CHANGED(description);
  IS_APP_VALUE_CHANGED(version);

  if (!app->additional_search_terms.empty() &&
      app->additional_search_terms != it->second->additional_search_terms) {
    return true;
  }

  if (app->icon_key.has_value()) {
    if (!it->second->icon_key.has_value()) {
      return true;
    }
    if (app->icon_key.value().resource_id !=
        it->second->icon_key.value().resource_id) {
      return true;
    }
    // Skip the kPaused icon effect, because we don't save the paused status,
    // and we wait for the family link to set the paused status and apply the
    // kPaused icon effect.
    if ((app->icon_key.value().icon_effects & (~IconEffects::kPaused)) !=
        (it->second->icon_key.value().icon_effects & (~IconEffects::kPaused))) {
      return true;
    }
  }

  IS_APP_VALUE_CHANGED(last_launch_time);
  IS_APP_VALUE_CHANGED(install_time);

  if (!app->permissions.empty() &&
      !IsEqual(app->permissions, it->second->permissions)) {
    return true;
  }

  IS_APP_VALUE_CHANGED_FOR_ENUM(install_reason, InstallReason::kUnknown)
  IS_APP_VALUE_CHANGED_FOR_ENUM(install_source, InstallSource::kUnknown)

  if (!app->policy_ids.empty() && app->policy_ids != it->second->policy_ids) {
    return true;
  }

  IS_APP_VALUE_CHANGED(is_platform_app);
  IS_APP_VALUE_CHANGED(recommendable);
  IS_APP_VALUE_CHANGED(searchable);
  IS_APP_VALUE_CHANGED(show_in_launcher);
  IS_APP_VALUE_CHANGED(show_in_shelf);
  IS_APP_VALUE_CHANGED(show_in_search);
  IS_APP_VALUE_CHANGED(show_in_management);
  IS_APP_VALUE_CHANGED(handles_intents);
  IS_APP_VALUE_CHANGED(allow_uninstall);

  if (!app->intent_filters.empty() &&
      !IsEqual(app->intent_filters, it->second->intent_filters)) {
    return true;
  }

  IS_APP_VALUE_CHANGED_FOR_ENUM(window_mode, WindowMode::kUnknown)

  IS_APP_VALUE_CHANGED(run_on_os_login)
  IS_APP_VALUE_CHANGED(allow_close)
  IS_APP_VALUE_CHANGED(app_size_in_bytes)
  IS_APP_VALUE_CHANGED(data_size_in_bytes)

  if (!app->supported_locales.empty() &&
      app->supported_locales != it->second->supported_locales) {
    return true;
  }

  IS_APP_VALUE_CHANGED(selected_locale);
  IS_APP_VALUE_CHANGED(extra);

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
