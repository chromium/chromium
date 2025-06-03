// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
#define CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
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

class PersistedData;

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
    std::optional<std::string> event_logging_permission_provider);

#if BUILDFLAG(IS_MAC)
bool AnyAppEnablesUsageStats(
    const std::vector<base::FilePath>& install_directories);
bool RemoteEventLoggingAllowed(
    const std::vector<base::FilePath>& install_directories,
    std::optional<std::string> event_logging_permission_provider);
#elif BUILDFLAG(IS_WIN)
bool AnyAppEnablesUsageStats(HKEY hive,
                             const std::vector<std::wstring>& key_paths);
bool RemoteEventLoggingAllowed(
    HKEY hive,
    const std::vector<std::wstring>& key_paths,
    std::optional<std::string> event_logging_permission_provider);
#endif

class UpdateUsageStatsTask
    : public base::RefCountedThreadSafe<UpdateUsageStatsTask> {
 public:
  UpdateUsageStatsTask(UpdaterScope scope,
                       scoped_refptr<PersistedData> persisted_data);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<UpdateUsageStatsTask>;
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, NoApps);
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, OneAppEnabled);
  FRIEND_TEST_ALL_PREFIXES(UpdateUsageStatsTaskTest, ZeroAppsEnabled);
  virtual ~UpdateUsageStatsTask();
  void SetUsageStatsEnabled(scoped_refptr<PersistedData> persisted_data,
                            bool enabled);

  SEQUENCE_CHECKER(sequence_checker_);
  const UpdaterScope scope_;
  scoped_refptr<PersistedData> persisted_data_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
