// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_

#include <map>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace aura {
class Window;
}

namespace app_restore {

// LacrosSaveHandler is a helper class for FullRestoreReadHandler to restore
// Lacros windows.
// TODO(crbug.com/1239984): Restore Lacros windows.
class COMPONENT_EXPORT(APP_RESTORE) LacrosReadHandler {
 public:
  LacrosReadHandler(const base::FilePath& profile_path);
  LacrosReadHandler(const LacrosReadHandler&) = delete;
  LacrosReadHandler& operator=(const LacrosReadHandler&) = delete;
  ~LacrosReadHandler();

  // Sets `app_id` and `window_id` to `restore_window_id_to_app_id_` to record
  // that there is a restore data for `app_id` and `window_id`.
  void AddRestoreData(const std::string& app_id, int32_t window_id);

  // Invoked when Lacros window is created. `restored_browser_session_id` is the
  // restored browser session id.
  void OnLacrosBrowserWindowAdded(aura::Window* const window,
                                  uint32_t restored_browser_session_id);

 private:
  // The user profile path for Lacros windows.
  base::FilePath profile_path_;

  // The map from the restore window id to the app id for Lacros windows.
  std::map<int32_t, std::string> restore_window_id_to_app_id_;

  // The map from the lacros window id to the restore window id for browser
  // windows.
  std::map<std::string, int32_t> lacros_window_id_to_restore_window_id_;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_
