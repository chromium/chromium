// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager_test_helper.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_manager.h"

namespace sessions {

CommandStorageManagerTestHelper::CommandStorageManagerTestHelper(
    CommandStorageManager* command_storage_manager)
    : command_storage_manager_(command_storage_manager) {
  CHECK(command_storage_manager);
}

void CommandStorageManagerTestHelper::RunTaskOnBackendThread(
    const base::Location& from_here,
    base::OnceClosure task) {
  command_storage_manager_->backend_task_runner_->PostNonNestableTask(
      from_here, std::move(task));
}

void CommandStorageManagerTestHelper::RunMessageLoopUntilBackendDone() {
  auto current_task_runner = base::SingleThreadTaskRunner::GetCurrentDefault();
  base::RunLoop run_loop;
  auto quit_closure = run_loop.QuitClosure();
  auto quit_from_backend =
      base::BindLambdaForTesting([&current_task_runner, &quit_closure]() {
        current_task_runner->PostTask(FROM_HERE, std::move(quit_closure));
      });
  RunTaskOnBackendThread(FROM_HERE, std::move(quit_from_backend));
  run_loop.Run();
}

bool CommandStorageManagerTestHelper::ProcessedAnyCommands() {
  return command_storage_manager_->backend_->inited() ||
         !command_storage_manager_->pending_commands().empty();
}

std::vector<std::unique_ptr<SessionCommand>>
CommandStorageManagerTestHelper::ReadLastSessionCommands() {
  return command_storage_manager_->backend_.get()
      ->ReadLastSessionCommands()
      .commands;
}

scoped_refptr<base::SequencedTaskRunner>
CommandStorageManagerTestHelper::GetBackendTaskRunner() {
  return command_storage_manager_->backend_task_runner_;
}

void CommandStorageManagerTestHelper::ForceAppendCommandsToFailForTesting() {
  RunTaskOnBackendThread(
      FROM_HERE,
      base::BindOnce(
          &CommandStorageBackend::ForceAppendCommandsToFailForTesting,
          command_storage_manager_->backend_));
}

}  // namespace sessions
