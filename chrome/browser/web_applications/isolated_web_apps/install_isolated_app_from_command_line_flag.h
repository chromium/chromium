// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_FLAG_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_FLAG_H_

#include <string>
#include <vector>

namespace base {
class CommandLine;
}

class Profile;

namespace web_app {

std::vector<std::string> GetAppsToInstallFromCommandLine(
    const base::CommandLine& command_line);

void InstallAppFromCommandLine(const base::CommandLine& command_line,
                               Profile& profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_FLAG_H_
