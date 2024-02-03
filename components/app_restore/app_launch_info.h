// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
#define COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/app_restore/window_info.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "ui/base/window_open_disposition.h"

namespace app_restore {

// This class is the parameter for the interface SaveAppLaunchInfo, to save the
// app launch information.
struct COMPONENT_EXPORT(APP_RESTORE) AppLaunchInfo {
  AppLaunchInfo(const std::string& app_id,
                int32_t window_id,
                apps::LaunchContainer container,
                WindowOpenDisposition disposition,
                int64_t display_id,
                std::vector<base::FilePath> launch_files,
                apps::IntentPtr intent);

  AppLaunchInfo(const std::string& app_id, int32_t window_id);

  AppLaunchInfo(const std::string& app_id,
                apps::LaunchContainer container,
                WindowOpenDisposition disposition,
                int64_t display_id,
                std::vector<base::FilePath> launch_files,
                apps::IntentPtr intent);

  AppLaunchInfo(const std::string& app_id,
                int32_t event_flags,
                int32_t arc_session_id,
                int64_t display_id);

  AppLaunchInfo(const std::string& app_id,
                int32_t event_flags,
                apps::IntentPtr intent,
                int32_t arc_session_id,
                int64_t display_id);

  AppLaunchInfo(const std::string& app_id,
                const std::string& handler_id,
                std::vector<base::FilePath> launch_files);

  AppLaunchInfo(const AppLaunchInfo&) = delete;
  AppLaunchInfo& operator=(const AppLaunchInfo&) = delete;

  ~AppLaunchInfo();

  std::string app_id;
  std::optional<int32_t> window_id;

  // App launch parameters.
  std::optional<int32_t> event_flag;
  std::optional<int32_t> container;
  std::optional<int32_t> disposition;
  std::optional<GURL> override_url;
  std::optional<int32_t> arc_session_id;
  std::optional<int64_t> display_id;
  std::optional<std::string> handler_id;
  std::vector<base::FilePath> file_paths;
  apps::IntentPtr intent = nullptr;

  // Additional info for browsers.
  BrowserExtraInfo browser_extra_info;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
