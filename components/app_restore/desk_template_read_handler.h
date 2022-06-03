// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_

#include <memory>
#include <string>

#include "base/component_export.h"
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

  RestoreData* restore_data() { return restore_data_.get(); }
  ArcReadHandler* arc_read_handler() { return arc_read_handler_.get(); }

  // Sets the `restore_data` for a launch session. `restore_data` can be
  // nullptr, which signifies that a launch session is over. Creates
  // `arc_read_handler_` if necessary, which is a helper class for dealing with
  // ARC apps.
  void SetRestoreData(std::unique_ptr<RestoreData> restore_data);

  // Gets the window information for `restore_window_id`.
  std::unique_ptr<WindowInfo> GetWindowInfo(int restore_window_id);

  // Fetches the restore id for the window from RestoreData for the given
  // `app_id`. `app_id` should be a Chrome app id.
  int32_t FetchRestoreWindowId(const std::string& app_id);

  // Modifies the `restore_data_` to set the next restore window id for the
  // chrome app with `app_id`.
  void SetNextRestoreWindowIdForChromeApp(const std::string& app_id);

  // Generates the ARC session id (1,000,000,001 - INT_MAX) for restored ARC
  // apps.
  int32_t GetArcSessionId();

  // Sets `arc_session_id` for `window_id`. `arc session id` is assigned when
  // ARC apps are restored.
  void SetArcSessionIdForWindowId(int32_t arc_session_id, int32_t window_id);

  // Returns the restore window id for the ARC app's `task_id`.
  int32_t GetArcRestoreWindowIdForTaskId(int32_t task_id);

  // Returns the restore window id for the ARC app's `session_id`.
  int32_t GetArcRestoreWindowIdForSessionId(int32_t session_id);

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
  // The restore data read from desk storage. Empty when no launch is underway.
  std::unique_ptr<RestoreData> restore_data_;

  // Helper that is created if an ARC app is launched. This class contains some
  // ARC specific logic needed to launch ARC apps.
  std::unique_ptr<ArcReadHandler> arc_read_handler_;

  base::ScopedObservation<aura::Env, aura::EnvObserver> env_observer_{this};

  base::ScopedMultiSourceObservation<aura::Window, aura::WindowObserver>
      observed_windows_{this};

  base::ScopedObservation<app_restore::AppRestoreArcInfo,
                          app_restore::AppRestoreArcInfo::Observer>
      arc_info_observer_{this};
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_DESK_TEMPLATE_READ_HANDLER_H_
