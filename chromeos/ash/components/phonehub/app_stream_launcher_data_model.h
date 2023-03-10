// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_LAUNCHER_DATA_MODEL_H_
#define CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_LAUNCHER_DATA_MODEL_H_

#include <memory>

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/ash/components/phonehub/notification.h"
#include "chromeos/ash/components/phonehub/proto/phonehub_api.pb.h"

namespace ash::phonehub {

// Keeps that data that is associated with the App Stream Mini Launcher.
class AppStreamLauncherDataModel {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnShouldShowMiniLauncherChanged();
    virtual void OnAppListChanged();
  };

  AppStreamLauncherDataModel();
  AppStreamLauncherDataModel(const AppStreamLauncherDataModel&) = delete;
  AppStreamLauncherDataModel& operator=(const AppStreamLauncherDataModel&) =
      delete;
  ~AppStreamLauncherDataModel();

  // Whether the App Stream Mini Launcher should be shown.
  // It only affects the UI when the state is "phone connected" and Eche
  // is turned on. As soon as the ui state becomes any other states other than
  // `kMiniLauncher` this will reset. So the next time the UI is shown
  // it goes back to the main phone connected view.
  void SetShouldShowMiniLauncher(bool should_show_mini_launcher);
  bool GetShouldShowMiniLauncher();

  int launcher_height() { return launcher_height_; }
  int launcher_width() { return launcher_width_; }
  void SetLauncherSize(int height, int width);

  // Resets the internal state w/o updating the UI.
  void ResetState();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void SetAppList(
      const std::vector<Notification::AppMetadata>& streamable_apps);
  const std::vector<Notification::AppMetadata>* GetAppsList();
  const std::vector<Notification::AppMetadata>* GetAppsListSortedByName();
  void AddAppToList(const Notification::AppMetadata& app);
  void RemoveAppFromList(const proto::App app);

 private:
  // Indicates if the Mini Launcher should be shown when the status is
  // "phone connected" or not.
  bool should_show_app_stream_launcher_ = false;
  int launcher_height_ = 0;
  int launcher_width_ = 0;
  std::vector<Notification::AppMetadata> apps_list_;
  std::vector<Notification::AppMetadata> apps_list_sorted_by_name_;
  base::ObserverList<Observer> observer_list_;
};

}  // namespace ash::phonehub

#endif  // CHROMEOS_ASH_COMPONENTS_PHONEHUB_APP_STREAM_LAUNCHER_DATA_MODEL_H_
