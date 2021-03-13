// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_save_handler.h"

#include <utility>

#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/constants.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"

namespace full_restore {

namespace {

// Delay between when an update is received, and when we save it to the
// full restore file.
constexpr base::TimeDelta kSaveDelay = base::TimeDelta::FromMilliseconds(2500);

}  // namespace

FullRestoreSaveHandler* FullRestoreSaveHandler::GetInstance() {
  static base::NoDestructor<FullRestoreSaveHandler> full_restore_save_handler;
  return full_restore_save_handler.get();
}

FullRestoreSaveHandler::FullRestoreSaveHandler() {
  aura::Env::GetInstance()->AddObserver(this);
}

FullRestoreSaveHandler::~FullRestoreSaveHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void FullRestoreSaveHandler::SetPrimaryProfilePath(
    const base::FilePath& profile_path) {
  primary_profile_path_ = profile_path;
}

void FullRestoreSaveHandler::SetActiveProfilePath(
    const base::FilePath& profile_path) {
  active_profile_path_ = profile_path;
}

void FullRestoreSaveHandler::OnWindowInitialized(aura::Window* window) {
  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    task_id_to_app_window_info_[window_id].window = window;
    observed_windows_.AddObservation(window);
    FullRestoreInfo::GetInstance()->OnWindowInitialized(window);

    // If the task id has an app id, OnTaskCreated has been invoked, and the app
    // launch info has been saved, so OnAppLaunched can be called to save the
    // window info.
    if (!task_id_to_app_window_info_[window_id].app_id.empty())
      FullRestoreInfo::GetInstance()->OnAppLaunched(window);

    return;
  }

  if (!SessionID::IsValidValue(window_id))
    return;

  observed_windows_.AddObservation(window);
  FullRestoreInfo::GetInstance()->OnWindowInitialized(window);

  std::string* app_id_str = window->GetProperty(::full_restore::kAppIdKey);
  AppLaunchInfoPtr app_launch_info;

  if (app_id_str) {
    // For Chrome apps, launched via event, get the app id from the window's
    // property, then find the app launch info from
    // |app_id_to_app_launch_infos_| to save the app launch info for |app_id|
    // and |window_id|.
    auto it = app_id_to_app_launch_infos_.find(*app_id_str);
    if (it == app_id_to_app_launch_infos_.end())
      return;

    auto launch_it = it->second.find(active_profile_path_);
    if (launch_it == it->second.end())
      return;

    DCHECK(!launch_it->second.empty());
    app_launch_info = std::move(*launch_it->second.begin());
    app_launch_info->window_id = window_id;
    it->second.erase(active_profile_path_);
    if (it->second.empty())
      app_id_to_app_launch_infos_.erase(it);
  } else {
    app_launch_info = std::make_unique<AppLaunchInfo>(
        extension_misc::kChromeAppId, window_id);
  }

  AddAppLaunchInfo(active_profile_path_, std::move(app_launch_info));

  FullRestoreInfo::GetInstance()->OnAppLaunched(window);
}

void FullRestoreSaveHandler::OnWindowDestroyed(aura::Window* window) {
  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    auto it = task_id_to_app_window_info_.find(window_id);
    if (it != task_id_to_app_window_info_.end()) {
      // The window could be recreated, so only remove the window info. When the
      // task is destroyed, the full restore data can be removed.
      it->second.window = nullptr;
      RemoveWindowInfo(it->second.app_id, window_id);
    }
    return;
  }

  DCHECK(SessionID::IsValidValue(window_id));

  RemoveAppRestoreData(window_id);
}

void FullRestoreSaveHandler::SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    AppLaunchInfoPtr app_launch_info) {
  if (profile_path.empty() || !app_launch_info)
    return;

  const std::string app_id = app_launch_info->app_id;

  if (!app_launch_info->window_id.has_value()) {
    // For ARC apps and Chrome apps, save |app_launch_info| to
    // |app_id_to_app_launch_infos_|, and wait for the task id or window
    // initialized to get the window id.
    app_id_to_app_launch_infos_[app_id][profile_path].emplace_back(
        std::move(app_launch_info));
    return;
  }

  const int window_id = app_launch_info->window_id.value();
  std::unique_ptr<WindowInfo> window_info;

  if (app_id != extension_misc::kChromeAppId) {
    // For browser windows, it could have been saved as
    // extension_misc::kChromeAppId in OnWindowInitialized. However, for the
    // system web apps, we need to save as the system web app app id and the
    // launch parameter, because system web apps can't be restored by the
    // browser session restore. So remove the record in
    // extension_misc::kChromeAppId, save the launch info as the system web
    // app id, and move window info to the record of the system web app id.
    auto it = window_id_to_app_restore_info_.find(window_id);
    if (it != window_id_to_app_restore_info_.end()) {
      window_info =
          profile_path_to_restore_data_[it->second.first].GetWindowInfo(
              it->second.second, window_id);
      RemoveAppRestoreData(window_id);
    }
  }

  AddAppLaunchInfo(profile_path, std::move(app_launch_info));

  if (window_info) {
    profile_path_to_restore_data_[profile_path].ModifyWindowInfo(
        app_id, window_id, *window_info);
  }
}

void FullRestoreSaveHandler::SaveWindowInfo(const WindowInfo& window_info) {
  if (!window_info.window)
    return;

  int32_t window_id =
      window_info.window->GetProperty(::full_restore::kWindowIdKey);

  if (window_info.window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    ModifyWindowInfo(window_id, window_info);
    return;
  }

  if (!SessionID::IsValidValue(window_id))
    return;

  ModifyWindowInfo(window_id, window_info);
}

void FullRestoreSaveHandler::OnTaskCreated(const std::string& app_id,
                                           int32_t task_id,
                                           int32_t session_id) {
  task_id_to_app_window_info_[task_id].app_id = app_id;

  auto it = app_id_to_app_launch_infos_.find(app_id);
  if (it == app_id_to_app_launch_infos_.end())
    return;

  auto launch_it = it->second.find(primary_profile_path_);
  if (launch_it == it->second.end() || launch_it->second.empty())
    return;

  auto app_launch_info = std::move(*launch_it->second.begin());
  app_launch_info->window_id = task_id;
  it->second.erase(primary_profile_path_);
  if (it->second.empty())
    app_id_to_app_launch_infos_.erase(it);

  AddAppLaunchInfo(primary_profile_path_, std::move(app_launch_info));

  // If the window has been created, OnAppLaunched can be called to save the
  // window info.
  if (task_id_to_app_window_info_[task_id].window) {
    FullRestoreInfo::GetInstance()->OnAppLaunched(
        task_id_to_app_window_info_[task_id].window);
  }
}

void FullRestoreSaveHandler::OnTaskDestroyed(int32_t task_id) {
  RemoveAppRestoreData(task_id);
  task_id_to_app_window_info_.erase(task_id);
}

void FullRestoreSaveHandler::Flush(const base::FilePath& profile_path) {
  if (save_running_.find(profile_path) != save_running_.end())
    return;

  save_running_.insert(profile_path);

  BackendTaskRunner(profile_path)
      ->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&FullRestoreFileHandler::WriteToFile,
                         GetFileHandler(profile_path),
                         profile_path_to_restore_data_[profile_path].Clone()),
          base::BindOnce(&FullRestoreSaveHandler::OnSaveFinished,
                         weak_factory_.GetWeakPtr(), profile_path));
}

void FullRestoreSaveHandler::RemoveApp(const base::FilePath& profile_path,
                                       const std::string& app_id) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end())
    return;

  it->second.RemoveApp(app_id);

  pending_save_profile_paths_.insert(profile_path);

  MaybeStartSaveTimer();
}

int32_t FullRestoreSaveHandler::GetArcSessionId() {
  if (arc_session_id_ >= kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is too large: " << arc_session_id_;
    arc_session_id_ = 0;
  }

  return ++arc_session_id_;
}

void FullRestoreSaveHandler::MaybeStartSaveTimer() {
  if (!save_timer_.IsRunning() && save_running_.empty()) {
    save_timer_.Start(FROM_HERE, kSaveDelay,
                      base::BindOnce(&FullRestoreSaveHandler::Save,
                                     weak_factory_.GetWeakPtr()));
  }
}

void FullRestoreSaveHandler::Save() {
  if (pending_save_profile_paths_.empty())
    return;

  for (const auto& file_path : pending_save_profile_paths_)
    Flush(file_path);

  pending_save_profile_paths_.clear();
}

void FullRestoreSaveHandler::OnSaveFinished(
    const base::FilePath& profile_path) {
  save_running_.erase(profile_path);
}

FullRestoreFileHandler* FullRestoreSaveHandler::GetFileHandler(
    const base::FilePath& profile_path) {
  if (profile_path_to_file_handler_.find(profile_path) ==
      profile_path_to_file_handler_.end()) {
    profile_path_to_file_handler_[profile_path] =
        base::MakeRefCounted<FullRestoreFileHandler>(profile_path);
  }
  return profile_path_to_file_handler_[profile_path].get();
}

base::SequencedTaskRunner* FullRestoreSaveHandler::BackendTaskRunner(
    const base::FilePath& profile_path) {
  return GetFileHandler(profile_path)->owning_task_runner();
}

void FullRestoreSaveHandler::AddAppLaunchInfo(
    const base::FilePath& profile_path,
    AppLaunchInfoPtr app_launch_info) {
  if (profile_path.empty() || !app_launch_info ||
      !app_launch_info->window_id.has_value()) {
    return;
  }

  window_id_to_app_restore_info_[app_launch_info->window_id.value()] =
      std::make_pair(profile_path, app_launch_info->app_id);

  // Each user should have one full restore file saving the restore data in the
  // profile directory |profile_path|. So |app_launch_info| is saved to the
  // restore data for the user with |profile_path|.
  profile_path_to_restore_data_[profile_path].AddAppLaunchInfo(
      std::move(app_launch_info));

  pending_save_profile_paths_.insert(profile_path);

  MaybeStartSaveTimer();
}

void FullRestoreSaveHandler::ModifyWindowInfo(int window_id,
                                              const WindowInfo& window_info) {
  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  const auto& profile_path = it->second.first;
  const auto& app_id = it->second.second;
  auto restore_data_it = profile_path_to_restore_data_.find(profile_path);
  if (restore_data_it == profile_path_to_restore_data_.end() ||
      !restore_data_it->second.HasAppRestoreData(app_id, window_id)) {
    return;
  }

  profile_path_to_restore_data_[profile_path].ModifyWindowInfo(
      app_id, window_id, window_info);

  pending_save_profile_paths_.insert(profile_path);

  MaybeStartSaveTimer();
}

void FullRestoreSaveHandler::RemoveAppRestoreData(int window_id) {
  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  profile_path_to_restore_data_[it->second.first].RemoveAppRestoreData(
      it->second.second, window_id);

  pending_save_profile_paths_.insert(it->second.first);

  window_id_to_app_restore_info_.erase(it);

  MaybeStartSaveTimer();
}

void FullRestoreSaveHandler::RemoveWindowInfo(const std::string& app_id,
                                              int window_id) {
  if (app_id.empty())
    return;

  auto it = profile_path_to_restore_data_.find(primary_profile_path_);
  if (it == profile_path_to_restore_data_.end())
    return;

  it->second.RemoveWindowInfo(app_id, window_id);
}

}  // namespace full_restore
