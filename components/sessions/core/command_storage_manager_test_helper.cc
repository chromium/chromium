// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager_test_helper.h"

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
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

CommandStorageBackend* CommandStorageManagerTestHelper::GetCleartextBackend() {
  return command_storage_manager_->backend_.get();
}

CommandStorageBackend* CommandStorageManagerTestHelper::GetEncryptedBackend() {
  return command_storage_manager_->encrypted_backend_.get();
}

void CommandStorageManagerTestHelper::RunTaskOnBackendThread(
    const base::Location& from_here,
    base::OnceClosure task) {
  command_storage_manager_->backend_task_runner_->PostNonNestableTask(
      from_here, std::move(task));
}

void CommandStorageManagerTestHelper::RunMessageLoopUntilBackendDone() {
  base::RunLoop run_loop;
  command_storage_manager_->backend_task_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(), run_loop.QuitClosure());
  run_loop.Run();
}

bool CommandStorageManagerTestHelper::ProcessedAnyCommands() {
  return command_storage_manager_->backend_->inited_for_testing() ||
         (command_storage_manager_->encrypted_backend_ &&
          command_storage_manager_->encrypted_backend_->inited_for_testing()) ||
         !command_storage_manager_->pending_commands().empty();
}

std::vector<std::unique_ptr<SessionCommand>>
CommandStorageManagerTestHelper::ReadLastSessionCommands() {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  base::RunLoop run_loop;
  command_storage_manager_->GetLastSessionCommands(base::BindLambdaForTesting(
      [&](std::vector<std::unique_ptr<SessionCommand>> commands_out,
          CommandStorageReadStatus status_out) {
        commands = std::move(commands_out);
        run_loop.Quit();
      }));
  run_loop.Run();
  RunMessageLoopUntilBackendDone();
  return commands;
}

scoped_refptr<base::SequencedTaskRunner>
CommandStorageManagerTestHelper::GetBackendTaskRunner() {
  return command_storage_manager_->backend_task_runner_;
}

bool CommandStorageManagerTestHelper::ShouldWriteCleartextFiles() {
  return command_storage_manager_->ShouldWriteCleartextFiles();
}

bool CommandStorageManagerTestHelper::ShouldWriteEncryptedFiles() {
  return command_storage_manager_->ShouldWriteEncryptedFiles();
}

void CommandStorageManagerTestHelper::ForceAppendCommandsToFailForTesting() {
  RunTaskOnBackendThread(
      FROM_HERE,
      base::BindOnce(
          static_cast<void (CommandStorageBackend::*)()>(
              &CommandStorageBackend::ForceAppendCommandsToFailForTesting),
          command_storage_manager_->backend_));
  if (command_storage_manager_->encrypted_backend_) {
    RunTaskOnBackendThread(
        FROM_HERE,
        base::BindOnce(
            static_cast<void (CommandStorageBackend::*)()>(
                &CommandStorageBackend::ForceAppendCommandsToFailForTesting),
            command_storage_manager_->encrypted_backend_));
  }
}

}  // namespace sessions
