// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <iterator>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/crx_file/crx_verifier.h"
#include "components/update_client/component.h"
#include "components/update_client/configurator.h"
#include "components/update_client/patcher.h"
#include "components/update_client/task_traits.h"
#include "components/update_client/unzipper.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_engine.h"

namespace update_client {

ActionRunner::ActionRunner(const Component& component)
    : is_per_user_install_(component.config()->IsPerUserInstall()),
      component_(component),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

ActionRunner::~ActionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ActionRunner::Run(Callback run_complete) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  run_complete_ = std::move(run_complete);

  base::CreateSequencedTaskRunner(kTaskTraits)
      ->PostTask(
          FROM_HERE,
          base::BindOnce(&ActionRunner::RunOnTaskRunner, base::Unretained(this),
                         component_.config()->GetUnzipperFactory()->Create(),
                         component_.config()->GetPatcherFactory()->Create()));
}

void ActionRunner::RunOnTaskRunner(std::unique_ptr<Unzipper> unzip,
                                   scoped_refptr<Patcher> patch) {
  const auto installer = component_.crx_component()->installer;

  base::FilePath crx_path;
  installer->GetInstalledFile(component_.action_run(), &crx_path);

  if (!is_per_user_install_) {
    RunRecoveryCRXElevated(std::move(crx_path));
    return;
  }

  const auto config = component_.config();
  auto unpacker = base::MakeRefCounted<ComponentUnpacker>(
      config->GetRunActionKeyHash(), crx_path, installer, std::move(unzip),
      std::move(patch), component_.crx_component()->crx_format_requirement);
  unpacker->Unpack(
      base::BindOnce(&ActionRunner::UnpackComplete, base::Unretained(this)));
}

void ActionRunner::UnpackComplete(const ComponentUnpacker::Result& result) {
  if (result.error != UnpackerError::kNone) {
    DCHECK(!base::DirectoryExists(result.unpack_path));

    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(std::move(run_complete_), false,
                       static_cast<int>(result.error), result.extended_error));
    return;
  }

  unpack_path_ = result.unpack_path;
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&ActionRunner::RunCommand, base::Unretained(this),
                     MakeCommandLine(result.unpack_path)));
}

#if !defined(OS_WIN)

void ActionRunner::RunRecoveryCRXElevated(const base::FilePath& crx_path) {
  NOTREACHED();
}

void ActionRunner::RunCommand(const base::CommandLine& cmdline) {
  base::DeleteFileRecursively(unpack_path_);
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(std::move(run_complete_), false, -1, 0));
}

base::CommandLine ActionRunner::MakeCommandLine(
    const base::FilePath& unpack_path) const {
  return base::CommandLine(base::CommandLine::NO_PROGRAM);
}

#endif  // OS_WIN

}  // namespace update_client
