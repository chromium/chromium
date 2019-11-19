// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/task_traits.h"

namespace {

const base::FilePath::CharType kRecoveryFileName[] =
    FILE_PATH_LITERAL("ChromeRecovery.exe");

}  // namespace

namespace update_client {

void ActionRunner::RunCommand(const base::CommandLine& cmdline) {
  base::LaunchOptions options;
  options.start_hidden = true;
  base::Process process = base::LaunchProcess(cmdline, options);

  base::PostTask(FROM_HERE, kTaskTraitsRunCommand,
                 base::BindOnce(&ActionRunner::WaitForCommand,
                                base::Unretained(this), std::move(process)));
}

void ActionRunner::WaitForCommand(base::Process process) {
  int exit_code = 0;
  const base::TimeDelta kMaxWaitTime = base::TimeDelta::FromSeconds(600);
  const bool succeeded =
      process.WaitForExitWithTimeout(kMaxWaitTime, &exit_code);
  base::DeleteFileRecursively(unpack_path_);
  main_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(run_complete_), succeeded, exit_code, 0));
}

base::CommandLine ActionRunner::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  base::CommandLine command_line(unpack_path.Append(kRecoveryFileName));
  if (!is_per_user_install_)
    command_line.AppendSwitch("system");
  command_line.AppendSwitchASCII(
      "browser-version", component_.config()->GetBrowserVersion().GetString());
  command_line.AppendSwitchASCII("sessionid", component_.session_id());
  const auto app_guid = component_.config()->GetAppGuid();
  if (!app_guid.empty())
    command_line.AppendSwitchASCII("appguid", app_guid);
  VLOG(1) << "run action: " << command_line.GetCommandLineString();
  return command_line;
}

void ActionRunner::RunRecoveryCRXElevated(const base::FilePath& crx_path) {
  base::CreateCOMSTATaskRunner(
      kTaskTraitsRunCommand, base::SingleThreadTaskRunnerThreadMode::DEDICATED)
      ->PostTask(FROM_HERE,
                 base::BindOnce(&ActionRunner::RunRecoveryCRXElevatedInSTA,
                                base::Unretained(this), crx_path));
}

void ActionRunner::RunRecoveryCRXElevatedInSTA(const base::FilePath& crx_path) {
  bool succeeded = false;
  int error_code = 0;
  int extra_code = 0;
  const auto config = component_.config();
  std::tie(succeeded, error_code, extra_code) =
      component_.config()->GetRecoveryCRXElevator().Run(
          crx_path, config->GetAppGuid(),
          config->GetBrowserVersion().GetString(), component_.session_id());
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(run_complete_), succeeded, error_code,
                                extra_code));
}

}  // namespace update_client
