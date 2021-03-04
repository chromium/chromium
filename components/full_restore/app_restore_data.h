// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_
#define COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace base {
class Value;
}

namespace full_restore {

struct AppLaunchInfo;
struct WindowInfo;

// This is the struct used by RestoreData to save both app launch parameters and
// app window information. This struct can be converted to JSON format to be
// written to the FullRestoreData file.
struct COMPONENT_EXPORT(FULL_RESTORE) AppRestoreData {
  AppRestoreData();
  explicit AppRestoreData(base::Value&& value);
  explicit AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info);

  AppRestoreData(const AppRestoreData&) = delete;
  AppRestoreData& operator=(const AppRestoreData&) = delete;

  ~AppRestoreData();

  std::unique_ptr<AppRestoreData> Clone() const;

  // Converts the struct AppRestoreData to base::Value, e.g.:
  // {
  //    "event_flag": 0,
  //    "container": 0,
  //    "disposition": 1,
  //    "display_id": "22000000",
  //    "url": "abc.com",
  //    "intent": { "action": "xx", "url": "cc.com", ... },
  //    "file_paths": { "aa.cc", "bb.h", ... },
  //    "index": 3,
  //    "desk_id": 1,
  //    "visible": true,
  //    "restored_bounds": { 0, 100, 200, 300 },
  //    "current_bounds": { 100, 200, 200, 300 },
  //    "window_state_type": 256,
  // }
  base::Value ConvertToValue() const;

  // Modifies the window's information based on |window_info|.
  void ModifyWindowInfo(const WindowInfo& window_info);

  // Clears the window's information.
  void ClearWindowInfo();

  // Gets the window information.
  std::unique_ptr<WindowInfo> GetWindowInfo() const;

  // Returns apps::mojom::WindowInfoPtr for app launch interfaces.
  apps::mojom::WindowInfoPtr GetAppWindowInfo() const;

  // App launch parameters.
  base::Optional<int32_t> event_flag;
  base::Optional<int32_t> container;
  base::Optional<int32_t> disposition;
  base::Optional<int64_t> display_id;
  base::Optional<GURL> url;
  base::Optional<apps::mojom::IntentPtr> intent;
  base::Optional<std::vector<base::FilePath>> file_paths;

  // Window's information.
  base::Optional<int32_t> activation_index;
  base::Optional<int32_t> desk_id;
  base::Optional<bool> visible_on_all_workspaces;
  base::Optional<gfx::Rect> restore_bounds;
  base::Optional<gfx::Rect> current_bounds;
  base::Optional<chromeos::WindowStateType> window_state_type;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_
