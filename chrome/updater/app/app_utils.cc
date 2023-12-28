// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_utils.h"

#include <algorithm>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "chrome/updater/constants.h"

namespace updater {

bool ShouldUninstall(const std::vector<std::string>& app_ids,
                     int server_starts,
                     bool had_apps) {
  bool has_app = std::any_of(
      app_ids.begin(), app_ids.end(), [](const std::string& app_id) {
        // The updater itself doesn't count.
        return !base::EqualsCaseInsensitiveASCII(app_id, kUpdaterAppId);
      });
  return !has_app &&
         (server_starts > kMaxServerStartsBeforeFirstReg || had_apps);
}

}  // namespace updater
