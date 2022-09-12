// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_

#include "base/callback.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}

class GURL;
class Profile;

namespace web_app {

void SetNextInstallationDoneCallbackForTesting(base::OnceClosure done_callback);

absl::optional<GURL> GetAppToInstallFromCommandLine(
    const base::CommandLine& command_line);

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_APP_FROM_COMMAND_LINE_H_
