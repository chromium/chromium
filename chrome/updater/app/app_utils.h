// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_APP_APP_UTILS_H_
#define CHROME_UPDATER_APP_APP_UTILS_H_

#include <string>
#include <vector>

namespace updater {

// Returns true if the app id is for the updater itself or its companion app.
bool IsUpdaterOrCompanionApp(const std::string& app_id);

// Returns true if the updater should uninstall itself. `app_ids` is the set of
// registered applications, `server_starts` is the number of times the server
// has launched, and `had_apps` is a bool indicating whether there has ever been
// an application (other than the updater itself) registered for updates.
bool ShouldUninstall(const std::vector<std::string>& app_ids,
                     int server_starts,
                     bool had_apps);

}  // namespace updater

#endif  // CHROME_UPDATER_APP_APP_UTILS_H_
