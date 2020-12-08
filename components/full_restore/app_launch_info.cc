// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/full_restore/app_launch_info.h"

namespace full_restore {

AppLaunchInfo::AppLaunchInfo(const std::string& app_id,
                             int32_t session_id,
                             apps::mojom::LaunchContainer container,
                             WindowOpenDisposition disposition,
                             int64_t display_id,
                             std::vector<base::FilePath> launch_files,
                             apps::mojom::IntentPtr& intent)
    : app_id_(app_id),
      id_(session_id),
      container_(static_cast<int32_t>(container)),
      disposition_(static_cast<int32_t>(disposition)),
      display_id_(display_id),
      file_paths_(std::move(launch_files)),
      intent_(std::move(intent)) {}

AppLaunchInfo::~AppLaunchInfo() = default;

}  // namespace full_restore
