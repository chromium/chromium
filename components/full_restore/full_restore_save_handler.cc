// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_save_handler.h"

#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"

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

void FullRestoreSaveHandler::OnWindowInitialized(aura::Window* window) {
  // TODO(crbug.com/1146900): Handle ARC app windows.

  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);

  if (!SessionID::IsValidValue(window_id))
    return;

  observed_windows_.AddObservation(window);
}

void FullRestoreSaveHandler::OnWindowDestroying(aura::Window* window) {
  // TODO(crbug.com/1146900): Handle ARC app windows.

  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  int32_t window_id = window->GetProperty(::full_restore::kWindowIdKey);
  DCHECK(SessionID::IsValidValue(window_id));

  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  profile_path_to_restore_data_[it->second.first].RemoveAppRestoreData(
      it->second.second, window_id);

  pending_save_profile_paths_.insert(it->second.first);

  window_id_to_app_restore_info_.erase(it);

  MaybeStartSaveTimer();
}

void FullRestoreSaveHandler::SaveAppLaunchInfo(
    const base::FilePath& profile_path,
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  if (!app_launch_info->window_id.has_value()) {
    // TODO(crbug.com/1146900): Handle ARC app windows.
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

void FullRestoreSaveHandler::SaveWindowInfo(const WindowInfo& window_info) {
  if (!window_info.window)
    return;

  int32_t window_id =
      window_info.window->GetProperty(::full_restore::kWindowIdKey);

  if (window_info.window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    // TODO(crbug.com/1146900): Handle ARC app windows.
    return;
  }

  if (!SessionID::IsValidValue(window_id))
    return;

  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  profile_path_to_restore_data_[it->second.first].ModifyWindowInfo(
      it->second.second, window_id, window_info);
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

void FullRestoreSaveHandler::OnSaveFinished(const base::FilePath& file_path) {
  save_running_.erase(file_path);
}

FullRestoreFileHandler* FullRestoreSaveHandler::GetFileHandler(
    const base::FilePath& file_path) {
  if (profile_path_to_file_handler_.find(file_path) ==
      profile_path_to_file_handler_.end()) {
    profile_path_to_file_handler_[file_path] =
        base::MakeRefCounted<FullRestoreFileHandler>(file_path);
  }
  return profile_path_to_file_handler_[file_path].get();
}

base::SequencedTaskRunner* FullRestoreSaveHandler::BackendTaskRunner(
    const base::FilePath& file_path) {
  return GetFileHandler(file_path)->owning_task_runner();
}

}  // namespace full_restore
