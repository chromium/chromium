// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/rand_util.h"
#include "base/version.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/scheduler.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

void WakeAllUpdaters() {
  const UpdaterScope scope = GetBrowserUpdaterScope();
  std::optional<base::FilePath> install_dir = GetInstallDirectory(scope);
  if (!install_dir) {
    return;
  }
  std::vector<base::Version> versions;
  base::FileEnumerator(*install_dir, false, base::FileEnumerator::DIRECTORIES)
      .ForEach([&versions](const base::FilePath& item) {
        base::Version version(item.BaseName().AsUTF8Unsafe());
        if (version.IsValid()) {
          versions.push_back(version);
        }
      });
  if (versions.empty()) {
    return;
  }
  std::optional<base::FilePath> executable = GetUpdaterExecutablePath(
      scope, versions.at(base::RandInt(0, versions.size() - 1)));
  if (!executable) {
    return;
  }
  base::CommandLine command_line(*executable);
  command_line.AppendSwitch(kWakeAllSwitch);
  if (IsSystemInstall(scope)) {
    command_line.AppendSwitch(kSystemSwitch);
  }
  int exit_code = 0;
  std::string output;
  base::GetAppOutputWithExitCode(command_line, &output, &exit_code);
  VLOG_IF(1, exit_code != 0)
      << "Updater backup scheduler failed: " << exit_code << ": " << output;
}

}  // namespace updater
