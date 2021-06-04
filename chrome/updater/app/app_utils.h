// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_UTILS_H_
#define CHROME_UPDATER_APP_APP_UTILS_H_

#include <string>
#include <vector>

namespace updater {
bool ShouldUninstall(const std::vector<std::string>& app_ids,
                     int server_starts);
}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_UTILS_H_
