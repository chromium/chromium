// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/saved_desk_test_util.h"

#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"

namespace desks_storage::saved_desk_test_util {

void AddBrowserWindow(bool is_lacros,
                      int window_id,
                      std::vector<GURL> urls,
                      app_restore::RestoreData* out_restore_data) {
  auto browser_info = std::make_unique<app_restore::AppLaunchInfo>(
      is_lacros ? app_constants::kLacrosAppId : app_constants::kChromeAppId,
      window_id);
  browser_info->browser_extra_info.urls = urls;
  out_restore_data->AddAppLaunchInfo(std::move(browser_info));
}

void AddPwaWindow(bool is_lacros,
                  int window_id,
                  std::string url,
                  app_restore::RestoreData* out_restore_data) {
  auto app_launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      is_lacros ? app_constants::kLacrosAppId : app_constants::kChromeAppId,
      window_id);

  app_launch_info->browser_extra_info.urls = {GURL(url)};
  app_launch_info->browser_extra_info.app_type_browser = true;

  out_restore_data->AddAppLaunchInfo(std::move(app_launch_info));
}

void AddGenericAppWindow(int window_id,
                         std::string app_id,
                         app_restore::RestoreData* out_restore_data) {
  auto app_launch_info =
      std::make_unique<app_restore::AppLaunchInfo>(app_id, window_id);

  out_restore_data->AddAppLaunchInfo(std::move(app_launch_info));
}

}  // namespace desks_storage::saved_desk_test_util
