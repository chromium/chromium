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

// A UsageStatsProvider evaluates the usage stat state of apps on the system to
// determine whether or not the updater is allowed to send usage stats.
class UsageStatsProvider {
 public:
  virtual ~UsageStatsProvider() = default;

  // Returns true if any app besides Omaha 4 or CECA is allowed to send usage
  // stats. The function looks at apps installed on the system to check if they
  // have usage stats enabled. This information is stored in the registry on
  // Windows, and in a crashpad database found in the `ApplicationSupport`
  // directory on MacOS.
  virtual bool AnyAppEnablesUsageStats() const = 0;

  // Returns true if the updater is allowed to send detailed event logs to an
  // external endpoint. Logging is allowed only if the following conditions are
  // all met:
  //    1) The updater manages an app (an `event_logging_permission_provider`)
  //       responsible for granting the updater permission to send remote
  //       logging events.
  //    2) The `event_logging_permission_provider` app has usage stats enabled.
  //    3) The updater manages no other apps. That is, the apps managed by the
  //    updater are a subset of {Updater, Enterprise Companion App,
  //    event_logging_permission_provider}.
  virtual bool RemoteEventLoggingAllowed() const = 0;

  static std::unique_ptr<UsageStatsProvider> Create(UpdaterScope scope);

 private:
  friend class UpdateUsageStatsTaskTest;

  // Creates a UsageStatsProvider that checks apps in the specified locations
  // (install directories on MacOS and registry paths on Windows), as well as
  // uses the specified app as an event logging permission provider. The app is
  // identified via an appid on Windows and as the name of the application
  // directory on MacOS.
#if BUILDFLAG(IS_WIN)
  static std::unique_ptr<UsageStatsProvider> Create(
      HKEY hive,
      std::optional<std::wstring> event_logging_permission_provider,
      std::vector<std::wstring> registry_paths);
#elif BUILDFLAG(IS_MAC)
  static std::unique_ptr<UsageStatsProvider> Create(
      std::optional<std::string> event_logging_permission_provider,
      std::vector<base::FilePath> app_directories);
#endif
};

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
