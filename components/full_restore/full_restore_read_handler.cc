// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/full_restore_read_handler.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "ash/constants/app_types.h"
#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/full_restore/app_launch_info.h"
#include "components/full_restore/desk_template_read_handler.h"
#include "components/full_restore/full_restore_file_handler.h"
#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_save_handler.h"
#include "components/full_restore/restore_data.h"
#include "components/full_restore/window_info.h"
#include "components/sessions/core/session_id.h"
#include "extensions/common/constants.h"
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

void FullRestoreReadHandler::SetCheckRestoreData(
    const base::FilePath& profile_path) {
  should_check_restore_data_.insert(profile_path);
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
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it != profile_path_to_restore_data_.end()) {
    // If the restore data has been read from the file, just use it, and don't
    // need to read it again.
    //
    // We must use post task here, because FullRestoreAppLaunchHandler calls
    // ReadFromFile in FullRestoreService construct function, and the callback
    // in FullRestoreAppLaunchHandler calls the init function of
    // FullRestoreService. If we don't use post task, and call the callback
    // function directly, it could cause deadloop.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(callback),
                       (it->second ? it->second->Clone() : nullptr)));
    return;
  }

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
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return;

  restore_data->SetNextRestoreWindowIdForChromeApp(app_id);
}

void FullRestoreReadHandler::RemoveApp(const base::FilePath& profile_path,
                                       const std::string& app_id) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return;

  restore_data->RemoveApp(app_id);
}

void FullRestoreReadHandler::RemoveAppRestoreData(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return;

  restore_data->RemoveAppRestoreData(app_id, restore_window_id);
}

bool FullRestoreReadHandler::HasAppTypeBrowser(
    const base::FilePath& profile_path) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return false;

  return restore_data->HasAppTypeBrowser();
}

bool FullRestoreReadHandler::HasBrowser(const base::FilePath& profile_path) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return false;

  return restore_data->HasBrowser();
}

bool FullRestoreReadHandler::HasWindowInfo(int32_t restore_window_id) {
  if (!SessionID::IsValidValue(restore_window_id) ||
      !base::Contains(should_check_restore_data_, active_profile_path_)) {
    return false;
  }

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

std::unique_ptr<AppLaunchInfo> FullRestoreReadHandler::GetArcAppLaunchInfo(
    const std::string& app_id,
    int32_t session_id) {
  return arc_read_handler_
             ? arc_read_handler_->GetArcAppLaunchInfo(app_id, session_id)
             : nullptr;
}

int32_t FullRestoreReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  auto* restore_data = GetRestoreData(active_profile_path_);
  if (!restore_data)
    return 0;

  return restore_data->FetchRestoreWindowId(app_id);
}

int32_t FullRestoreReadHandler::GetArcRestoreWindowIdForTaskId(
    int32_t task_id) {
  if (!arc_read_handler_)
    return 0;

  return arc_read_handler_->GetArcRestoreWindowIdForTaskId(task_id);
}

int32_t FullRestoreReadHandler::GetArcRestoreWindowIdForSessionId(
    int32_t session_id) {
  if (!arc_read_handler_)
    return 0;

  return arc_read_handler_->GetArcRestoreWindowIdForSessionId(session_id);
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
    // `DeskTemplateReadHandler::GetWindowInfo()` will return nullptr if full
    // restore is running.
    // TODO(sammiequon): Separate full restore and desk templates logic.
    window_info = DeskTemplateReadHandler::GetInstance()->GetWindowInfo(
        restore_window_id);
    if (!window_info &&
        base::Contains(should_check_restore_data_, active_profile_path_)) {
      window_info = GetWindowInfo(restore_window_id);
    }
  }
  if (!window_info)
    return;

  ApplyProperties(window_info.get(), &out_params->init_properties_container);

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

void FullRestoreReadHandler::ApplyProperties(
    WindowInfo* window_info,
    ui::PropertyHandler* property_handler) {
  DCHECK(window_info);
  DCHECK(property_handler);

  // Create a clone so `property_handler` can have complete ownership of a copy
  // of WindowInfo.
  WindowInfo* window_info_clone = window_info->Clone();
  property_handler->SetProperty(kWindowInfoKey, window_info_clone);

  if (window_info->activation_index) {
    const int32_t index = *window_info->activation_index;
    // kActivationIndexKey is owned, which allows for passing in this raw
    // pointer.
    property_handler->SetProperty(kActivationIndexKey, new int32_t(index));
    // Windows opened from full restore should not be activated. Widgets that
    // are shown are activated by default. Force the widget to not be
    // activatable; the activation will be restored in ash once the window is
    // launched.
    property_handler->SetProperty(kLaunchedFromFullRestoreKey, true);
  }
  if (window_info->pre_minimized_show_state_type) {
    property_handler->SetProperty(aura::client::kPreMinimizedShowStateKey,
                                  *window_info->pre_minimized_show_state_type);
  }
}

void FullRestoreReadHandler::AddChromeBrowserLaunchInfoForTesting(
    const base::FilePath& profile_path) {
  auto session_id = SessionID::NewUnique();
  auto app_launch_info = std::make_unique<AppLaunchInfo>(
      extension_misc::kChromeAppId, session_id.id());
  app_launch_info->app_type_browser = true;

  if (profile_path_to_restore_data_.find(profile_path) ==
      profile_path_to_restore_data_.end()) {
    profile_path_to_restore_data_[profile_path] =
        std::make_unique<RestoreData>();
  }

  profile_path_to_restore_data_[profile_path]->AddAppLaunchInfo(
      std::move(app_launch_info));
  window_id_to_app_restore_info_[session_id.id()] =
      std::make_pair(profile_path, extension_misc::kChromeAppId);
}

std::unique_ptr<AppLaunchInfo> FullRestoreReadHandler::GetAppLaunchInfo(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return nullptr;

  return restore_data->GetAppLaunchInfo(app_id, restore_window_id);
}

std::unique_ptr<WindowInfo> FullRestoreReadHandler::GetWindowInfo(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  auto* restore_data = GetRestoreData(profile_path);
  if (!restore_data)
    return nullptr;

  return restore_data->GetWindowInfo(app_id, restore_window_id);
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
  } else {
    profile_path_to_restore_data_[profile_path] = nullptr;
  }

  std::move(callback).Run(std::move(restore_data));

  // Call FullRestoreSaveHandler to start a timer to clear the restore data
  // after reading the restore data. Otherwise, if the user doesn't select
  // restore, and never launch a new app, the restore data is not cleared. So
  // when the system is reboot, the restore process could restore the previous
  // record before the last reboot.
  FullRestoreSaveHandler::GetInstance()->ClearRestoreData(profile_path);
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

RestoreData* FullRestoreReadHandler::GetRestoreData(
    const base::FilePath& profile_path) {
  auto it = profile_path_to_restore_data_.find(profile_path);
  if (it == profile_path_to_restore_data_.end() || !it->second) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace full_restore
