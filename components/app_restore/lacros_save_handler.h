// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_LACROS_SAVE_HANDLER_H_
#define COMPONENTS_APP_RESTORE_LACROS_SAVE_HANDLER_H_

#include <map>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace aura {
class Window;
}

namespace full_restore {

// LacrosSaveHandler is a helper class for FullRestoreSaveHandler to handle
// Lacros windows special cases, e.g. Lacros window id, etc.
// TODO(crbug.com/1239984):
// 1. Save app launch info for Chrome app windows opened with Lacros windows.
// 2. Save window info for Chrome app windows opened with Lacros windows.
// 3. Use the browser session id as the window id.
class COMPONENT_EXPORT(APP_RESTORE) LacrosSaveHandler {
 public:
  explicit LacrosSaveHandler(const base::FilePath& profile_path);
  LacrosSaveHandler(const LacrosSaveHandler&) = delete;
  LacrosSaveHandler& operator=(const LacrosSaveHandler&) = delete;
  ~LacrosSaveHandler();

  // Invoked when `window` is initialized.
  void OnWindowInitialized(aura::Window* window);

  // Invoked when `window` is destroyed.
  void OnWindowDestroyed(aura::Window* window);

 private:
  struct WindowData {
    std::string app_id;
    int32_t window_id;
  };

  // The primary user profile path.
  base::FilePath profile_path_;

  // `window_id_` is used to record the current used window id. When a new
  // Lacros window is created, ++window_id to generate the new window id.
  int32_t window_id_ = 0;

  // |window_candidates_| is used to record the map from the exo application id
  // to `app_id` and `window_id`. `app_id` might be changed for Chrome app
  // windows because the Lacros app id is set for all Lacros windows, and when
  // OnAppWindowAdded is called, `app_id` is modified to the Chrome app id. The
  // record is removed when the window is destroyed.
  std::map<std::string, WindowData> window_candidates_;
};

}  // namespace full_restore

#endif  // COMPONENTS_APP_RESTORE_LACROS_SAVE_HANDLER_H_
