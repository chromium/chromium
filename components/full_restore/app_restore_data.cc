// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "components/full_restore/app_restore_data.h"

#include "components/full_restore/app_launch_info.h"

namespace full_restore {

AppRestoreData::AppRestoreData() = default;
AppRestoreData::~AppRestoreData() = default;

AppRestoreData::AppRestoreData(std::unique_ptr<AppLaunchInfo> app_launch_info) {
  if (!app_launch_info)
    return;

  if (app_launch_info->event_flag.has_value())
    event_flag = app_launch_info->event_flag.value();

  if (app_launch_info->container.has_value())
    container = app_launch_info->container.value();

  if (app_launch_info->disposition.has_value())
    disposition = app_launch_info->disposition.value();

  if (app_launch_info->display_id.has_value())
    display_id = app_launch_info->display_id.value();

  if (app_launch_info->url.has_value())
    url = std::move(app_launch_info->url.value());

  if (app_launch_info->file_paths.has_value())
    file_paths = std::move(app_launch_info->file_paths.value());

  if (app_launch_info->intent.has_value())
    intent = std::move(app_launch_info->intent.value());
}

}  // namespace full_restore
