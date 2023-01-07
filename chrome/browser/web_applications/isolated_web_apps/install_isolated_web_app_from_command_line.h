// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_

#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "chrome/browser/web_applications/isolated_web_apps/isolated_web_app_url_info.h"
#include "chrome/browser/web_applications/isolation_data.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
class CommandLine;
}

class PrefService;
class Profile;

namespace web_app {

base::expected<absl::optional<IsolationData>, std::string>
GetIsolationDataFromCommandLine(const base::CommandLine& command_line,
                                const PrefService* prefs);

void MaybeInstallAppFromCommandLine(const base::CommandLine& command_line,
                                    Profile& profile);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_INSTALL_ISOLATED_WEB_APP_FROM_COMMAND_LINE_H_
