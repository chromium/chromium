// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULL_RESTORE_APP_LAUNCH_INFO_H_
#define COMPONENTS_FULL_RESTORE_APP_LAUNCH_INFO_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/optional.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace full_restore {

// This class is the parameter for the interface SaveAppLaunchInfo, to save the
// app launch information.
class COMPONENT_EXPORT(FULL_RESTORE) AppLaunchInfo {
 public:
  AppLaunchInfo(const std::string& app_id,
                int32_t session_id,
                apps::mojom::LaunchContainer container,
                WindowOpenDisposition disposition,
                int64_t display_id,
                std::vector<base::FilePath> launch_files,
                apps::mojom::IntentPtr& intent);
  ~AppLaunchInfo();

  AppLaunchInfo(const AppLaunchInfo&) = delete;
  AppLaunchInfo& operator=(const AppLaunchInfo&) = delete;

 private:
  const std::string app_id_;
  const base::Optional<int32_t> id_;
  const base::Optional<int32_t> event_flag_;
  const base::Optional<int32_t> container_;
  const base::Optional<int32_t> disposition_;
  const base::Optional<int64_t> display_id_;
  const base::Optional<GURL> url_;
  const base::Optional<std::vector<base::FilePath>> file_paths_;
  const base::Optional<apps::mojom::IntentPtr> intent_;
};

}  // namespace full_restore

#endif  // COMPONENTS_FULL_RESTORE_APP_LAUNCH_INFO_H_
