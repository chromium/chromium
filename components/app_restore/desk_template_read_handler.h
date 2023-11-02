// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "components/app_restore/app_restore_arc_info.h"
#include "components/app_restore/arc_read_handler.h"
#include "ui/aura/env.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace app_restore {

struct AppLaunchInfo;
class RestoreData;
struct WindowInfo;

// DeskTemplateReadHandler is responsible for receiving `RestoreData` from desks
// storage. It keeps a copy of restore data during a desk template launch and
// provides APIs for reading window info of the launched applications.
class COMPONENT_EXPORT(APP_RESTORE) DeskTemplateReadHandler
    : public aura::EnvObserver,
      public aura::WindowObserver,
      public ArcReadHandler::Delegate,
      public AppRestoreArcInfo::Observer {
 public:
  DeskTemplateReadHandler();
  DeskTemplateReadHandler(const DeskTemplateReadHandler&) = delete;
  DeskTemplateReadHandler& operator=(const DeskTemplateReadHandler&) = delete;
  ~DeskTemplateReadHandler() override;

  static DeskTemplateReadHandler* Get();

  // Returns the ARC read handler for the launch associated to the window
  // `restore_window_id`. Returns nullptr if the launch is unknown.
  ArcReadHandler* GetArcReadHandlerForWindow(int32_t restore_window_id);

  // Sets the `restore_data` for the launch identified by `launch_id`. Creates
  // `arc_read_handler_` if necessary, which is a helper class for dealing with
  // ARC apps.
  void SetRestoreData(int32_t launch_id,
                      std::unique_ptr<RestoreData> restore_data);

  // Returns restore data for the launch associated to the window
  // `restore_window_id`. Returns nullptr if the launch is unknown.
  RestoreData* GetRestoreDataForWindow(int32_t restore_window_id);

  // Clears restore data for the launch identified by `launch_id`.
  void ClearRestoreData(int32_t launch_id);

  // Gets the window information for `restore_window_id`.
  std::unique_ptr<WindowInfo> GetWindowInfo(int32_t restore_window_id);

  // Fetches the restore id for the window from RestoreData for the given
  // `app_id`. `app_id` should be a Chrome app id.
  int32_t FetchRestoreWindowId(const std::string& app_id);

  // Modifies the `restore_data_` to set the next restore window id for the
  // chrome app with `app_id`.
  void SetNextRestoreWindowIdForChromeApp(const std::string& app_id);

  // Sets `arc_session_id` for `window_id`. `arc_session_id` is assigned when
  // ARC apps are restored.
  void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id);
  // Same as above, but for `launch_id`.
  void SetLaunchIdForArcSessionId(int32_t arc_session_id, int32_t launch_id);

  // Returns the restore window id for the ARC app's `task_id`. Returns 0 if the
  // task does not belong to a desk template launch.
  int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

  // Returns the restore window id for the ARC app's `session_id`. Returns 0 if
  // the session does not belong to a desk template launch.
  int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

  // Returns true if `session_id` is known to desk templates.
  bool IsKnownArcSessionId(int32_t session_id) const;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;

  // ArcReadHandler::Delegate:
  std::unique_ptr<AppLaunchInfo> GetAppLaunchInfo(
      const base::FilePath& profile_path,
      const std::string& app_id,
      int32_t restore_window_id) override;
  std::unique_ptr<WindowInfo> GetWindowInfo(const base::FilePath& profile_path,
                                            const std::string& app_id,
                                            int32_t restore_window_id) override;
  void RemoveAppRestoreData(const base::FilePath& profile_path,
                            const std::string& app_id,
                            int32_t restore_window_id) override;

  // AppRestoreArcInfo::Observer:
  void OnTaskCreated(const std::string& app_id,
                     int32_t task_id,
                     int32_t session_id) override;
  void OnTaskDestroyed(int32_t task_id) override;

 private:
  // Returns the launch id that `arc_session_id` is associated with, or 0.
  int32_t GetLaunchIdForArcSessionId(int32_t arc_session_id);

  // Returns the launch id that `restore_window_id` is associated with, or 0.
  int32_t GetLaunchIdForRestoreWindowId(int32_t restore_window_id);

  // Returns the arc read handler associated with `launch_id`, or nullptr.
  ArcReadHandler* GetArcReadHandlerForLaunch(int32_t launch_id);

  // Returns the most recent launch that has `app_id`.
  RestoreData* GetMostRecentRestoreDataForApp(const std::string& app_id);

  // Maps launch id to restore data.
  base::flat_map<int32_t, std::unique_ptr<RestoreData>> restore_data_;

  // Maps launch id to helpers with logic specific to launching ARC apps.
  base::flat_map<int32_t, std::unique_ptr<ArcReadHandler>> arc_read_handler_;

  // Mapping ARC session id to launch id.
  base::flat_map<int32_t, int32_t> session_id_to_launch_id_;
  // Mapping ARC task id to launch id.
  base::flat_map<int32_t, int32_t> task_id_to_launch_id_;

  // Maps restore window id to launch id.
  base::flat_map<int32_t, int32_t> restore_window_id_to_launch_id_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::ScopedObservation<app_restore::AppRestoreArcInfo,
                          app_restore::AppRestoreArcInfo::Observer>
      arc_info_observer_{this};
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
