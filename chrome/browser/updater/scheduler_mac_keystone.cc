// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/mac/keystone_glue.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

void CheckProcessExit(base::Process process, base::OnceClosure callback) {
  if (!process.IsValid() ||
      process.WaitForExitWithTimeout(base::TimeDelta(), nullptr)) {
    std::move(callback).Run();
  } else {
    base::ThreadPool::PostDelayedTask(
        FROM_HERE,
        {base::TaskPriority::BEST_EFFORT, base::MayBlock(),
         base::WithBaseSyncPrimitives(),
         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
        base::BindOnce(&CheckProcessExit, std::move(process),
                       std::move(callback)),
        base::Minutes(1));
  }
}

}  // namespace

void DoPeriodicTasks(base::OnceClosure callback) {
  if (!keystone_glue::KeystoneEnabled()) {
    return;
  }

  // The registration framework doesn't provide a mechanism to ask Keystone to
  // just do its normal routine tasks, so instead launch the agent directly.
  // The agent can be in one of four places depending on the age and mode of
  // Keystone.
  for (UpdaterScope scope : {UpdaterScope::kSystem, UpdaterScope::kUser}) {
    absl::optional<base::FilePath> keystone_path = GetKeystoneFolderPath(scope);
    if (!keystone_path) {
      continue;
    }
    for (const std::string& folder : {"Helpers", "Resources"}) {
      base::FilePath agent_path =
          keystone_path->Append("Contents")
              .Append(folder)
              .Append(
                  "GoogleSoftwareUpdateAgent.app/Contents/MacOS/"
                  "GoogleSoftwareUpdateAgent");
      if (base::PathExists(agent_path)) {
        CheckProcessExit(base::LaunchProcess(base::CommandLine(agent_path), {}),
                         std::move(callback));
        return;
      }
    }
  }
}

}  // namespace updater
