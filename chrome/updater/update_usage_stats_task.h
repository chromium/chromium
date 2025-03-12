// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_
#define CHROME_UPDATER_UPDATE_USAGE_STATS_TASK_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/updater/updater_scope.h"

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
  virtual bool AnyAppEnablesUsageStats(UpdaterScope scope) = 0;
  static std::unique_ptr<UsageStatsProvider> Create();

 private:
#if BUILDFLAG(IS_WIN)
  static std::unique_ptr<UsageStatsProvider> Create(
      const std::wstring& system_key,
      const std::wstring& user_key);
#elif BUILDFLAG(IS_MAC)
  static std::unique_ptr<UsageStatsProvider> Create(
      const base::FilePath& app_directory);
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
