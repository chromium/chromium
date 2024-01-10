// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
#define COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_

#include <optional>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/intent.h"
#include "components/tab_groups/tab_group_info.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

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
  std::vector<GURL> urls;
  std::optional<int32_t> active_tab_index;
  std::optional<int32_t> first_non_pinned_tab_index;
  std::optional<bool> app_type_browser;
  std::optional<std::string> app_name;
  // Represents tab groups associated with this browser instance if there are
  // any. This is only used in Desks Storage, tab groups in full restore are
  // persisted by sessions. This field is not converted to base::Value in base
  // value conversions.
  std::vector<tab_groups::TabGroupInfo> tab_group_infos;
  // Lacros only, the ID of the lacros profile that this browser uses.
  std::optional<uint64_t> lacros_profile_id;
};

}  // namespace app_restore

#endif  // COMPONENTS_APP_RESTORE_APP_LAUNCH_INFO_H_
