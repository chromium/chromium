// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_
#define COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_

#include <map>
#include <set>

#include "base/component_export.h"
#include "base/files/file_path.h"

namespace aura {
class Window;
}

namespace app_restore {

// LacrosSaveHandler is a helper class for FullRestoreReadHandler to restore
// Lacros windows.
//
// For Lacros browser window, the restored browser session id is used as the
// restore window id. So only when the restored browser session id is received
// via mojom calls, the window can be restored. OnLacrosBrowserWindowAdded is
// called when both `window` is initialized, and the restored browser session id
// is received. So there could be 2 scenarios:
//
// 1. `window` is initialized first, then `window` is added to the hidden
// container, and OnWindowAddedToRootWindow is called to save `window` in
// `window_candidates_` to wait for the restored browser session id. When
// OnLacrosBrowserWindowAdded is called, call UpdateWindow to apply the restore
// window properties, and remove `window` from the hidden container.
//
// 2. The restored browser session id is received first, then
// OnLacrosBrowserWindowAdded is called when `window` is initialized. We have to
// wait for the OnWindowAddedToRootWindow callback, because `window`'s root is
// not set yet in the OnWindowInitialized/OnLacrosBrowserWindowAdded callback,
// and we can't remove `window` from the hidden container. When
// OnWindowAddedToRootWindow is called, `window` can be restored and removed
// from the hidden container.
//
// TODO(crbug.com/1239984): Restore Lacros windows.
class COMPONENT_EXPORT(APP_RESTORE) LacrosReadHandler {
 public:
  LacrosReadHandler(const base::FilePath& profile_path);
  LacrosReadHandler(const LacrosReadHandler&) = delete;
  LacrosReadHandler& operator=(const LacrosReadHandler&) = delete;
  ~LacrosReadHandler();

  // Invoked when `window` is initialized.
  void OnWindowInitialized(aura::Window* window);

  // Sets `app_id` and `window_id` to `restore_window_id_to_app_id_` to record
  // that there is a restore data for `app_id` and `window_id`.
  void AddRestoreData(const std::string& app_id, int32_t window_id);

  // Invoked when an Chrome app Lacros window is created. `app_id` is the
  // AppService id, and `window_id` is the wayland app_id property for the
  // window.
  void OnAppWindowAdded(const std::string& app_id,
                        const std::string& lacros_window_id);

  // Invoked when an Chrome app Lacros window is removed. `app_id` is the
  // AppService id, and `window_id` is the wayland app_id property for the
  // window.
  void OnAppWindowRemoved(const std::string& app_id,
                          const std::string& lacros_window_id);

  // Invoked when `window` is added to the root window.
  void OnWindowAddedToRootWindow(aura::Window* window);

  // Invoked when `window` is destroyed.
  void OnWindowDestroyed(aura::Window* window);

  // Returns the restore window id for the Lacros window with
  // `lacros_window_id`.
  int32_t GetLacrosRestoreWindowId(const std::string& lacros_window_id) const;

 private:
  struct WindowData {
    std::string app_id;
    int32_t restore_window_id = -1;
  };

  // Sets `app_id` and `restore_window_id` for `window` in
  // `window_to_window_data_`. If there is no restore data for
  // `restore_window_id`, `app_id` won't be set to skip setting the restore data
  // for `window`.
  void SetWindowData(aura::Window* const window,
                     const std::string& app_id,
                     int32_t restore_window_id);

  // Sets `kRestoreWindowIdKey` and `kWindowInfoKey` to restore and remove
  // `window from the hidden container`.
  void UpdateWindow(aura::Window* const window);

  // The user profile path for Lacros windows.
  base::FilePath profile_path_;

  // The map from the restore window id to the app id for Lacros windows.
  std::map<int32_t, std::string> restore_window_id_to_app_id_;

  // The map from the window to the app id and the restore window id.
  std::map<aura::Window*, WindowData> window_to_window_data_;

  // The mojom call to forward the restore window id could be received later
  // than the OnWindowAddedToRootWindow callback. So add windows to
  // `window_candidates_` to record window candidates. Once the restore window
  // id is received, the window can be restored and removed from the hidden
  // container.
  std::set<aura::Window*> window_candidates_;

  // The map from the lacros window id to the app id for Chrome app windows.
  std::map<std::string, std::string> lacros_window_id_to_app_id_;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_LACROS_READ_HANDLER_H_
