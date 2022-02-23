// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/update_usage_stats_task.h"

#include <string>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/sys_string_conversions.h"
#include "base/win/registry.h"
#include "base/win/windows_types.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/win/win_constants.h"
#include "chrome/updater/win/win_util.h"

namespace updater {

bool UpdateUsageStatsTask::UsageStatsAllowed(
    const std::vector<std::string>& app_ids) const {
  for (auto app_id : app_ids) {
    if (app_id == kUpdaterAppId) {
      continue;
    }
    std::wstring app_id_u16;
    DWORD usagestats = 0;
    if (base::win::RegKey(scope_ == UpdaterScope::kUser ? HKEY_CURRENT_USER
                                                        : HKEY_LOCAL_MACHINE,
                          base::StrCat({scope_ == UpdaterScope::kUser
                                            ? CLIENT_STATE_KEY
                                            : CLIENT_STATE_MEDIUM_KEY,
                                        base::SysUTF8ToWide(app_id)})
                              .c_str(),
                          Wow6432(KEY_READ))
                .ReadValueDW(L"usagestats", &usagestats) == ERROR_SUCCESS &&
        usagestats == 1) {
      return true;
    }
  }
  return false;
}

}  // namespace updater
