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
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/views/widget/widget_delegate.h"

namespace full_restore {

FullRestoreReadHandler* FullRestoreReadHandler::GetInstance() {
  static base::NoDestructor<FullRestoreReadHandler> full_restore_read_handler;
  return full_restore_read_handler.get();
}

FullRestoreReadHandler::FullRestoreReadHandler() {
  if (aura::Env::HasInstance())
    env_observer_.Observe(aura::Env::GetInstance());
}

FullRestoreReadHandler::~FullRestoreReadHandler() = default;

void FullRestoreReadHandler::OnWindowInitialized(aura::Window* window) {
  int32_t window_id = window->GetProperty(::full_restore::kRestoreWindowIdKey);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    // If there isn't restore data for ARC apps, we don't need to handle ARC app
    // windows restoration.
    if (!arc_read_handler_)
      return;

    if (window_id == kParentToHiddenContainer ||
        arc_read_handler_->HasRestoreData(window_id)) {
      observed_windows_.AddObservation(window);

      // If |window| is added to a hidden container, that means the ARC task is
      // not created yet, so add |window| to |arc_window_candidates_| to wait
      // the task to be created.
      if (window_id == kParentToHiddenContainer)
        arc_read_handler_->AddArcWindowCandidate(window);

      FullRestoreInfo::GetInstance()->OnWindowInitialized(window);
    }
    return;
  }

  if (!SessionID::IsValidValue(window_id)) {
    return;
  }

  observed_windows_.AddObservation(window);
  FullRestoreInfo::GetInstance()->OnWindowInitialized(window);
}

void FullRestoreReadHandler::OnWindowDestroyed(aura::Window* window) {
  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    if (arc_read_handler_)
      arc_read_handler_->OnWindowDestroyed(window);
    return;
  }

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
  if (arc_read_handler_)
    arc_read_handler_->OnTaskCreated(app_id, task_id, session_id);
}

void FullRestoreReadHandler::OnTaskDestroyed(int32_t task_id) {
  if (arc_read_handler_)
    arc_read_handler_->OnTaskDestroyed(task_id);
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

void FullRestoreReadHandler::RemoveAppRestoreData(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end())
    return;

  it->second->RemoveAppRestoreData(app_id, restore_window_id);
}

bool FullRestoreReadHandler::HasWindowInfo(int32_t restore_window_id) {
  if (!SessionID::IsValidValue(restore_window_id))
    return false;

  auto it = window_id_to_app_restore_info_.find(restore_window_id);
  if (it == window_id_to_app_restore_info_.end())
    return false;

  return true;
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    aura::Window* window) {
  if (!window)
    return nullptr;

  const int32_t restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);

  if (window->GetProperty(aura::client::kAppType) ==
      static_cast<int>(ash::AppType::ARC_APP)) {
    return arc_read_handler_
               ? arc_read_handler_->GetWindowInfo(restore_window_id)
               : nullptr;
  }

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
  if (!arc_read_handler_)
    return 0;

  return arc_read_handler_->GetArcRestoreWindowId(task_id);
}

void FullRestoreReadHandler::ModifyWidgetParams(
    int32_t restore_window_id,
    views::Widget::InitParams* out_params) {
  DCHECK(out_params);

  const bool is_arc_app =
      out_params->init_properties_container.GetProperty(
          aura::client::kAppType) == static_cast<int>(ash::AppType::ARC_APP);
  std::unique_ptr<WindowInfo> window_info;
  if (is_arc_app) {
    window_info = arc_read_handler_
                      ? arc_read_handler_->GetWindowInfo(restore_window_id)
                      : nullptr;
  } else {
    window_info = GetWindowInfo(restore_window_id);
  }
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
    out_params->init_properties_container.SetProperty(
        kLaunchedFromFullRestoreKey, true);
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

  // Register to track when the widget has initialized. If a delegate is not
  // set, then the widget creator is responsible for calling
  // OnWidgetInitialized.
  views::WidgetDelegate* delegate = out_params->delegate;
  if (delegate) {
    delegate->RegisterWidgetInitializedCallback(
        base::BindOnce(&FullRestoreReadHandler::OnWidgetInitialized,
                       weak_factory_.GetWeakPtr(), delegate));
  }
}

int32_t FullRestoreReadHandler::GetArcSessionId() {
  DCHECK(arc_read_handler_);
  return arc_read_handler_->GetArcSessionId();
}

void FullRestoreReadHandler::SetArcSessionIdForWindowId(int32_t arc_session_id,
                                                        int32_t window_id) {
  DCHECK(arc_read_handler_);
  arc_read_handler_->SetArcSessionIdForWindowId(arc_session_id, window_id);
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end())
    return nullptr;

  return it->second->GetWindowInfo(app_id, restore_window_id);
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    int32_t restore_window_id) {
  if (!SessionID::IsValidValue(restore_window_id))
    return nullptr;

  auto it = window_id_to_app_restore_info_.find(restore_window_id);
  if (it == window_id_to_app_restore_info_.end())
    return nullptr;

  const base::FilePath& profile_path = it->second.first;
  const std::string& app_id = it->second.second;
  return GetWindowInfo(profile_path, app_id, restore_window_id);
}

void FullRestoreReadHandler::OnGetRestoreData(
    const base::FilePath& profile_path,
    Callback callback,
    std::unique_ptr<RestoreData> restore_data) {
  if (restore_data) {
    profile_path_to_restore_data_[profile_path] = restore_data->Clone();

    for (auto it = restore_data->app_id_to_launch_list().begin();
         it != restore_data->app_id_to_launch_list().end(); it++) {
      const std::string& app_id = it->first;
      for (auto data_it = it->second.begin(); data_it != it->second.end();
           data_it++) {
        int32_t window_id = data_it->first;
        // Only ARC app launch parameters have event_flag.
        if (data_it->second->event_flag.has_value()) {
          if (!arc_read_handler_)
            arc_read_handler_ = std::make_unique<ArcReadHandler>(profile_path);
          arc_read_handler_->AddRestoreData(app_id, window_id);
        } else {
          window_id_to_app_restore_info_[window_id] =
              std::make_pair(profile_path, app_id);
        }
      }
    }
  }

  std::move(callback).Run(std::move(restore_data));
}

void FullRestoreReadHandler::RemoveAppRestoreData(int32_t window_id) {
  auto it = window_id_to_app_restore_info_.find(window_id);
  if (it == window_id_to_app_restore_info_.end())
    return;

  const base::FilePath& profile_path = it->second.first;
  const std::string& app_id = it->second.second;
  RemoveAppRestoreData(profile_path, app_id, window_id);

  window_id_to_app_restore_info_.erase(it);
}

void FullRestoreReadHandler::OnWidgetInitialized(
    views::WidgetDelegate* delegate) {
  FullRestoreInfo::GetInstance()->OnWidgetInitialized(delegate->GetWidget());
}

}  // namespace full_restore
