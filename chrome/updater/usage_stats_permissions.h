// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_USAGE_STATS_PERMISSIONS_H_
#define CHROME_UPDATER_USAGE_STATS_PERMISSIONS_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {
class FilePath;
}

namespace updater {

struct EventLoggingPermissionProvider;

// Returns true if any app besides Omaha 4 or CECA is allowed to send usage
// stats. The function looks at apps installed on the system to check if they
// have usage stats enabled. This information is stored in the registry on
// Windows, and in a crashpad database found in the `ApplicationSupport`
// directory on MacOS.
bool AnyAppEnablesUsageStats(UpdaterScope scope);

// Returns true if the updater is allowed to send detailed event logs to an
// external endpoint. Logging is allowed only if the following conditions are
// all met:
//    1) The updater manages an app (an `event_logging_permission_provider`)
//       responsible for granting the updater permission to send remote
//       logging events.
//    2) The `event_logging_permission_provider` app has usage stats enabled.
//    3) The updater manages no other apps. That is, the apps managed by the
//    updater are a subset of {Updater, Enterprise Companion App,
//    `event_logging_permission_provider`}.
bool RemoteEventLoggingAllowed(
    UpdaterScope scope,
    const std::vector<std::string>& installed_app_ids,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider);

#if BUILDFLAG(IS_MAC)
bool AnyAppEnablesUsageStats(
    const std::vector<base::FilePath>& app_support_directories);
bool RemoteEventLoggingAllowed(
    const std::vector<std::string>& installed_app_ids,
    const std::vector<base::FilePath>& app_support_directories,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider);
#elif BUILDFLAG(IS_WIN)
bool AnyAppEnablesUsageStats(HKEY hive,
                             const std::vector<std::wstring>& key_paths);
bool RemoteEventLoggingAllowed(
    HKEY hive,
    const std::vector<std::wstring>& key_paths,
    const std::vector<std::string>& installed_app_ids,
    std::optional<EventLoggingPermissionProvider>
        event_logging_permission_provider);
#endif

}  // namespace updater

#endif  // CHROME_UPDATER_USAGE_STATS_PERMISSIONS_H_
