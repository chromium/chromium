// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_read_handler.h"

#include <cstdint>
#include <utility>

#include "ash/public/cpp/app_types.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/client/aura_constants.h"
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
  int32_t window_id = window->GetProperty(::full_restore::kRestoreWindowIdKey);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    if (!base::Contains(window_id_to_app_restore_info_, window_id))
      return;

    observed_windows_.AddObservation(window);
    return;
  }

  if (!SessionID::IsValidValue(window_id)) {
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

void FullRestoreReadHandler::OnTaskCreated(const std::string& app_id,
                                           int32_t task_id,
                                           int32_t session_id) {
  auto it = arc_session_id_to_window_id_.find(session_id);
  if (it == arc_session_id_to_window_id_.end())
    return;

  arc_task_id_to_app_id_window_id_[task_id] =
      std::make_pair(app_id, it->second);
  arc_session_id_to_window_id_.erase(it);
}

void FullRestoreReadHandler::OnTaskDestroyed(int32_t task_id) {
  arc_task_id_to_app_id_window_id_.erase(task_id);
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

int32_t FullRestoreReadHandler::GetArcRestoreWindowId(int32_t task_id) {
  auto it = arc_task_id_to_app_id_window_id_.find(task_id);
  if (it == arc_task_id_to_app_id_window_id_.end())
    return -1;

  return it->second.second;
}

void FullRestoreReadHandler::ModifyWidgetParams(
    int32_t restore_window_id,
    views::Widget::InitParams* out_params) {
  DCHECK(out_params);

  std::unique_ptr<WindowInfo> window_info = GetWindowInfo(restore_window_id);
  if (!window_info)
    return;

  if (window_info->activation_index) {
    const int32_t index = *window_info->activation_index;
    // kActivationIndexKey is owned, which allows for passing in this raw
    // pointer.
    out_params->init_properties_container.SetProperty(kActivationIndexKey,
                                                      new int32_t(index));
    // Windows opened from full restore should not be activated. Widgets that
    // are shown are activated by default. Force the widget to not be
    // activatable; the activation will be restored in ash once the window is
    // launched.
    out_params->activatable = views::Widget::InitParams::ACTIVATABLE_NO;
  }
  if (window_info->desk_id)
    out_params->workspace = base::NumberToString(*window_info->desk_id);
  out_params->visible_on_all_workspaces =
      window_info->visible_on_all_workspaces.has_value();
  if (window_info->current_bounds)
    out_params->bounds = *window_info->current_bounds;
  if (window_info->window_state_type) {
    // ToWindowShowState will make us lose some ash-specific types (left/right
    // snap). Ash is responsible for restoring these states by checking
    // GetWindowInfo.
    out_params->show_state =
        chromeos::ToWindowShowState(*window_info->window_state_type);
  }
}

int32_t FullRestoreReadHandler::GetArcSessionId() {
  if (arc_session_id_ < kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is overflow: " << arc_session_id_;
    arc_session_id_ = kArcSessionIdOffsetForRestoredLaunching;
  }

  return ++arc_session_id_;
}

void FullRestoreReadHandler::SetArcSessionIdForWindowId(int32_t arc_session_id,
                                                        int32_t window_id) {
  DCHECK_GT(arc_session_id,
            full_restore::kArcSessionIdOffsetForRestoredLaunching);
  arc_session_id_to_window_id_[arc_session_id] = window_id;
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
