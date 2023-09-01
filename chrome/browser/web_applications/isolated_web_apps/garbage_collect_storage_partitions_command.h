// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GARBAGE_COLLECT_STORAGE_PARTITIONS_COMMAND_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GARBAGE_COLLECT_STORAGE_PARTITIONS_COMMAND_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
#include "chrome/browser/web_applications/commands/web_app_command.h"

class Profile;

namespace web_app {

class AllAppsLock;
class AllAppsLockDescription;
class ExtensionInstallGate;

// Starts a transaction to:
// 1. Gather all the valid Storage Partition domains and store them in
// a list. Currently supports the following sub systems:
//   a. Web Apps - Isolated Web Apps.
//   b. Extensions - Chrome Apps
// 2. Call BrowserContext::GarbageCollectStoragePartitions with the list as a
// parameter to delete any Storage Partition domain level paths that is invalid
// and currently inactive.
class GarbageCollectStoragePartititonsCommand
    : public WebAppCommandTemplate<AllAppsLock> {
 public:
  explicit GarbageCollectStoragePartititonsCommand(Profile* profile,
                                                   base::OnceClosure done);
  ~GarbageCollectStoragePartititonsCommand() override;

  // WebAppCommandTemplate<AllAppsLock>:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;
  void OnShutdown() override;
  const LockDescription& lock_description() const override;
  base::Value ToDebugValue() const override;

 private:
  void ResetStorageGarbageCollectPref();

  void OnPrefReset();

  void DoGarbageCollection();

  void OnSuccess();

  std::unique_ptr<AllAppsLockDescription> lock_description_;
  std::unique_ptr<AllAppsLock> lock_;

  base::raw_ptr<Profile> profile_;

  base::OnceClosure done_closure_;

  std::unique_ptr<ExtensionInstallGate> install_gate_;

  base::Value::Dict debug_info_;

  base::WeakPtrFactory<GarbageCollectStoragePartititonsCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif
