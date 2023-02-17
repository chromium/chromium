// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/app_restore/arc_save_handler.h"

#include "base/containers/contains.h"
#include "base/ranges/algorithm.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_info.h"
#include "components/app_restore/app_restore_utils.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/app_restore/window_info.h"
#include "components/app_restore/window_properties.h"
#include "ui/aura/window.h"

namespace full_restore {

namespace {

// Repeat timer interval between each checking that whether a task is created
// for each app launching.
constexpr base::TimeDelta kCheckCycleInterval = base::Seconds(600);

}  // namespace

ArcSaveHandler::ArcSaveHandler(const base::FilePath& profile_path)
    : profile_path_(profile_path) {}

ArcSaveHandler::~ArcSaveHandler() = default;

void ArcSaveHandler::SaveAppLaunchInfo(AppLaunchInfoPtr app_launch_info) {
  DCHECK(app_launch_info->arc_session_id.has_value());

  // Save |app_launch_info| to |session_id_to_app_launch_info_|, and wait for
  // the ARC task to be created.
  int32_t session_id = app_launch_info->arc_session_id.value();

  // If the ghost window has been created for `session_id`, and the launch info
  // hasn't been added yet, add `app_launch_info` to the restore data.
  if (base::Contains(ghost_window_session_id_to_app_id_, session_id)) {
    if (!FullRestoreSaveHandler::GetInstance()->HasAppRestoreData(
            profile_path_, app_launch_info->app_id, session_id)) {
      app_launch_info->window_id = session_id;
      FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
          profile_path_, std::move(app_launch_info));

      // Go through `arc_window_candidates_`. If the window for `session_id` has
      // been created, call OnAppLaunched to save the window info.
      auto window_it = base::ranges::find(
          arc_window_candidates_, session_id, [](aura::Window* window) {
            return window->GetProperty(app_restore::kGhostWindowSessionIdKey);
          });
      if (window_it != arc_window_candidates_.end()) {
        app_restore::AppRestoreInfo::GetInstance()->OnAppLaunched(*window_it);
        arc_window_candidates_.erase(*window_it);
      }
    }
    return;
  }

  session_id_to_app_launch_info_[session_id] =
      std::make_pair(std::move(app_launch_info), base::TimeTicks::Now());

  MaybeStartCheckTimer();
}

void ArcSaveHandler::ModifyWindowInfo(
    const app_restore::WindowInfo& window_info) {
  aura::Window* window = window_info.window;

  int32_t task_id = window->GetProperty(app_restore::kWindowIdKey);
  auto task_it = task_id_to_app_id_.find(task_id);
  if (task_it != task_id_to_app_id_.end()) {
    FullRestoreSaveHandler::GetInstance()->ModifyWindowInfo(
        profile_path_, task_it->second, task_id, window_info);
    return;
  }

  // For the ghost window, modify the window info with `session_id` as the
  // window id.
  int32_t session_id =
      window->GetProperty(app_restore::kGhostWindowSessionIdKey);
  auto it = ghost_window_session_id_to_app_id_.find(session_id);
  if (it != ghost_window_session_id_to_app_id_.end()) {
    FullRestoreSaveHandler::GetInstance()->ModifyWindowInfo(
        profile_path_, it->second, session_id, window_info);
  }
}

void ArcSaveHandler::OnWindowInitialized(aura::Window* window) {
  int32_t task_id = window->GetProperty(app_restore::kWindowIdKey);
  if (!base::Contains(task_id_to_app_id_, task_id)) {
    // Check `session_id` to see whether this is a ghost window.
    int32_t session_id =
        window->GetProperty(app_restore::kGhostWindowSessionIdKey);
    if (session_id < app_restore::kArcSessionIdOffsetForRestoredLaunching) {
      // If the task hasn't been created, and this is not a ghost window, add
      // `window` to `arc_window_candidates_` to wait for the task to be
      // created.
      arc_window_candidates_.insert(window);
      return;
    }

    // Save `session_id` for the ghost window, to wait for the task created to
    // replace the window id with the task id in the restore data.
    const std::string* app_id = window->GetProperty(app_restore::kAppIdKey);
    DCHECK(app_id);
    ghost_window_session_id_to_app_id_[session_id] = *app_id;

    auto it = session_id_to_app_launch_info_.find(session_id);
    if (it == session_id_to_app_launch_info_.end()) {
      arc_window_candidates_.insert(window);
      return;
    }

    // If there is `app_launch_info`, add it to the restore data using
    // `session_id` as `window_id`.
    auto app_launch_info = std::move(it->second.first);
    session_id_to_app_launch_info_.erase(it);
    if (session_id_to_app_launch_info_.empty())
      check_timer_.Stop();

    app_launch_info->window_id = session_id;
    FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
        profile_path_, std::move(app_launch_info));
    return;
  }

  // If the task has been created, call OnAppLaunched to save the window
  // information.
  app_restore::AppRestoreInfo::GetInstance()->OnAppLaunched(window);
}

void ArcSaveHandler::OnWindowDestroyed(aura::Window* window) {
  arc_window_candidates_.erase(window);

  int32_t task_id = window->GetProperty(app_restore::kWindowIdKey);

  auto task_it = task_id_to_app_id_.find(task_id);
  if (task_it != task_id_to_app_id_.end()) {
    // Wait for the task to be destroyed to remove the full restore data for
    // the task. Don't remove the window info, because it might affect the ghost
    // window creating due to no window bounds. Send the window to background.
    if (is_connection_ready_) {
      FullRestoreSaveHandler::GetInstance()->SendWindowToBackground(
          profile_path_, task_it->second, task_id);
    }
    return;
  }

  // If the ghost window has been created for `session_id`, remove
  // `app_launch_info` from the restore data with `session_id` as the window id.
  int32_t session_id =
      window->GetProperty(app_restore::kGhostWindowSessionIdKey);
  auto it = ghost_window_session_id_to_app_id_.find(session_id);
  if (it != ghost_window_session_id_to_app_id_.end()) {
    // For ghost windows, we don't need to wait for OnTaskDestroyed, so remove
    // AppRestoreData for `session_id`.
    FullRestoreSaveHandler::GetInstance()->RemoveAppRestoreData(
        profile_path_, it->second, session_id);
    ghost_window_session_id_to_app_id_.erase(it);
  }
}

void ArcSaveHandler::OnTaskCreated(const std::string& app_id,
                                   int32_t task_id,
                                   int32_t session_id) {
  auto it = session_id_to_app_launch_info_.find(session_id);
  if (it == session_id_to_app_launch_info_.end()) {
    auto session_it = ghost_window_session_id_to_app_id_.find(session_id);
    if (session_it == ghost_window_session_id_to_app_id_.end())
      return;

    // For the ghost window, modify the window id from `session_id` to
    // `task_id`.
    FullRestoreSaveHandler::GetInstance()->ModifyWindowId(
        profile_path_, session_it->second, session_id, task_id);
    task_id_to_app_id_[task_id] = session_it->second;
    ghost_window_session_id_to_app_id_.erase(session_it);
    return;
  }

  auto app_launch_info = std::move(it->second.first);
  task_id_to_app_id_[task_id] = app_launch_info->app_id;
  session_id_to_app_launch_info_.erase(it);
  if (session_id_to_app_launch_info_.empty())
    check_timer_.Stop();

  app_launch_info->window_id = task_id;
  FullRestoreSaveHandler::GetInstance()->AddAppLaunchInfo(
      profile_path_, std::move(app_launch_info));

  // Go through |arc_window_candidates_|. If the window for |task_id| has been
  // created, call OnAppLaunched to save the window info.
  auto window_it = base::ranges::find(
      arc_window_candidates_, task_id, [](aura::Window* window) {
        return window->GetProperty(app_restore::kWindowIdKey);
      });
  if (window_it != arc_window_candidates_.end()) {
    app_restore::AppRestoreInfo::GetInstance()->OnAppLaunched(*window_it);
    arc_window_candidates_.erase(*window_it);
  }
}

void ArcSaveHandler::OnTaskDestroyed(int32_t task_id) {
  auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->RemoveAppRestoreData(
      profile_path_, it->second, task_id);

  task_id_to_app_id_.erase(task_id);
}

void ArcSaveHandler::OnArcPlayStoreEnabledChanged(bool enabled) {
  if (!enabled)
    task_id_to_app_id_.clear();
}

void ArcSaveHandler::OnTaskThemeColorUpdated(int32_t task_id,
                                             uint32_t primary_color,
                                             uint32_t status_bar_color) {
  auto it = task_id_to_app_id_.find(task_id);
  if (it == task_id_to_app_id_.end())
    return;

  FullRestoreSaveHandler::GetInstance()->ModifyThemeColor(
      profile_path_, it->second, task_id, primary_color, status_bar_color);
}

int32_t ArcSaveHandler::GetArcSessionId() {
  if (session_id_ >= app_restore::kArcSessionIdOffsetForRestoredLaunching) {
    LOG(WARNING) << "ARC session id is too large: " << session_id_;
    session_id_ = 0;
  }

  return ++session_id_;
}

std::string ArcSaveHandler::GetAppId(aura::Window* window) {
  // First check |task_id_to_app_id_| to see if we can find app id there.
  const int32_t task_id = window->GetProperty(app_restore::kWindowIdKey);
  auto task_iter = task_id_to_app_id_.find(task_id);
  if (task_iter != task_id_to_app_id_.end())
    return task_iter->second;

  // If not, try to search in |ghost_window_session_id_to_app_id_|.
  const int32_t session_id =
      window->GetProperty(app_restore::kGhostWindowSessionIdKey);
  auto ghost_iter = ghost_window_session_id_to_app_id_.find(session_id);
  return ghost_iter != ghost_window_session_id_to_app_id_.end()
             ? ghost_iter->second
             : std::string();
}

void ArcSaveHandler::MaybeStartCheckTimer() {
  if (!check_timer_.IsRunning()) {
    check_timer_.Start(
        FROM_HERE, kCheckCycleInterval,
        base::BindRepeating(&ArcSaveHandler::CheckTasksForAppLaunching,
                            weak_factory_.GetWeakPtr()));
  }
}

void ArcSaveHandler::CheckTasksForAppLaunching() {
  std::set<int32_t> session_ids;
  for (const auto& it : session_id_to_app_launch_info_) {
    base::TimeDelta time_delta = base::TimeTicks::Now() - it.second.second;
    if (time_delta > kCheckCycleInterval)
      session_ids.insert(it.first);
  }

  for (auto id : session_ids)
    session_id_to_app_launch_info_.erase(id);

  if (session_id_to_app_launch_info_.empty())
    check_timer_.Stop();
}

}  // namespace full_restore
