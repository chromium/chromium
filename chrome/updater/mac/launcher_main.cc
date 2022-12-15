// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <unistd.h>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// This is a placeholder main for a non-side-by-side launcher that launches an
// UpdateService-handling server.
int Main() {
  const UpdaterScope scope =
      geteuid() == 0 ? UpdaterScope::kSystem : UpdaterScope::kUser;
  const absl::optional<base::FilePath> updater_path =
      GetUpdaterExecutablePath(scope);
  if (!updater_path) {
    return kErrorGettingUpdaterPath;
  }

  // TODO(crbug.com/1339108): We need to check directories top-down, not bottom-
  // up.
  // If the file (or any parent directory) is not owned by this user (nor owned
  // by root), or is world-writable, fail.
  base::FilePath check_path = *updater_path;
  while (check_path.DirName() != check_path) {
    base::stat_wrapper_t sb = {};
    if (base::File::Stat(check_path.value().c_str(), &sb)) {
      return kErrorStattingPath;
    }
    if ((sb.st_uid != 0 && sb.st_uid != geteuid()) ||
        ((sb.st_mode & base::FILE_PERMISSION_WRITE_BY_OTHERS) != 0)) {
      return kErrorPathOwnershipMismatch;
    }
    // TODO(crbug.com/1339108): Handle (forbid?) symlinks. Symlinks are
    // problematic since the parents of the symlink might have different access
    // controls than the parents of the symlink's destination.
    // TODO(crbug.com/1339108): Check POSIX.1e ACLs.
    check_path = check_path.DirName();
  }

  // TODO(crbug.com/1339108): Check code signing, unless this is not code
  // signed?

  // TODO(crbug.com/1339108): Check for chroot (if scope == kSystem).

  base::CommandLine command_line(*updater_path);
  command_line.AppendSwitch(kServerSwitch);
  command_line.AppendSwitchASCII(kServerServiceSwitch,
                                 kServerUpdateServiceSwitchValue);
  if (scope == UpdaterScope::kSystem) {
    command_line.AppendSwitch(kSystemSwitch);
  }
  command_line.AppendSwitch(kEnableLoggingSwitch);
  command_line.AppendSwitchASCII(kLoggingModuleSwitch,
                                 kLoggingModuleSwitchValue);
  base::LaunchOptions opts;
  opts.clear_environment = true;
  // TODO(crbug.com/1339108): Reset rlimits to default values, unless current
  // limits are higher.
  // TODO(crbug.com/1339108): Reset POSIX signal dispositions.
  // TODO(crbug.com/1339108): Climb bootstrap ports until the bootstrap port is
  // the top-level "system" bootstrap port.
  // TODO(crbug.com/1339108): Run the process in a separate terminal session.
  if (!base::LaunchProcess(command_line, opts).IsValid()) {
    return kErrorLaunchingProcess;
  }
  return 0;
}

}  // namespace updater

int main(int argc, const char* const* argv) {
  return updater::Main();
}
