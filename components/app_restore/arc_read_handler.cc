// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/arc_read_handler.h"

#include <memory>

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window.h"

namespace app_restore {
namespace {

bool IsValidRestoreWindowId(int32_t restore_window_id) {
  return restore_window_id != 0 &&
         restore_window_id != kParentToHiddenContainer;
}

}  // namespace

ArcReadHandler::ArcReadHandler(const base::FilePath& profile_path,
                               Delegate* delegate)
    : profile_path_(profile_path), delegate_(delegate) {
  DCHECK(delegate_);
}

ArcReadHandler::~ArcReadHandler() = default;

void ArcReadHandler::AddRestoreData(const std::string& app_id,
                                    int32_t window_id) {
  window_id_to_app_id_[window_id] = app_id;
}

void ArcReadHandler::AddArcWindowCandidate(aura::Window* window) {
  if (!base::Contains(task_id_to_window_id_,
                      window->GetProperty(kWindowIdKey))) {
    // Check `session_id` to see whether this is a ghost window.
    int32_t session_id = window->GetProperty(kGhostWindowSessionIdKey);
    if (session_id >= kArcSessionIdOffsetForRestoredLaunching)
      return;

    // If the task hasn't been created, and this is not a ghost window, add
    // `window` to `arc_window_candidates_` to wait for the task to be created.
    arc_window_candidates_.insert(window);
  }
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

  int32_t restore_window_id = window->GetProperty(kRestoreWindowIdKey);
  RemoveAppRestoreData(restore_window_id);
}

void ArcReadHandler::OnTaskCreated(const std::string& app_id,
                                   int32_t task_id,
                                   int32_t session_id) {
  auto it = session_id_to_window_id_.find(session_id);
  if (it == session_id_to_window_id_.end()) {
    not_restored_task_ids_.insert(task_id);
    UpdateWindowCandidates(task_id, /*restore_window_id=*/-1);
    return;
  }

  int32_t restore_window_id = it->second;
  session_id_to_window_id_.erase(it);
  task_id_to_window_id_[task_id] = restore_window_id;

  UpdateWindowCandidates(task_id, restore_window_id);
}

void ArcReadHandler::OnTaskDestroyed(int32_t task_id) {
  not_restored_task_ids_.erase(task_id);

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

std::unique_ptr<AppLaunchInfo> ArcReadHandler::GetArcAppLaunchInfo(
    const std::string& app_id,
    int32_t session_id) {
  int32_t restore_window_id = GetArcRestoreWindowIdForSessionId(session_id);
  if (restore_window_id == 0)
    return nullptr;

  return delegate_->GetAppLaunchInfo(profile_path_, app_id, restore_window_id);
}

std::unique_ptr<WindowInfo> ArcReadHandler::GetWindowInfo(
    int32_t restore_window_id) {
  if (!IsValidRestoreWindowId(restore_window_id))
    return nullptr;

  auto it = window_id_to_app_id_.find(restore_window_id);
  if (it == window_id_to_app_id_.end())
    return nullptr;

  std::unique_ptr<WindowInfo> window_info =
      delegate_->GetWindowInfo(profile_path_, it->second, restore_window_id);
  if (!window_info)
    return nullptr;

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

int32_t ArcReadHandler::GetArcRestoreWindowIdForTaskId(int32_t task_id) {
  auto it = task_id_to_window_id_.find(task_id);
  if (it != task_id_to_window_id_.end())
    return it->second;

  // If |session_id_to_window_id_| is empty, that means there is no ARC apps
  // launched. If `not_restored_task_ids_` has `task_id`, that means the ARC app
  // window is not restored.
  if (session_id_to_window_id_.empty() ||
      base::Contains(not_restored_task_ids_, task_id)) {
    return 0;
  }

  // If |session_id_to_window_id_| is not empty, that means there are ARC
  // apps launched. Returns -1 to add the ARC app window to the hidden
  // container.
  return kParentToHiddenContainer;
}

int32_t ArcReadHandler::GetArcRestoreWindowIdForSessionId(int32_t session_id) {
  // If `session_id` doesn't exist, that means there is no ARC app restored.
  auto it = session_id_to_window_id_.find(session_id);
  return it == session_id_to_window_id_.end() ? 0 : it->second;
}

void ArcReadHandler::SetArcSessionIdForWindowId(int32_t session_id,
                                                int32_t window_id) {
  DCHECK_GT(session_id, kArcSessionIdOffsetForRestoredLaunching);
  session_id_to_window_id_[session_id] = window_id;
}

void ArcReadHandler::RemoveAppRestoreData(int32_t window_id) {
  if (!IsValidRestoreWindowId(window_id))
    return;

  auto it = window_id_to_app_id_.find(window_id);
  if (it == window_id_to_app_id_.end())
    return;

  delegate_->RemoveAppRestoreData(profile_path_, it->second, window_id);

  window_id_to_app_id_.erase(it);
}

void ArcReadHandler::UpdateWindowCandidates(int32_t task_id,
                                            int32_t restore_window_id) {
  // Go through `arc_window_candidates_`.
  auto window_it = base::ranges::find(
      arc_window_candidates_, task_id,
      [](aura::Window* window) { return window->GetProperty(kWindowIdKey); });
  if (window_it == arc_window_candidates_.end())
    return;

  // If `restore_window_id` is valid, sets the window property
  // `kRestoreWindowIdKey` and `kWindowInfoKey`.
  if (IsValidRestoreWindowId(restore_window_id)) {
    (*window_it)->SetProperty(kRestoreWindowIdKey, restore_window_id);

    // When the window was created, there was not any window info due to there
    // being no task. Apply properties to the window now that there is window
    // info.
    std::unique_ptr<WindowInfo> window_info = GetWindowInfo(restore_window_id);
    if (window_info)
      ApplyProperties(window_info.get(), *window_it);
  }

  // Remove the window from the hidden container.
  if ((*window_it)->GetProperty(kParentToHiddenContainerKey)) {
    app_restore::AppRestoreInfo::GetInstance()->OnParentWindowToValidContainer(
        *window_it);
  }

  arc_window_candidates_.erase(*window_it);
}

}  // namespace app_restore
