// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/ipc/update_service_dialer.h"

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/posix_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace updater {

// Start the update service by running the launcher directly.
bool DialUpdateService(UpdaterScope scope) {
  absl::optional<base::FilePath> updater = GetUpdateServiceLauncherPath(scope);
  if (updater) {
    if (!base::PathExists(*updater)) {
      // If there's no updater present, abandon dialing.
      return false;
    }
    base::LaunchProcess(base::CommandLine(*updater), {});
  }

  return true;
}

}  // namespace updater
