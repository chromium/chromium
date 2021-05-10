// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/arc_read_handler.h"

#include "components/full_restore/full_restore_info.h"
#include "components/full_restore/full_restore_read_handler.h"
#include "components/full_restore/window_info.h"
#include "ui/aura/window.h"

namespace full_restore {

ArcReadHandler::ArcReadHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ArcReadHandler::~ArcReadHandler() = default;

void ArcReadHandler::AddRestoreData(const std::string& app_id,
                                    int32_t window_id) {
  window_id_to_app_id_[window_id] = app_id;
}

void ArcReadHandler::AddArcWindowCandidate(aura::Window* window) {
  arc_window_candidates_.insert(window);
}

void ArcReadHandler::OnWindowDestroyed(aura::Window* window) {
  DCHECK(window);

  // If |window| is list in |arc_window_candidates_|, |window| is not attached
  // to a valid restore window id yet, so we don't need to remove AppRestoreData
  // from the restore data.
  auto it = arc_window_candidates_.find(window);
  if (it != arc_window_candidates_.end()) {
    arc_window_candidates_.erase(it);
    return;
  }

  int32_t restore_window_id =
      window->GetProperty(::full_restore::kRestoreWindowIdKey);
  RemoveAppRestoreData(restore_window_id);
}

void ArcReadHandler::OnTaskCreated(const std::string& app_id,
                                   int32_t task_id,
                                   int32_t session_id) {
  auto it = session_id_to_window_id_.find(session_id);
  if (it == session_id_to_window_id_.end())
    return;

  int32_t restore_window_id = it->second;
  session_id_to_window_id_.erase(it);
  task_id_to_window_id_[task_id] = restore_window_id;

  // Go through |arc_window_candidates_|. If the window for |task_id| has been
  // created, set the correct restore window id, and remove the window from the
  // hidden container.
  auto window_it = std::find_if(
      arc_window_candidates_.begin(), arc_window_candidates_.end(),
      [task_id](aura::Window* window) {
        return window->GetProperty(::full_restore::kWindowIdKey) == task_id;
      });
  if (window_it != arc_window_candidates_.end()) {
    (*window_it)
        ->SetProperty(full_restore::kRestoreWindowIdKey, restore_window_id);
    FullRestoreInfo::GetInstance()->OnARCTaskReadyForUnparentedWindow(
        *window_it);
    arc_window_candidates_.erase(*window_it);
  }
}

void ArcReadHandler::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_window_id_.find(task_id);
  if (it == task_id_to_window_id_.end())
    return;

  int32_t window_id = it->second;
  task_id_to_window_id_.erase(it);

  RemoveAppRestoreData(window_id);
}

bool ArcReadHandler::HasRestoreData(int32_t window_id) {
  return base::Contains(window_id_to_app_id_, window_id);
}

std::unique_ptr<WindowInfo> ArcReadHandler::GetWindowInfo(
    int32_t restore_window_id) {
  if (restore_window_id == 0 || restore_window_id == kParentToHiddenContainer)
    return nullptr;

  auto it = window_id_to_app_id_.find(restore_window_id);
  if (it == window_id_to_app_id_.end())
    return nullptr;

  auto window_info = FullRestoreReadHandler::GetInstance()->GetWindowInfo(
      profile_path_, it->second, restore_window_id);

  // For ARC windows, Android can restore window bounds, so remove the window
  // bounds from the window info.
  window_info->current_bounds.reset();

  // For ARC windows, Android can restore window minimized or maximized status,
  // so remove the WindowStateType from the window info for the minimized and
  // maximized state.
  if (window_info->window_state_type.has_value() &&
      (chromeos::IsMinimizedWindowStateType(
           window_info->window_state_type.value()) ||
       window_info->window_state_type.value() ==
           chromeos::WindowStateType::kMaximized)) {
    window_info->window_state_type.reset();
  }

  return window_info;
}

int32_t ArcReadHandler::GetArcRestoreWindowId(int32_t task_id) {
  auto it = task_id_to_window_id_.find(task_id);
  if (it != task_id_to_window_id_.end())
    return it->second;

  // If |session_id_to_window_id_| is empty, that means there is no ARC apps
  // launched.
  if (session_id_to_window_id_.empty())
    return 0;

  // If |session_id_to_window_id_| is not empty, that means there are ARC
  // apps launched. Returns -1 to add the ARC app window to the hidden
  // container.
  return kParentToHiddenContainer;
}

int32_t ArcReadHandler::GetArcSessionId() {
  if (session_id_ < kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is overflow: " << session_id_;
    session_id_ = kArcSessionIdOffsetForRestoredLaunching;
  }

  return ++session_id_;
}

void ArcReadHandler::SetArcSessionIdForWindowId(int32_t session_id,
                                                int32_t window_id) {
  DCHECK_GT(session_id, full_restore::kArcSessionIdOffsetForRestoredLaunching);
  session_id_to_window_id_[session_id] = window_id;
}

void ArcReadHandler::RemoveAppRestoreData(int32_t window_id) {
  if (window_id == 0 || window_id == kParentToHiddenContainer)
    return;

  auto it = window_id_to_app_id_.find(window_id);
  if (it == window_id_to_app_id_.end())
    return;

  const std::string& app_id = it->second;
  FullRestoreReadHandler::GetInstance()->RemoveAppRestoreData(
      profile_path_, app_id, window_id);

  window_id_to_app_id_.erase(it);
}

}  // namespace full_restore
