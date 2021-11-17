// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/desk_template_read_handler.h"

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace app_restore {

DeskTemplateReadHandler::DeskTemplateReadHandler() {
  if (aura::Env::HasInstance())
    env_observer_.Observe(aura::Env::GetInstance());
  arc_info_observer_.Observe(app_restore::AppRestoreArcInfo::GetInstance());
}

DeskTemplateReadHandler::~DeskTemplateReadHandler() = default;

// static
DeskTemplateReadHandler* DeskTemplateReadHandler::Get() {
  static base::NoDestructor<DeskTemplateReadHandler> desk_template_read_handler;
  return desk_template_read_handler.get();
}

void DeskTemplateReadHandler::SetRestoreData(
    std::unique_ptr<RestoreData> restore_data) {
  // It is expected we do not replace an existing valid `restore_data_` before
  // it is cleared by the app launch handler.
  if (restore_data) {
    DCHECK(!restore_data_)
        << "Restore data should be cleared before setting a new one";
  }

  restore_data_ = std::move(restore_data);

  if (!ash::features::AreDesksTemplatesEnabled())
    return;

  arc_read_handler_.reset();
  if (!restore_data_)
    return;

  // Add restore data to the `arc_read_handler_` for ARC apps. Create
  // `arc_read_handler_` if we have at least one ARC app.
  for (const std::pair<std::string, const RestoreData::LaunchList&> entry :
       restore_data_->app_id_to_launch_list()) {
    const std::string& app_id = entry.first;
    for (const std::pair<int, const std::unique_ptr<AppRestoreData>&>
             app_restore_data : entry.second) {
      // Only ARC app launch parameters have event_flag.
      if (!app_restore_data.second->event_flag.has_value())
        continue;
      if (!arc_read_handler_) {
        arc_read_handler_ =
            std::make_unique<ArcReadHandler>(base::FilePath(), this);
      }
      const int32_t& window_id = app_restore_data.first;
      arc_read_handler_->AddRestoreData(app_id, window_id);
    }
  }
}

std::unique_ptr<WindowInfo> DeskTemplateReadHandler::GetWindowInfo(
    int restore_window_id) {
  if (!restore_data_)
    return nullptr;

  // Try to find the window info associated with `restore_window_id`.
  const RestoreData::AppIdToLaunchList& launch_list =
      restore_data_->app_id_to_launch_list();
  for (const auto& it : launch_list) {
    const std::string& app_id = it.first;
    const AppRestoreData* app_restore_data =
        restore_data_->GetAppRestoreData(app_id, restore_window_id);
    if (app_restore_data)
      return app_restore_data->GetWindowInfo();
  }

  return nullptr;
}

int32_t DeskTemplateReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  return restore_data_ ? restore_data_->FetchRestoreWindowId(app_id) : 0;
}

void DeskTemplateReadHandler::SetNextRestoreWindowIdForChromeApp(
    const std::string& app_id) {
  if (restore_data_)
    restore_data_->SetNextRestoreWindowIdForChromeApp(app_id);
}

int32_t DeskTemplateReadHandler::GetArcSessionId() {
  return arc_read_handler_ ? arc_read_handler_->GetArcSessionId() : 0;
}

void DeskTemplateReadHandler::SetArcSessionIdForWindowId(int32_t arc_session_id,
                                                         int32_t window_id) {
  if (arc_read_handler_)
    arc_read_handler_->SetArcSessionIdForWindowId(arc_session_id, window_id);
}

int32_t DeskTemplateReadHandler::GetArcRestoreWindowIdForTaskId(
    int32_t task_id) {
  return arc_read_handler_
             ? arc_read_handler_->GetArcRestoreWindowIdForTaskId(task_id)
             : 0;
}

int32_t DeskTemplateReadHandler::GetArcRestoreWindowIdForSessionId(
    int32_t session_id) {
  return arc_read_handler_
             ? arc_read_handler_->GetArcRestoreWindowIdForSessionId(session_id)
             : 0;
}

void DeskTemplateReadHandler::OnWindowInitialized(aura::Window* window) {
  // If there isn't restore data for ARC apps, we don't need to handle ARC app
  // windows restoration.
  if (!arc_read_handler_)
    return;

  if (window->GetProperty(aura::client::kAppType) !=
      static_cast<int>(ash::AppType::ARC_APP)) {
    return;
  }

  const int32_t window_id = window->GetProperty(kRestoreWindowIdKey);
  if (window_id == app_restore::kParentToHiddenContainer ||
      arc_read_handler_->HasRestoreData(window_id)) {
    observed_windows_.AddObservation(window);
    arc_read_handler_->AddArcWindowCandidate(window);
  }
}

void DeskTemplateReadHandler::OnWindowDestroyed(aura::Window* window) {
  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  if (arc_read_handler_)
    arc_read_handler_->OnWindowDestroyed(window);
}

std::unique_ptr<app_restore::AppLaunchInfo>
DeskTemplateReadHandler::GetAppLaunchInfo(const base::FilePath& profile_path,
                                          const std::string& app_id,
                                          int32_t restore_window_id) {
  return restore_data_
             ? restore_data_->GetAppLaunchInfo(app_id, restore_window_id)
             : nullptr;
}

std::unique_ptr<WindowInfo> DeskTemplateReadHandler::GetWindowInfo(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  return restore_data_ ? restore_data_->GetWindowInfo(app_id, restore_window_id)
                       : nullptr;
}

void DeskTemplateReadHandler::RemoveAppRestoreData(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  if (restore_data_)
    restore_data_->RemoveAppRestoreData(app_id, restore_window_id);
}

void DeskTemplateReadHandler::OnTaskCreated(const std::string& app_id,
                                            int32_t task_id,
                                            int32_t session_id) {
  if (arc_read_handler_)
    arc_read_handler_->OnTaskCreated(app_id, task_id, session_id);
}

void DeskTemplateReadHandler::OnTaskDestroyed(int32_t task_id) {
  if (arc_read_handler_)
    arc_read_handler_->OnTaskDestroyed(task_id);
}

}  // namespace app_restore
