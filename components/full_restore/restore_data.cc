// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/full_restore/restore_data.h"

#include "components/full_restore/app_launch_info.h"

namespace full_restore {

RestoreData::RestoreData() = default;
RestoreData::~RestoreData() = default;

void RestoreData::AddAppLaunchInfo(
    std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info || !app_launch_info->id.has_value())
    return;

  const std::string app_id = app_launch_info->app_id;
  const int32_t id = app_launch_info->id.value();
  app_id_to_launch_list_[app_id][id] =
      std::make_unique<AppRestoreData>(std::move(app_launch_info));
}

}  // namespace full_restore
