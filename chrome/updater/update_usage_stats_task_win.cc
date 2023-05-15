// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/util/win_util.h"
#include "chrome/updater/win/win_constants.h"

namespace updater {

namespace {

bool AppUsageStatsAllowed(UpdaterScope scope, const std::wstring& app_id) {
  DWORD usagestats = 0;
  if (IsSystemInstall(scope) &&
      base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        base::StrCat({CLIENT_STATE_MEDIUM_KEY, app_id}).c_str(),
                        Wow6432(KEY_READ))
              .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
    return usagestats == 1;
  }

  if (base::win::RegKey(UpdaterScopeToHKeyRoot(scope),
                        base::StrCat({CLIENT_STATE_KEY, app_id}).c_str(),
                        Wow6432(KEY_READ))
          .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS) {
    return usagestats == 1;
  }

  return false;
}

}  // namespace

bool OtherAppUsageStatsAllowed(const std::vector<std::string>& app_ids,
                               UpdaterScope scope) {
  for (auto app_id : app_ids) {
    if (base::EqualsCaseInsensitiveASCII(app_id, kUpdaterAppId)) {
      continue;
    }

    if (AppUsageStatsAllowed(scope, base::SysUTF8ToWide(app_id))) {
      VLOG(2) << "usagestats enabled by app " << app_id;
      return true;
    }
  }

  VLOG(2) << "No app enables usagestats.";
  return false;
}

}  // namespace updater
