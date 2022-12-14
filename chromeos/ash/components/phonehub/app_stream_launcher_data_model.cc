// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/app_stream_launcher_data_model.h"

#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/notification.h"

namespace ash::phonehub {

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
  std::sort(apps_list_sorted_by_name_.begin(), apps_list_sorted_by_name_.end(),
            [](const Notification::AppMetadata& a,
               const Notification::AppMetadata& b) {
              return a.visible_app_name < b.visible_app_name;
            });
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

}  // namespace ash::phonehub
