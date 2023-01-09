// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_AUTO_RUN_ON_OS_UPGRADE_TASK_H_
#define CHROME_UPDATER_AUTO_RUN_ON_OS_UPGRADE_TASK_H_

#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "chrome/updater/updater_scope.h"

namespace updater {

class PersistedData;

// The AutoRunOnOsUpgrade task runs app commands registered with a REG_DWORD of
// `AutoRunOnOSUpgrade` when the OS upgrades (specifically, when `HasOSUpgraded`
// returns `true`. This feature is used by apps that need to do registration
// updates or fixing shortcuts or other maintenance in response to the OS
// updating.
class AutoRunOnOsUpgradeTask
    : public base::RefCountedThreadSafe<AutoRunOnOsUpgradeTask> {
 public:
  AutoRunOnOsUpgradeTask(UpdaterScope scope,
                         scoped_refptr<PersistedData> persisted_data);
  void Run(base::OnceClosure callback);

 private:
  friend class base::RefCountedThreadSafe<AutoRunOnOsUpgradeTask>;
  FRIEND_TEST_ALL_PREFIXES(AutoRunOnOsUpgradeTaskTest, RunOnOsUpgradeForApp);
  virtual ~AutoRunOnOsUpgradeTask();

  // Runs AutoRunOnOSUpgrade commands registered for all `app_ids`.
  void RunOnOsUpgradeForApps(const std::vector<std::string>& app_ids);

  // Runs AutoRunOnOSUpgrade commands registered for `app_id`. Returns the
  // number of commands successfully launched.
  size_t RunOnOsUpgradeForApp(const std::string& app_id);

  bool HasOSUpgraded();
  void SetOSUpgraded();

  SEQUENCE_CHECKER(sequence_checker_);
  UpdaterScope scope_;
  scoped_refptr<PersistedData> persisted_data_;
  std::string os_upgrade_string_;
};

}  // namespace updater

#endif  // CHROME_UPDATER_AUTO_RUN_ON_OS_UPGRADE_TASK_H_
