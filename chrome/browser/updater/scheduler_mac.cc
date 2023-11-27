// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/updater/scheduler.h"

#include <string>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/callback.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/mac/keystone_glue.h"
#include "chrome/browser/updater/browser_updater_client.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/mac_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

namespace {

void CheckProcessExit(base::Process process, base::OnceClosure callback) {
  if (!process.IsValid() ||
      process.WaitForExitWithTimeout(base::TimeDelta(), nullptr)) {
    std::move(callback).Run();
  } else {
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CheckProcessExit, std::move(process),
                       std::move(callback)),
        base::Minutes(1));
  }
}

}  // namespace

void DoPeriodicTasks(base::OnceClosure callback) {
  if (base::FeatureList::IsEnabled(features::kUseChromiumUpdater)) {
    BrowserUpdaterClient::Create(GetUpdaterScope())
        ->RunPeriodicTasks(std::move(callback));
  } else {
    if (!keystone_glue::KeystoneEnabled()) {
      return;
    }

    // The registration framework doesn't provide a mechanism to ask Keystone to
    // just do its normal routine tasks, so instead launch the agent directly.
    // The agent can be in one of four places depending on the age and mode of
    // Keystone.
    for (UpdaterScope scope : {UpdaterScope::kSystem, UpdaterScope::kUser}) {
      absl::optional<base::FilePath> keystone_path =
          GetKeystoneFolderPath(scope);
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
          CheckProcessExit(
              base::LaunchProcess(base::CommandLine(agent_path), {}),
              std::move(callback));
          return;
        }
      }
    }
    std::move(callback).Run();
  }
}

}  // namespace updater
