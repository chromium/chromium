// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_
#define COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/optional.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

namespace full_restore {

struct AppLaunchInfo;

// This is the struct used by RestoreData to save both app launch parameters and
// app window information. This struct can be converted to JSON format to be
// written to the FullRestoreData file.
//
// TODO(crbug.com/1146900): Add the interface to convert this struct to JSON
// format.
struct COMPONENT_EXPORT(FULL_RESTORE) AppRestoreData {
  AppRestoreData();
  ~AppRestoreData();

  AppRestoreData(const AppRestoreData&) = delete;
  AppRestoreData& operator=(const AppRestoreData&) = delete;

  AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info);

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
  base::Optional<gfx::Rect> restored_bounds;
  base::Optional<gfx::Rect> current_bounds;
  base::Optional<int32_t> Window_state_type;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_APP_RESTORE_DATA_H_
