// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/updater/util/launchd_util.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/updater/updater_scope.h"
#import "chrome/updater/util/mac_util.h"
#import "chrome/updater/util/util.h"

namespace updater {

namespace {

void PollLaunchctlListImpl(UpdaterScope scope,
                           const std::string& service,
                           LaunchctlPresence expectation,
                           base::TimeTicks deadline,
                           base::OnceCallback<void(bool)> callback) {
  if (base::TimeTicks::Now() > deadline) {
    std::move(callback).Run(false);
    return;
  }
  base::CommandLine command_line(base::FilePath("/bin/launchctl"));
  command_line.AppendArg("list");
  command_line.AppendArg(service);
  if (IsSystemInstall(scope))
    command_line = MakeElevated(command_line);

  base::Process process = base::LaunchProcess(command_line, {});
  if (!process.IsValid()) {
    VLOG(1) << "Failed to launch launchctl process.";
    std::move(callback).Run(false);
    return;
  }
  int exit_code = 0;
  if (!process.WaitForExitWithTimeout(deadline - base::TimeTicks::Now(),
                                      &exit_code)) {
    std::move(callback).Run(false);
    return;
  }
  if ((exit_code == 0 && expectation == LaunchctlPresence::kPresent) ||
      (exit_code != 0 && expectation == LaunchctlPresence::kAbsent)) {
    std::move(callback).Run(true);
    return;
  }
  base::ThreadPool::PostDelayedTask(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(&PollLaunchctlListImpl, scope, service, expectation,
                     deadline, std::move(callback)),
      base::Milliseconds(500));
}

}  // namespace

void PollLaunchctlList(UpdaterScope scope,
                       const std::string& service,
                       LaunchctlPresence expectation,
                       base::TimeDelta timeout,
                       base::OnceCallback<void(bool)> callback) {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::WithBaseSyncPrimitives()},
      base::BindOnce(
          &PollLaunchctlListImpl, scope, service, expectation,
          base::TimeTicks::Now() + timeout,
          base::BindOnce(
              [](scoped_refptr<base::TaskRunner> runner,
                 base::OnceCallback<void(bool)> callback, bool result) {
                runner->PostTask(FROM_HERE,
                                 base::BindOnce(std::move(callback), result));
              },
              base::SequencedTaskRunner::GetCurrentDefault(),
              std::move(callback))));
}

}  // namespace updater
