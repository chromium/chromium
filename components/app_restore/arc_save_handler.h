// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_ARC_SAVE_HANDLER_H_
#define COMPONENTS_APP_RESTORE_ARC_SAVE_HANDLER_H_

#include <map>
#include <memory>
#include <set>
#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"

namespace app_restore {
struct AppLaunchInfo;
struct WindowInfo;
}  // namespace app_restore

namespace aura {
class Window;
}

namespace full_restore {

// ArcSaveHandler is a helper class for FullRestoreSaveHandler to handle ARC app
// windows special cases, e.g. ARC task creation, ARC session id, etc.
class COMPONENT_EXPORT(APP_RESTORE) ArcSaveHandler {
 public:
  using AppLaunchInfoPtr = std::unique_ptr<app_restore::AppLaunchInfo>;
  using SessionIdMap =
      std::map<int32_t, std::pair<AppLaunchInfoPtr, base::TimeTicks>>;

  explicit ArcSaveHandler(const base::FilePath& profile_path);
  ArcSaveHandler(const ArcSaveHandler&) = delete;
  ArcSaveHandler& operator=(const ArcSaveHandler&) = delete;
  ~ArcSaveHandler();

  // Saves |app_launch_info| to |arc_session_id_to_app_launch_info_|, and wait
  // for the ARC task to be created.
  void SaveAppLaunchInfo(AppLaunchInfoPtr app_launch_info);

  // Saves |window_info|.
  void ModifyWindowInfo(const app_restore::WindowInfo& window_info);

  // Invoked when |window| is initialized.
  void OnWindowInitialized(aura::Window* window);

  // Invoked when |window| is destroyed.
  void OnWindowDestroyed(aura::Window* window);

  // Invoked when the task is created for an ARC app.
  void OnTaskCreated(const std::string& app_id,
                     int32_t task_id,
                     int32_t session_id);

  // Invoked when the task is destroyed for an ARC app.
  void OnTaskDestroyed(int32_t task_id);

  // Invoked when the task theme color is updated for an ARC app.
  void OnTaskThemeColorUpdated(int32_t task_id,
                               uint32_t primary_color,
                               uint32_t status_bar_color);

  // Generates the ARC session id (0 - 1,000,000,000) for ARC apps.
  int32_t GetArcSessionId();

  // Returns the app id that associates with |window|.
  std::string GetAppId(aura::Window* window);

  void set_is_connection_ready(bool is_connection_ready) {
    is_connection_ready_ = is_connection_ready;
  }

 private:
  friend class FullRestoreSaveHandlerTestApi;

  // Starts the timer to check whether a task is created for the app launching
  // (if timer isn't already running).
  void MaybeStartCheckTimer();

  // Check whether a task is created for each app launching. If not, remove the
  // app launching record.
  void CheckTasksForAppLaunching();

  // The user profile path for ARC app.
  base::FilePath profile_path_;

  int32_t session_id_ = 0;

  // Specify whether the ARC instance connection is ready.
  bool is_connection_ready_ = false;

  // The map from the ARC session id to the app launch info.
  SessionIdMap session_id_to_app_launch_info_;

  // The map from the task id to the app id. The task id is saved in the window
  // property. This map is used to find the app id when save the window info.
  std::map<int32_t, std::string> task_id_to_app_id_;

  // The map from the session id to the app id for ghost windows, so that we can
  // save the restore data, once the ghost window is created.
  std::map<int32_t, std::string> ghost_window_session_id_to_app_id_;

  // ARC app tasks could be created after the window initialized.
  // |arc_window_candidates_| is used to record those initialized ARC app
  // windows, whose tasks have not been created. Once the task for the window is
  // created, the window is removed from |arc_window_candidates_|.
  std::set<aura::Window*> arc_window_candidates_;

  // Timer used to check whether a task is created. App launching could have
  // failed. If an app is launched without a task created, the launch record
  // should be removed from |session_id_to_app_launch_info_|.
  base::RepeatingTimer check_timer_;

  base::WeakPtrFactory<ArcSaveHandler> weak_factory_{this};
};

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_ARC_SAVE_HANDLER_H_
