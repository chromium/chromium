// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"

#include <vector>

#include "base/i18n/case_conversion.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash::phonehub {
namespace {
void SortStreamableAppsList(
    std::vector<Notification::AppMetadata>& streamable_apps) {
  std::sort(
      streamable_apps.begin(), streamable_apps.end(),
      [](const Notification::AppMetadata& a,
         const Notification::AppMetadata& b) {
        std::u16string a_app_name = base::i18n::ToLower(a.visible_app_name);
        std::u16string b_app_name = base::i18n::ToLower(b.visible_app_name);
        return a_app_name < b_app_name;
      });
}
}  // namespace

AppStreamLauncherDataModel::AppStreamLauncherDataModel() = default;

AppStreamLauncherDataModel::~AppStreamLauncherDataModel() = default;

void AppStreamLauncherDataModel::Observer::OnShouldShowMiniLauncherChanged() {}
void AppStreamLauncherDataModel::Observer::OnAppListChanged() {}

void AppStreamLauncherDataModel::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void AppStreamLauncherDataModel::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void AppStreamLauncherDataModel::SetShouldShowMiniLauncher(
    bool should_show_mini_launcher) {
  should_show_app_stream_launcher_ = should_show_mini_launcher;
  for (auto& observer : observer_list_)
    observer.OnShouldShowMiniLauncherChanged();
}

bool AppStreamLauncherDataModel::GetShouldShowMiniLauncher() {
  return should_show_app_stream_launcher_;
}

void AppStreamLauncherDataModel::SetLauncherSize(int height, int width) {
  launcher_height_ = height;
  launcher_width_ = width;
}

void AppStreamLauncherDataModel::ResetState() {
  should_show_app_stream_launcher_ = false;
}

void AppStreamLauncherDataModel::SetAppList(
    const std::vector<Notification::AppMetadata>& streamable_apps) {
  PA_LOG(INFO) << "App Streaming Launcher data updated with "
               << streamable_apps.size() << " apps";
  apps_list_ = streamable_apps;

  apps_list_sorted_by_name_ = streamable_apps;

  // Alphabetically sort the app list.
  SortStreamableAppsList(apps_list_sorted_by_name_);

  for (auto& observer : observer_list_)
    observer.OnAppListChanged();
}

const std::vector<Notification::AppMetadata>*
AppStreamLauncherDataModel::GetAppsList() {
  return &apps_list_;
}

const std::vector<Notification::AppMetadata>*
AppStreamLauncherDataModel::GetAppsListSortedByName() {
  return &apps_list_sorted_by_name_;
}

void AppStreamLauncherDataModel::AddAppToList(
    const Notification::AppMetadata& app) {
  apps_list_.emplace_back(app);
  // Alphabetically sort the app list.
  apps_list_sorted_by_name_.emplace_back(app);
  SortStreamableAppsList(apps_list_sorted_by_name_);

  for (auto& observer : observer_list_) {
    observer.OnAppListChanged();
  }
}

void AppStreamLauncherDataModel::RemoveAppFromList(
    const proto::App app_to_remove) {
  std::erase_if(apps_list_,
                [&app_to_remove](const Notification::AppMetadata& app) {
                  return app.package_name == app_to_remove.package_name();
                });

  std::erase_if(apps_list_sorted_by_name_,
                [&app_to_remove](const Notification::AppMetadata& app) {
                  return app.package_name == app_to_remove.package_name();
                });

  for (auto& observer : observer_list_) {
    observer.OnAppListChanged();
  }
}
}  // namespace ash::phonehub
