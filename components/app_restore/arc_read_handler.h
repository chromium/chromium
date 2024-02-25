// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_ARC_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_ARC_READ_HANDLER_H_

#include <map>
#include <set>
#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "components/app_restore/app_restore_utils.h"

namespace aura {
class Window;
}

namespace full_restore {
class FullRestoreReadHandlerTestApi;
}

namespace app_restore {
struct AppLaunchInfo;
struct WindowInfo;

// ArcReadHandler is a helper class for a Delegate to read the app and window
// info of ARC apps, which have special cases, e.g. ARC task creation, ARC
// session id, etc.
//
// Android is responsible for restoring ARC window bounds, so full restore won't
// restore window bounds, but restore activation_index, desk_id, window_state,
// etc, when the widget has initialized by calling OnWidgetInitialized. For
// ghost windows, when the ghost window is created, all window properties are
// restored by ghost window.
//
// Task id is saved as the window id. So only when the task id is received, the
// window can be restored. Task id and window init are two separate threads, so
// there could be 2 scenarios:
// 1. Window is initialized first, then `window` is added to the hidden
// container, and saved in `arc_window_candidates_`. When the task is created,
// OnTaskCreated callback applies the restore window properties, and remove the
// window from the hidden container.
// 2. Task is created first, then Window is initialized. So before `window` is
// initialized, ModifyWidgetParams is called to apply the restore window
// properties, and when `window` is initialized, it can be restored directly.
class COMPONENT_EXPORT(APP_RESTORE) ArcReadHandler {
 public:
  // A delegate class which allows an owner of ArcReadHandler to have some
  // specific behavior.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Gets the app launch information from `profile_path` for `app_id` and
    // `restore_window_id`.
    virtual std::unique_ptr<AppLaunchInfo> GetAppLaunchInfo(
        const base::FilePath& profile_path,
        const std::string& app_id,
        int32_t restore_window_id) = 0;

    // Gets the window information from `profile_path` for `app_id` and
    // `restore_window_id`.
    virtual std::unique_ptr<WindowInfo> GetWindowInfo(
        const base::FilePath& profile_path,
        const std::string& app_id,
        int32_t restore_window_id) = 0;

    // Removes AppRestoreData from `profile_path` for `app_id` and
    // `restore_window_id`.
    virtual void RemoveAppRestoreData(const base::FilePath& profile_path,
                                      const std::string& app_id,
                                      int32_t restore_window_id) = 0;
  };

  ArcReadHandler(const base::FilePath& profile_path, Delegate* delegate);
  ArcReadHandler(const ArcReadHandler&) = delete;
  ArcReadHandler& operator=(const ArcReadHandler&) = delete;
  ~ArcReadHandler();

  // Sets |app_id| and |window_id| to |window_id_to_app_id_| to record that
  // there is a restore data for |app_id| and |window_id|.
  void AddRestoreData(const std::string& app_id, int32_t window_id);

  // Add |window| to |arc_window_candidates_|.
  void AddArcWindowCandidate(aura::Window* window);

  // Invoked when |window| is destroyed.
  void OnWindowDestroyed(aura::Window* window);

  // Invoked when the task is created for an ARC app.
  void OnTaskCreated(const std::string& app_id,
                     int32_t task_id,
                     int32_t session_id);

  // Invoked when the task is destroyed for an ARC app.
  void OnTaskDestroyed(int32_t task_id);

  // Returns true if there is restore data for |window_id|, otherwise returns
  // false.
  bool HasRestoreData(int32_t window_id);

  // Gets the ARC app launch information from the full restore file for `app_id`
  // and `session_id`.
  std::unique_ptr<AppLaunchInfo> GetArcAppLaunchInfo(const std::string& app_id,
                                                     int32_t session_id);

  // Gets the window information for |restore_window_id|.
  std::unique_ptr<WindowInfo> GetWindowInfo(int32_t restore_window_id);

  // Returns the restore window id for the ARC app's |task_id|.
  int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

  // Returns the restore window id for the ARC app's `session_id`.
  int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

  // Sets |session_id| for |window_id| to |session_id_to_window_id_|.
  // |session_id| is assigned when ARC apps are restored.
  void SetArcSessionIdForWindowId(int32_t session_id, int32_t window_id);

 private:
  friend class full_restore::FullRestoreReadHandlerTestApi;

  // Removes AppRestoreData for |restore_window_id|.
  void RemoveAppRestoreData(int32_t restore_window_id);

  // Finds the window from `arc_window_candidates_` for `task_id`, and remove
  // the window from `arc_window_candidates_`.
  void UpdateWindowCandidates(int32_t task_id, int32_t restore_window_id);

  // The user profile path for ARC app windows.
  base::FilePath profile_path_;

  // The map from the window id to the app id for ARC app windows. The window id
  // is saved in the window property |kRestoreWindowIdKey|.
  std::map<int32_t, std::string> window_id_to_app_id_;

  // The map from the arc session id to the window id.
  std::map<int32_t, int32_t> session_id_to_window_id_;

  // The map from the arc task id to the window id.
  std::map<int32_t, int32_t> task_id_to_window_id_;

  // ARC app tasks could be created after the window initialized.
  // |arc_window_candidates_| is used to record those initialized ARC app
  // windows, whose tasks have not been created. Once the task for the window is
  // created, the window is removed from |arc_window_candidates_|.
  std::set<raw_ptr<aura::Window, SetExperimental>> arc_window_candidates_;

  // ARC app tasks could be created before the window initialized.
  // `not_restored_task_ids_` is used to record tasks not created by the restore
  // process. Once the window is created for the task, the window can be removed
  // from the hidden container.
  std::set<int32_t> not_restored_task_ids_;

  raw_ptr<Delegate> delegate_;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_ARC_READ_HANDLER_H_
