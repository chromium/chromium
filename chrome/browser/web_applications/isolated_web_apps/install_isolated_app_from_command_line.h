// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/callback_helpers.h"

namespace base {
class CommandLine;
}

class GURL;
class Profile;

namespace web_app {

std::vector<GURL> GetAppsToInstallFromCommandLine(
    const base::CommandLine& command_line);

void MaybeInstallAppFromCommandLine(
    const base::CommandLine& command_line,
    Profile& profile,
    base::OnceCallback<void()> done = base::DoNothing());

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_
