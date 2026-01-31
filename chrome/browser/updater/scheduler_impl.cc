// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/rand_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/version.h"
#include "build/build_config.h"
#include "chrome/browser/updater/browser_updater_client_util.h"
#include "chrome/browser/updater/scheduler.h"
#include "chrome/updater/constants.h"
#include "chrome/updater/updater_scope.h"
#include "chrome/updater/util/util.h"

namespace updater {

namespace {

void CheckProcessDone(base::Process process, base::OnceClosure callback) {
  // `WaitForExitWithTimeout` will reap the zombie process after it exits.
  if (!process.IsValid() ||
      process.WaitForExitWithTimeout(base::TimeDelta(), nullptr)) {
    std::move(callback).Run();
    return;
  }
  // If the process is still running, yield the thread back to the thread pool
  // and check again in a minute.
  base::ThreadPool::PostDelayedTask(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CheckProcessDone, std::move(process),
                     std::move(callback)),
      base::Minutes(1));
}

}  // namespace

void WakeAllUpdaters(base::OnceClosure callback) {
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
      scope, versions.at(base::RandIntInclusive(0, versions.size() - 1)));
  if (!executable) {
    return;
  }
  base::CommandLine command_line(*executable);
  command_line.AppendSwitch(kWakeAllSwitch);
  if (IsSystemInstall(scope)) {
    command_line.AppendSwitch(kSystemSwitch);
  }

  base::LaunchOptions options;
#if BUILDFLAG(IS_POSIX)
  // Don't inherit signals sent to the browser process group.
  options.new_process_group = true;
#elif BUILDFLAG(IS_WIN)
  // Don't group the updater wake process under the Chrome job.
  options.force_breakaway_from_job_ = true;
#endif
  CheckProcessDone(base::LaunchProcess(command_line, options),
                   std::move(callback));
}

}  // namespace updater
