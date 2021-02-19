// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_
#define COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_

#include <map>
#include <memory>

#include "base/component_export.h"
#include "components/full_restore/app_restore_data.h"

namespace base {
class Value;
}

namespace full_restore {

struct AppLaunchInfo;
struct WindowInfo;

// This class is responsible for saving all app launch and app windows
// information. It can be converted to JSON format to be written to the
// FullRestoreData file.
//
// TODO(crbug.com/1146900):
// 1. Add the interface to modify LaunchAndWindowInfo when the window
// information is updated.
// 2. Add the interface to remove LaunchAndWindowInfo.
class COMPONENT_EXPORT(FULL_RESTORE) RestoreData {
 public:
  // Map from a window id to AppRestoreData.
  using LaunchList = std::map<int, std::unique_ptr<AppRestoreData>>;

  // Map from an app id to LaunchList.
  using AppIdToLaunchList = std::map<std::string, LaunchList>;

  RestoreData();
  explicit RestoreData(std::unique_ptr<base::Value> restore_data_value);

  ~RestoreData();

  RestoreData(const RestoreData&) = delete;
  RestoreData& operator=(const RestoreData&) = delete;

  std::unique_ptr<RestoreData> Clone() const;

  // Converts |app_id_to_launch_list_| to base::Value, e.g.:
  // {
  //   "odknhmnlageboeamepcngndbggdpaobj":    // app_id
  //     {
  //       "403":                             // window_id
  //         {
  //           "container": 0,
  //           "disposition": 1,
  //           "display_id": "22000000",
  //           "index": 3,
  //           "desk_id": 1,
  //           "restored_bounds": { 0, 100, 200, 300 },
  //           "current_bounds": { 100, 200, 200, 300 },
  //           "window_state_type": 256,
  //         },
  //     },
  //   "pjibgclleladliembfgfagdaldikeohf":    // app_id
  //     {
  //       "413":                             // window_id
  //         {
  //           "container": 0,
  //           "disposition": 3,
  //           "display_id": "22000000",
  //           ...
  //         },
  //       "415":                             // window_id
  //         {
  //           ...
  //         },
  //     },
  // }
  base::Value ConvertToValue() const;

  // Adds |app_launch_info| to |app_id_to_launch_list_|.
  void AddAppLaunchInfo(std::unique_ptr<AppLaunchInfo> app_launch_info);

  // Modifies the window's information based on |window_info| for the window
  // with |window_id| of the app with |app_id|.
  void ModifyWindowInfo(const std::string& app_id,
                        int32_t window_id,
                        const WindowInfo& window_info);

  // Modifies |chrome_app_id_to_current_window_id_| to set the next restore
  // window id for the given |app_id|.
  //
  // If there is only 1 window for |app_id|, its window id is set as the
  // restore window id to restore window properties when there is a window
  // created for |app_id|.
  //
  // If there is more than 1 window for |app_id|, we can't know which window is
  // for which launching, so activation_index for all windows are set as
  // INT32_MIN to send all windows to the background. The first record in
  // LaunchList is set as the restore window id for |app_id|.
  void SetNextRestoreWindowIdForChromeApp(const std::string& app_id);

  // Removes a AppRestoreData with |window_id| for |app_id|.
  void RemoveAppRestoreData(const std::string& app_id, int window_id);

  // Removes the launch list for |app_id|.
  void RemoveApp(const std::string& app_id);

  // Gets the window information with |window_id| for |app_id|.
  std::unique_ptr<WindowInfo> GetWindowInfo(const std::string& app_id,
                                            int window_id);

  // Fetches the restore window id from the restore data for the given chrome
  // app |app_id|. |app_id| should be a Chrome app id.
  //
  // If there is only 1 window for |app_id|, return its window id, and remove
  // the record of |app_id| in |chrome_app_id_to_current_window_id_|, so that
  // when there are more windows created for |app_id|, FetchRestoreWindowId
  // returns 0, and we know they are not the restored window, but launched by
  // the user.
  //
  // If there is more than 1 window for |app_id|, returns the window id saved in
  // |chrome_app_id_to_current_window_id_|, then modify
  // |chrome_app_id_to_current_window_id_| to set the next restore window id.
  //
  // For example,
  // app_id: 'aa' {window id: 1};
  // app_id: 'bb' {window id: 11, 12, 13};
  // chrome_app_id_to_current_window_id_: 'aa': 1 'bb': 11
  //
  // FetchRestoreWindowId('aa') return 1.
  // Then chrome_app_id_to_current_window_id_: 'bb': 11
  // FetchRestoreWindowId('aa') return 0.
  //
  // FetchRestoreWindowId('bb') return 11.
  // Then chrome_app_id_to_current_window_id_: 'bb': 12
  // FetchRestoreWindowId('bb') return 12.
  // Then chrome_app_id_to_current_window_id_: 'bb': 13
  // FetchRestoreWindowId('bb') return 13.
  // Then chrome_app_id_to_current_window_id_ is empty.
  // FetchRestoreWindowId('bb') return 0.
  int32_t FetchRestoreWindowId(const std::string& app_id);

  const AppIdToLaunchList& app_id_to_launch_list() const {
    return app_id_to_launch_list_;
  }

 private:
  AppIdToLaunchList app_id_to_launch_list_;

  // Saves the next restore window_id to be handled for each chrome app.
  std::map<std::string, int> chrome_app_id_to_current_window_id_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_
