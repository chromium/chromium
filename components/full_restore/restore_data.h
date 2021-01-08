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

  // Add |app_launch_info| to |app_id_to_launch_list_|.
  void AddAppLaunchInfo(std::unique_ptr<AppLaunchInfo> app_launch_info);

  // Modify the window's information based on |window_info| for the window with
  // |window_id| of the app with |app_id|.
  void ModifyWindowInfo(const std::string& app_id,
                        int32_t window_id,
                        const WindowInfo& window_info);

  // Remove a AppRestoreData with |window_id| for |app_id|.
  void RemoveAppRestoreData(const std::string& app_id, int window_id);

  // Remove the launch list for |app_id|.
  void RemoveApp(const std::string& app_id);

  const AppIdToLaunchList& app_id_to_launch_list() const {
    return app_id_to_launch_list_;
  }

 private:
  AppIdToLaunchList app_id_to_launch_list_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_RESTORE_DATA_H_
