// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_read_handler.h"

#include <cstdint>
#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/full_restore_utils.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/env.h"

namespace full_restore {

FullRestoreReadHandler* FullRestoreReadHandler::GetInstance() {
  static base::NoDestructor<FullRestoreReadHandler> full_restore_read_handler;
  return full_restore_read_handler.get();
}

FullRestoreReadHandler::FullRestoreReadHandler() {
  aura::Env::GetInstance()->AddObserver(this);
}

FullRestoreReadHandler::~FullRestoreReadHandler() {
  aura::Env::GetInstance()->RemoveObserver(this);
}

void FullRestoreReadHandler::OnWindowInitialized(aura::Window* window) {
  // TODO(crbug.com/1146900): Handle ARC app windows.

  if (!SessionID::IsValidValue(
          window->GetProperty(::full_restore::kRestoreWindowIdKey))) {
    return;
  }

  observed_windows_.AddObservation(window);
}

void FullRestoreReadHandler::OnWindowDestroyed(aura::Window* window) {
  // TODO(crbug.com/1146900): Handle ARC app windows.

  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  int32_t restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);
  DCHECK(SessionID::IsValidValue(restore_window_id));

  RemoveAppRestoreData(restore_window_id);
}

void FullRestoreReadHandler::SetActiveProfilePath(
    const base::FilePath& profile_path) {
  active_profile_path_ = profile_path;
}

void FullRestoreReadHandler::ReadFromFile(const base::FilePath& profile_path,
                                          Callback callback) {
  auto file_handler =
      base::MakeRefCounted<FullRestoreFileHandler>(profile_path);
  file_handler->owning_task_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&FullRestoreFileHandler::ReadFromFile, file_handler.get()),
      base::BindOnce(&FullRestoreReadHandler::OnGetRestoreData,
                     weak_factory_.GetWeakPtr(), profile_path,
                     std::move(callback)));
}

void FullRestoreReadHandler::SetNextRestoreWindowIdForChromeApp(
    const base::FilePath& profile_path,
    const std::string& app_id) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end())
    return;

  it->second->SetNextRestoreWindowIdForChromeApp(app_id);
}

void FullRestoreReadHandler::RemoveApp(const base::FilePath& profile_path,
                                       const std::string& app_id) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end())
    return;

  it->second->RemoveApp(app_id);
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    int32_t restore_window_id) {
  // TODO(crbug.com/1146900): Handle ARC app windows.

  if (!SessionID::IsValidValue(restore_window_id))
    return nullptr;

  auto it = window_id_to_app_restore_info_.find(restore_window_id);
  if (it == window_id_to_app_restore_info_.end())
    return nullptr;

  return profile_path_to_restore_data_[it->second.first]->GetWindowInfo(
      it->second.second, restore_window_id);
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    aura::Window* window) {
  if (!window)
    return nullptr;

  const int32_t restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);
  return GetWindowInfo(restore_window_id);
}

int32_t FullRestoreReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  auto it = profile_path_to_restore_data_.find(active_profile_path_);
  if (it == profile_path_to_restore_data_.end())
    return 0;

  return it->second->FetchRestoreWindowId(app_id);
}

void FullRestoreReadHandler::OnGetRestoreData(
    const base::FilePath& profile_path,
    Callback callback,
    std::unique_ptr<RestoreData> restore_data) {
  if (restore_data) {
    profile_path_to_restore_data_[profile_path] = restore_data->Clone();

    for (auto it = restore_data->app_id_to_launch_list().begin();
         it != restore_data->app_id_to_launch_list().end(); it++) {
      for (auto data_it = it->second.begin(); data_it != it->second.end();
           data_it++) {
        window_id_to_app_restore_info_[data_it->first] =
            std::make_pair(profile_path, it->first);
      }
    }
  }

  std::move(callback).Run(std::move(restore_data));
}

void FullRestoreReadHandler::RemoveAppRestoreData(int window_id) {
  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  profile_path_to_restore_data_[it->second.first]->RemoveAppRestoreData(
      it->second.second, window_id);

  window_id_to_app_restore_info_.erase(it);
}

}  // namespace full_restore
