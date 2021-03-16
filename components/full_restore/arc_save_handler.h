// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_ARC_SAVE_HANDLER_H_
#define COMPONENTS_FULL_RESTORE_ARC_SAVE_HANDLER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace aura {
class Window;
}

namespace full_restore {

struct AppLaunchInfo;

// ArcSaveHandler is a helper class for FullRestoreSaveHandler to handle ARC app
// windows special cases, e.g. ARC task creation, ARC session id, etc.
class COMPONENT_EXPORT(FULL_RESTORE) ArcSaveHandler {
 public:
  using AppLaunchInfoPtr = std::unique_ptr<AppLaunchInfo>;

  explicit ArcSaveHandler(const base::FilePath& profile_path);
  ArcSaveHandler(const ArcSaveHandler&) = delete;
  ArcSaveHandler& operator=(const ArcSaveHandler&) = delete;
  ~ArcSaveHandler();

  // Saves |app_launch_info| to |arc_session_id_to_app_launch_info_|, and wait
  // for the ARC task to be created.
  void SaveAppLaunchInfo(AppLaunchInfoPtr app_launch_info);

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

  // Generates the ARC session id (0 - 1,000,000,000) for ARC apps.
  int32_t GetArcSessionId();

  const std::map<int32_t, std::string>& GetArcTaskIdMapForTesting() const {
    return task_id_to_app_id_;
  }

 private:
  // The user profile path for ARC app.
  base::FilePath profile_path_;

  int32_t session_id_ = 0;

  // The map from the ARC session id to the app launch info.
  std::map<int32_t, AppLaunchInfoPtr> session_id_to_app_launch_info_;

  // The map from the task id to the app id. The task id is saved in the window
  // property. This map is used to find the app id when save the window info.
  std::map<int32_t, std::string> task_id_to_app_id_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_ARC_SAVE_HANDLER_H_
