// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/desk_template_read_handler.h"

#include "base/containers/adapters.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/client/aura_constants.h"

namespace app_restore {

namespace {

// Erase all entries in `map` that have the value `value`.
void EraseMapByValue(base::flat_map<int32_t, int32_t>& map, int32_t value) {
  for (auto it = map.begin(); it != map.end();) {
    if (it->second == value)
      it = map.erase(it);
    else
      ++it;
  }
}

}  // namespace

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

ArcReadHandler* DeskTemplateReadHandler::GetArcReadHandlerForWindow(
    int32_t restore_window_id) {
  return GetArcReadHandlerForLaunch(
      GetLaunchIdForRestoreWindowId(restore_window_id));
}

void DeskTemplateReadHandler::SetRestoreData(
    int32_t launch_id,
    std::unique_ptr<RestoreData> restore_data) {
  RestoreData* rd = restore_data.get();

  DCHECK_EQ(restore_data_.count(launch_id), 0u);
  DCHECK_EQ(arc_read_handler_.count(launch_id), 0u);

  restore_data_[launch_id] = std::move(restore_data);

  // Set up mapping from restore window IDs to launch ID. Create an ARC read
  // handler and add restore data to it if we have at least one ARC app.
  for (const auto& [app_id, launch_list] : rd->app_id_to_launch_list()) {
    for (const auto& [window_id, app_restore_data] : launch_list) {
      restore_window_id_to_launch_id_[window_id] = launch_id;

      // Only ARC app launch parameters have event_flag.
      if (!app_restore_data->event_flag.has_value())
        continue;

      auto& arc_read_handler = arc_read_handler_[launch_id];
      if (!arc_read_handler) {
        arc_read_handler =
            std::make_unique<ArcReadHandler>(base::FilePath(), this);
      }

      arc_read_handler->AddRestoreData(app_id, window_id);
    }
  }
}

RestoreData* DeskTemplateReadHandler::GetRestoreDataForWindow(
    int32_t restore_window_id) {
  auto it =
      restore_data_.find(GetLaunchIdForRestoreWindowId(restore_window_id));
  return it != restore_data_.end() ? it->second.get() : nullptr;
}

void DeskTemplateReadHandler::ClearRestoreData(int32_t launch_id) {
  restore_data_.erase(launch_id);
  arc_read_handler_.erase(launch_id);

  EraseMapByValue(restore_window_id_to_launch_id_, launch_id);
  EraseMapByValue(session_id_to_launch_id_, launch_id);
  EraseMapByValue(task_id_to_launch_id_, launch_id);
}

std::unique_ptr<WindowInfo> DeskTemplateReadHandler::GetWindowInfo(
    int32_t restore_window_id) {
  const auto* restore_data = GetRestoreDataForWindow(restore_window_id);
  if (!restore_data)
    return nullptr;

  // Try to find the window info associated with `restore_window_id`.
  const RestoreData::AppIdToLaunchList& launch_list =
      restore_data->app_id_to_launch_list();
  for (const auto& it : launch_list) {
    const std::string& app_id = it.first;
    const AppRestoreData* app_restore_data =
        restore_data->GetAppRestoreData(app_id, restore_window_id);
    if (app_restore_data)
      return app_restore_data->GetWindowInfo();
  }

  return nullptr;
}

int32_t DeskTemplateReadHandler::FetchRestoreWindowId(
    const std::string& app_id) {
  if (RestoreData* restore_data = GetMostRecentRestoreDataForApp(app_id))
    return restore_data->FetchRestoreWindowId(app_id);
  return 0;
}

void DeskTemplateReadHandler::SetNextRestoreWindowIdForChromeApp(
    const std::string& app_id) {
  if (RestoreData* restore_data = GetMostRecentRestoreDataForApp(app_id))
    restore_data->SetNextRestoreWindowIdForChromeApp(app_id);
}

void DeskTemplateReadHandler::SetLaunchIdForArcSessionId(int32_t arc_session_id,
                                                         int32_t launch_id) {
  session_id_to_launch_id_[arc_session_id] = launch_id;
}

void DeskTemplateReadHandler::SetArcSessionIdForWindowId(int32_t arc_session_id,
                                                         int32_t window_id) {
  if (int32_t launch_id = GetLaunchIdForArcSessionId(arc_session_id)) {
    if (ArcReadHandler* handler = GetArcReadHandlerForLaunch(launch_id))
      handler->SetArcSessionIdForWindowId(arc_session_id, window_id);
  }
}

int32_t DeskTemplateReadHandler::GetArcRestoreWindowIdForTaskId(
    int32_t task_id) {
  auto it = task_id_to_launch_id_.find(task_id);
  if (it == task_id_to_launch_id_.end())
    return 0;

  ArcReadHandler* handler = GetArcReadHandlerForLaunch(it->second);
  return handler ? handler->GetArcRestoreWindowIdForTaskId(task_id) : 0;
}

int32_t DeskTemplateReadHandler::GetArcRestoreWindowIdForSessionId(
    int32_t session_id) {
  int32_t launch_id = GetLaunchIdForArcSessionId(session_id);
  if (!launch_id)
    return 0;

  ArcReadHandler* handler = GetArcReadHandlerForLaunch(launch_id);
  return handler ? handler->GetArcRestoreWindowIdForSessionId(session_id) : 0;
}

bool DeskTemplateReadHandler::IsKnownArcSessionId(int32_t session_id) const {
  return session_id_to_launch_id_.contains(session_id);
}

void DeskTemplateReadHandler::OnWindowInitialized(aura::Window* window) {
  // If there isn't restore data for ARC apps, we don't need to handle ARC app
  // windows restoration.
  if (arc_read_handler_.empty() || !IsArcWindow(window))
    return;

  const int32_t window_id = window->GetProperty(kRestoreWindowIdKey);
  ArcReadHandler* handler = GetArcReadHandlerForWindow(window_id);

  if (window_id == app_restore::kParentToHiddenContainer ||
      (handler && handler->HasRestoreData(window_id))) {
    observed_windows_.AddObservation(window);
    handler->AddArcWindowCandidate(window);
  }
}

void DeskTemplateReadHandler::OnWindowDestroyed(aura::Window* window) {
  DCHECK(observed_windows_.IsObservingSource(window));
  observed_windows_.RemoveObservation(window);

  const int32_t window_id = window->GetProperty(kRestoreWindowIdKey);
  if (ArcReadHandler* handler = GetArcReadHandlerForWindow(window_id))
    handler->OnWindowDestroyed(window);
}

std::unique_ptr<app_restore::AppLaunchInfo>
DeskTemplateReadHandler::GetAppLaunchInfo(const base::FilePath& profile_path,
                                          const std::string& app_id,
                                          int32_t restore_window_id) {
  if (RestoreData* restore_data = GetRestoreDataForWindow(restore_window_id))
    return restore_data->GetAppLaunchInfo(app_id, restore_window_id);
  return nullptr;
}

std::unique_ptr<WindowInfo> DeskTemplateReadHandler::GetWindowInfo(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  if (RestoreData* restore_data = GetRestoreDataForWindow(restore_window_id))
    return restore_data->GetWindowInfo(app_id, restore_window_id);
  return nullptr;
}

void DeskTemplateReadHandler::RemoveAppRestoreData(
    const base::FilePath& profile_path,
    const std::string& app_id,
    int32_t restore_window_id) {
  if (RestoreData* restore_data = GetRestoreDataForWindow(restore_window_id))
    restore_data->RemoveAppRestoreData(app_id, restore_window_id);
}

void DeskTemplateReadHandler::OnTaskCreated(const std::string& app_id,
                                            int32_t task_id,
                                            int32_t session_id) {
  int32_t launch_id = GetLaunchIdForArcSessionId(session_id);
  // If the task's `session_id` isn't one we are tracking, then this task has
  // not been created from a desk template launch. When this is the case, we
  // don't track the task id.
  if (launch_id == 0)
    return;

  task_id_to_launch_id_[task_id] = launch_id;

  if (ArcReadHandler* handler = GetArcReadHandlerForLaunch(launch_id))
    handler->OnTaskCreated(app_id, task_id, session_id);
}

void DeskTemplateReadHandler::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_launch_id_.find(task_id);
  if (it == task_id_to_launch_id_.end())
    return;

  if (ArcReadHandler* handler = GetArcReadHandlerForLaunch(it->second))
    handler->OnTaskDestroyed(task_id);
}

int32_t DeskTemplateReadHandler::GetLaunchIdForArcSessionId(
    int32_t arc_session_id) {
  auto it = session_id_to_launch_id_.find(arc_session_id);
  return it != session_id_to_launch_id_.end() ? it->second : 0;
}

int32_t DeskTemplateReadHandler::GetLaunchIdForRestoreWindowId(
    int32_t restore_window_id) {
  auto it = restore_window_id_to_launch_id_.find(restore_window_id);
  return it != restore_window_id_to_launch_id_.end() ? it->second : 0;
}

ArcReadHandler* DeskTemplateReadHandler::GetArcReadHandlerForLaunch(
    int32_t launch_id) {
  auto it = arc_read_handler_.find(launch_id);
  return it != arc_read_handler_.end() ? it->second.get() : nullptr;
}

RestoreData* DeskTemplateReadHandler::GetMostRecentRestoreDataForApp(
    const std::string& app_id) {
  // Go from newest to oldest.
  for (const auto& entry : base::Reversed(restore_data_)) {
    const std::unique_ptr<RestoreData>& restore_data = entry.second;
    if (restore_data->app_id_to_launch_list().count(app_id)) {
      return restore_data.get();
    }
  }
  return nullptr;
}
}  // namespace app_restore
