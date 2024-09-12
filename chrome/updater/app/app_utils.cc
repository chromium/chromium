// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/app/app_utils.h"

#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "chrome/enterprise_companion/global_constants.h"
#include "chrome/updater/constants.h"

namespace updater {

bool IsUpdaterOrCompanionApp(const std::string& app_id) {
  return base::EqualsCaseInsensitiveASCII(app_id, kUpdaterAppId) ||
         base::EqualsCaseInsensitiveASCII(
             app_id, enterprise_companion::kCompanionAppId);
}

bool ShouldUninstall(const std::vector<std::string>& app_ids,
                     int server_starts,
                     bool had_apps) {
  bool has_app = base::ranges::any_of(app_ids, [](const std::string& app_id) {
    // The updater and the companion app don't count.
    return !IsUpdaterOrCompanionApp(app_id);
  });
  return !has_app &&
         (server_starts > kMaxServerStartsBeforeFirstReg || had_apps);
}

}  // namespace updater
