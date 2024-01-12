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
#include "chrome/browser/web_applications/locks/all_apps_lock.h"

class Profile;

namespace web_app {

class ExtensionInstallGate;

// Starts a transaction to:
// 1. Gather all the valid Storage Partition domains and store them in
// a list. Currently supports the following sub systems:
//   a. Web Apps - Isolated Web Apps.
//   b. Extensions - Chrome Apps
// 2. Call BrowserContext::GarbageCollectStoragePartitions with the list as a
// parameter to delete any Storage Partition domain level paths that is invalid
// and currently inactive.
class GarbageCollectStoragePartitionsCommand
    : public WebAppCommand<AllAppsLock> {
 public:
  explicit GarbageCollectStoragePartitionsCommand(Profile* profile,
                                                  base::OnceClosure done);
  ~GarbageCollectStoragePartitionsCommand() override;

  // WebAppCommand:
  void StartWithLock(std::unique_ptr<AllAppsLock> lock) override;

 private:
  void ResetStorageGarbageCollectPref();

  void OnPrefReset();

  void DoGarbageCollection();

  void OnSuccess();

  std::unique_ptr<AllAppsLock> lock_;
  raw_ptr<Profile> profile_ = nullptr;
  std::unique_ptr<ExtensionInstallGate> install_gate_;

  base::WeakPtrFactory<GarbageCollectStoragePartitionsCommand> weak_factory_{
      this};
};

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_ISOLATED_WEB_APPS_GARBAGE_COLLECT_STORAGE_PARTITIONS_COMMAND_H_
