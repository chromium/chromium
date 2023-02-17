// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

void Main() {
  for (UpdaterScope scope : {UpdaterScope::kSystem, UpdaterScope::kUser}) {
    absl::optional<base::FilePath> path = GetUpdaterExecutablePath(scope);
    if (!path) {
      continue;
    }
    base::CommandLine command(*path);
    command.AppendSwitch(kWakeSwitch);
    if (scope == UpdaterScope::kSystem) {
      command.AppendSwitch(kSystemSwitch);
    }
    command.AppendSwitch(kEnableLoggingSwitch);
    command.AppendSwitchNative(kLoggingModuleSwitch, kLoggingModuleSwitchValue);
    base::LaunchProcess(command, {});
  }
}

}  // namespace updater

// The agent is a shim to trick the keystone registration framework. When run,
// it should launch the --wake task. Not all callers correctly provide a scope,
// so it will wake both scopes (if present).
int main() {
  updater::Main();
  return 0;
}
