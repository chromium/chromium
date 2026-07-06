// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sessions/core/command_storage_manager.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/values_equivalent.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/sessions/core/command_storage_backend.h"
#include "components/sessions/core/command_storage_features.h"
#include "components/sessions/core/command_storage_manager_delegate.h"
#include "components/sessions/core/session_command.h"
#include "components/sessions/core/session_constants.h"

namespace sessions {
namespace {

// Delay between when a command is received, and when we save it to the
// backend.
constexpr base::TimeDelta kSaveDelay = base::Milliseconds(2500);

std::vector<std::unique_ptr<SessionCommand>> DeepCopyCommands(
    const std::vector<std::unique_ptr<SessionCommand>>& commands) {
  std::vector<std::unique_ptr<SessionCommand>> commands_copy;
  commands_copy.reserve(commands.size());
  for (const auto& command : commands) {
    commands_copy.push_back(command->Clone());
  }
  return commands_copy;
}

bool CommandsEqual(const std::vector<std::unique_ptr<SessionCommand>>& a,
                   const std::vector<std::unique_ptr<SessionCommand>>& b) {
  return std::ranges::equal(a, b, [](const auto& x, const auto& y) {
    return base::ValuesEquivalent(x, y);
  });
}

// Result of comparing the commands read from the cleartext and encrypted
// backends.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionReadComparisonResult)
enum class SessionReadComparisonResult {
  kMatch = 0,
  kErrorMismatch = 1,
  kCommandsMismatch = 2,
  kMaxValue = kCommandsMismatch,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SessionReadComparisonResult)

// Which operation encountered an uninitialized encrypted backend.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(SessionEncryptedBackendUninitialized)
enum class SessionEncryptedBackendUninitialized {
  kSave = 0,
  kMoveCurrentSessionToLastSession = 1,
  kDeleteLastSession = 2,
  kGetLastSessionCommands = 3,
  kMaxValue = kGetLastSessionCommands,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/session/enums.xml:SessionEncryptedBackendUninitialized)

void OnBackendReadFinished(
    scoped_refptr<base::RefCountedData<
        CommandStorageBackend::ReadCommandsResult>> saved_result,
    CommandStorageManager::GetCommandsCallback callback,
    CommandStorageBackend::ReadCommandsResult result) {
  if (saved_result) {
    saved_result->data.commands = DeepCopyCommands(result.commands);
    saved_result->data.error_reading = result.error_reading;
  }
  std::move(callback).Run(std::move(result.commands), result.error_reading);
}

// Called when the encrypted backend has finished reading commands, and it is
// expected that there is a cleartext backend that preceded it.
void CompareCleartextAndEncryptedReadResults(
    scoped_refptr<base::RefCountedData<
        CommandStorageBackend::ReadCommandsResult>> cleartext_result,
    const CommandStorageBackend::ReadCommandsResult& encrypted_result) {
  SessionReadComparisonResult comparison_result =
      SessionReadComparisonResult::kMatch;
  if (cleartext_result->data.error_reading != encrypted_result.error_reading) {
    comparison_result = SessionReadComparisonResult::kErrorMismatch;
  } else if (!CommandsEqual(cleartext_result->data.commands,
                            encrypted_result.commands)) {
    comparison_result = SessionReadComparisonResult::kCommandsMismatch;
  }
  base::UmaHistogramEnumeration(
      "Session.CommandStorageManager.EncryptedReadMatch", comparison_result);
}

// Called when the encrypted backend has finished reading the commands, and the
// encrypted backend is preferred.
// Args:
//   cleartext_result: The result from the cleartext backend; could be null if
//     the read of the encrypted backend was not preceded by a cleartext read.
//   backend_task_runner: The task runner to use for the cleartext backend.
//   backend: The cleartext backend.
//   callback: The callback to run when the cleartext backend has finished
//     reading the commands.
//   result: The result from the encrypted backend.
void OnEncryptedBackendReadFinishedWithPreferEncrypted(
    scoped_refptr<base::RefCountedData<
        CommandStorageBackend::ReadCommandsResult>> cleartext_result,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner,
    scoped_refptr<CommandStorageBackend> backend,
    CommandStorageManager::GetCommandsCallback callback,
    CommandStorageBackend::ReadCommandsResult result) {
  if (cleartext_result) {
    CompareCleartextAndEncryptedReadResults(cleartext_result, result);
  }
  if (!result.error_reading && !result.commands.empty()) {
    std::move(callback).Run(std::move(result.commands), result.error_reading);
    if (GetEncryptSessionStorageStage() ==
        EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted) {
      // Make a best effort to delete the old cleartext file (leftover from
      // previous stage) when now in stage kWriteEncryptedReadPreferEncrypted.
      backend_task_runner->PostNonNestableTask(
          FROM_HERE,
          base::BindOnce(&CommandStorageBackend::DeleteLastSession, backend));
    }
    return;
  }

  // Fallback to the cleartext backend.
  if (cleartext_result) {
    if (!cleartext_result->data.error_reading) {
      std::move(callback).Run(std::move(cleartext_result->data.commands),
                              cleartext_result->data.error_reading);
    } else {  // Both backends had errors.
      std::move(callback).Run(std::move(result.commands), result.error_reading);
    }
  } else {
    // Fallback to cleartext in stage kWriteEncryptedReadPreferEncrypted.
    backend_task_runner->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                       backend.get()),
        base::BindOnce(&OnBackendReadFinished, /*saved_result=*/nullptr,
                       std::move(callback)));
  }
}

void LogEncryptedBackendUninitialized(
    SessionEncryptedBackendUninitialized reason) {
  base::UmaHistogramEnumeration(
      "Session.CommandStorageManager.EncryptedBackendUninitialized", reason);
}

#if DCHECK_IS_ON()
base::Value CommandToDebugValue(const SessionCommand& command) {
  // There is no convenience function to transform the pickled contents into the
  // payload struct itself, so just output the id for now.
  return base::Value(command.id());
}
#endif  // DCHECK_IS_ON()

}  // namespace

CommandStorageManager::CommandStorageManager(
    SessionType type,
    const base::FilePath& path,
    CommandStorageManagerDelegate* delegate,
    os_crypt_async::OSCryptAsync* os_crypt_async,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : file_path_(path),
      session_type_(type),
      backend_task_runner_(backend_task_runner
                               ? backend_task_runner
                               : CreateDefaultBackendTaskRunner()),
      backend_(
          base::MakeRefCounted<CommandStorageBackend>(backend_task_runner_,
                                                      path,
                                                      type,
                                                      /*encryptor=*/nullptr)),
      delegate_(delegate) {
  CHECK(os_crypt_async);
  if (ShouldWriteEncryptedFiles()) {
    os_crypt_async->GetInstance(base::BindOnce(
        &CommandStorageManager::OnEncryptorReady, weak_factory_.GetWeakPtr(),
        /*start_time=*/base::TimeTicks::Now()));
  } else if (GetEncryptSessionStorageStage() ==
             EncryptSessionStorageStage::kClearOnly) {
    // If we don't update encrypted files, then we need to delete the stale
    // directory on the background thread. We need to be sure encryption is
    // fully disabled as we need to know the entire folder is extranious.
    backend_task_runner->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(
            [](const base::FilePath encrypted_session_directory) {
              base::DeletePathRecursively(encrypted_session_directory);
            },
            file_path_.Append(kEncryptedSessionsDirectory)));
  }
}

CommandStorageManager::~CommandStorageManager() = default;

// static
scoped_refptr<base::SequencedTaskRunner>
CommandStorageManager::CreateDefaultBackendTaskRunner() {
  return base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN});
}

bool CommandStorageManager::ShouldWriteCleartextFiles() const {
  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  switch (stage) {
    case EncryptSessionStorageStage::kClearOnly:
    case EncryptSessionStorageStage::kWriteBothReadOnlyClear:
    case EncryptSessionStorageStage::kWriteBothReadPreferEncrypted:
      return true;
    case EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted:
      return false;
  }
  NOTREACHED();
}

bool CommandStorageManager::ShouldWriteEncryptedFiles() const {
  EncryptSessionStorageStage stage = GetEncryptSessionStorageStage();
  switch (stage) {
    case EncryptSessionStorageStage::kWriteBothReadOnlyClear:
    case EncryptSessionStorageStage::kWriteBothReadPreferEncrypted:
    case EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted:
      // On iOS, SessionRestore and AppRestore do not use CommandStorageBackend.
      return session_type_ == SessionType::kTabRestore || !BUILDFLAG(IS_IOS);
    case EncryptSessionStorageStage::kClearOnly:
      return false;
  }
  NOTREACHED();
}

void CommandStorageManager::OnEncryptorReady(
    base::TimeTicks start_time,
    scoped_refptr<os_crypt_async::Encryptor> encryptor) {
  CHECK(ShouldWriteEncryptedFiles());
  CHECK(encrypted_backend_ == nullptr);
  base::UmaHistogramTimes(
      "Session.CommandStorageManager.OnEncryptorReadyDuration",
      base::TimeTicks::Now() - start_time);
  encrypted_backend_ = base::MakeRefCounted<CommandStorageBackend>(
      backend_task_runner_, file_path_, session_type_, std::move(encryptor),
      /*clock=*/nullptr);

  std::vector<base::OnceClosure> ops = std::move(pending_encrypted_ops_);
  for (base::OnceClosure& op : ops) {
    std::move(op).Run();
  }
}

void CommandStorageManager::ScheduleCommand(
    std::unique_ptr<SessionCommand> command) {
  DCHECK(command);
  commands_since_reset_++;
  pending_commands_.push_back(std::move(command));
  StartSaveTimer();
}

void CommandStorageManager::AppendRebuildCommand(
    std::unique_ptr<SessionCommand> command) {
  std::vector<std::unique_ptr<SessionCommand>> commands;
  commands.push_back(std::move(command));
  AppendRebuildCommands(std::move(commands));
}

void CommandStorageManager::AppendRebuildCommands(
    std::vector<std::unique_ptr<SessionCommand>> commands) {
  commands_since_reset_ += commands.size();
  pending_commands_.insert(pending_commands_.end(),
                           std::make_move_iterator(commands.begin()),
                           std::make_move_iterator(commands.end()));
}

void CommandStorageManager::EraseCommand(SessionCommand* old_command) {
  auto it = std::ranges::find(pending_commands_, old_command,
                              &std::unique_ptr<SessionCommand>::get);
  CHECK(it != pending_commands_.end());
  pending_commands_.erase(it);
  DCHECK_GT(commands_since_reset_, 0);
  --commands_since_reset_;
}

void CommandStorageManager::SwapCommand(
    SessionCommand* old_command,
    std::unique_ptr<SessionCommand> new_command) {
  auto it = std::ranges::find(pending_commands_, old_command,
                              &std::unique_ptr<SessionCommand>::get);
  CHECK(it != pending_commands_.end());
  *it = std::move(new_command);
}

void CommandStorageManager::ClearPendingCommands() {
  DCHECK_GE(commands_since_reset_, static_cast<int>(pending_commands_.size()));
  commands_since_reset_ -= static_cast<int>(pending_commands_.size());
  pending_commands_.clear();
}

void CommandStorageManager::StartSaveTimer() {
  // Don't start a timer when testing.
  if (delegate_->ShouldUseDelayedSave() &&
      base::SingleThreadTaskRunner::HasCurrentDefault() && !HasPendingSave()) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&CommandStorageManager::Save,
                       weak_factory_for_timer_.GetWeakPtr()),
        kSaveDelay);
  }
}

void CommandStorageManager::Save() {
  weak_factory_for_timer_.InvalidateWeakPtrs();

  // Inform the delegate that we will save the commands now, giving it the
  // opportunity to append more commands.
  delegate_->OnWillSaveCommands();

  if (pending_commands_.empty()) {
    return;
  }

#if DCHECK_IS_ON()
  for (const std::unique_ptr<SessionCommand>& command : pending_commands_) {
    written_commands_reverse_debug_log_.push_front(
        CommandToDebugValue(*command));
  }
  if (written_commands_reverse_debug_log_.size() > kMaxLogSize) {
    written_commands_reverse_debug_log_.resize(kMaxLogSize);
  }
#endif  // DCHECK_IS_ON()

  auto error_callback = base::BindOnce(
      &CommandStorageManager::OnErrorWritingToFile, weak_factory_.GetWeakPtr());
  EncryptSessionStorageStage encrypt_stage = GetEncryptSessionStorageStage();
  switch (encrypt_stage) {
    case EncryptSessionStorageStage::kClearOnly:
      backend_task_runner_->PostNonNestableTask(
          FROM_HERE, base::BindOnce(&CommandStorageBackend::AppendCommands,
                                    backend_, std::move(pending_commands_),
                                    pending_reset_, std::move(error_callback)));
      break;
    case EncryptSessionStorageStage::kWriteBothReadOnlyClear: {
      // The clear backend is the primary backend (called first, reports errors)
      // The encrypted backend is secondary (uses a copy of the commands).
      std::vector<std::unique_ptr<SessionCommand>> pending_commands_copy =
          DeepCopyCommands(pending_commands_);
      backend_task_runner_->PostNonNestableTask(
          FROM_HERE, base::BindOnce(&CommandStorageBackend::AppendCommands,
                                    backend_, std::move(pending_commands_),
                                    pending_reset_, std::move(error_callback)));
      if (encrypted_backend_) {
        backend_task_runner_->PostNonNestableTask(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::AppendCommands,
                           encrypted_backend_, std::move(pending_commands_copy),
                           pending_reset_, base::DoNothing()));
      } else {
        // This should be uncommon, but could occur if OnEncryptorReady is slow.
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kSave);
      }
      break;
    }
    case EncryptSessionStorageStage::kWriteBothReadPreferEncrypted: {
      // The clear backend is written first so that we can rollback to it if
      // the encrypted version has problems.  But otherwise the encrypted
      // backend is primary (e.g., for reporting errors).
      std::vector<std::unique_ptr<SessionCommand>> pending_commands_copy =
          DeepCopyCommands(pending_commands_);
      backend_task_runner_->PostNonNestableTask(
          FROM_HERE, base::BindOnce(&CommandStorageBackend::AppendCommands,
                                    backend_, std::move(pending_commands_copy),
                                    pending_reset_, base::DoNothing()));
      if (encrypted_backend_) {
        backend_task_runner_->PostNonNestableTask(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::AppendCommands,
                           encrypted_backend_, std::move(pending_commands_),
                           pending_reset_, std::move(error_callback)));
      } else {
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kSave);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(error_callback)));
      }
      break;
    }
    case EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted:
      if (encrypted_backend_) {
        backend_task_runner_->PostNonNestableTask(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::AppendCommands,
                           encrypted_backend_, std::move(pending_commands_),
                           pending_reset_, std::move(error_callback)));
      } else {
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kSave);
        base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(std::move(error_callback)));
      }
      break;
  }
  if (pending_reset_) {
    commands_since_reset_ = 0;
    pending_reset_ = false;
  }
}

bool CommandStorageManager::HasPendingSave() const {
  return weak_factory_for_timer_.HasWeakPtrs();
}

void CommandStorageManager::MoveCurrentSessionToLastSession() {
  Save();
  backend_task_runner_->PostNonNestableTask(
      FROM_HERE,
      base::BindOnce(&CommandStorageBackend::MoveCurrentSessionToLastSession,
                     backend_.get()));
  if (ShouldWriteEncryptedFiles()) {
    if (!encrypted_backend_) {
      // This should be uncommon, but could occur if OnEncryptorReady is slow.
      LogEncryptedBackendUninitialized(SessionEncryptedBackendUninitialized::
                                           kMoveCurrentSessionToLastSession);
      pending_encrypted_ops_.push_back(base::BindOnce(
          &CommandStorageManager::MoveCurrentSessionToLastSession,
          weak_factory_.GetWeakPtr()));
      return;
    }
    backend_task_runner_->PostNonNestableTask(
        FROM_HERE,
        base::BindOnce(&CommandStorageBackend::MoveCurrentSessionToLastSession,
                       encrypted_backend_.get()));
  }
}

void CommandStorageManager::DeleteLastSession() {
  backend_task_runner_->PostNonNestableTask(
      FROM_HERE, base::BindOnce(&CommandStorageBackend::DeleteLastSession,
                                backend_.get()));
  if (ShouldWriteEncryptedFiles()) {
    if (!encrypted_backend_) {
      // This should be uncommon, but could occur if OnEncryptorReady is slow.
      LogEncryptedBackendUninitialized(
          SessionEncryptedBackendUninitialized::kDeleteLastSession);
      pending_encrypted_ops_.push_back(
          base::BindOnce(&CommandStorageManager::DeleteLastSession,
                         weak_factory_.GetWeakPtr()));
      return;
    }
    backend_task_runner_->PostNonNestableTask(
        FROM_HERE, base::BindOnce(&CommandStorageBackend::DeleteLastSession,
                                  encrypted_backend_.get()));
  }
}

void CommandStorageManager::GetLastSessionCommands(
    GetCommandsCallback callback) {
  EncryptSessionStorageStage encrypt_stage = GetEncryptSessionStorageStage();
  switch (encrypt_stage) {
    case EncryptSessionStorageStage::kClearOnly:
      backend_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                         backend_.get()),
          base::BindOnce(&OnBackendReadFinished, /*saved_result=*/nullptr,
                         std::move(callback)));
      break;
    case EncryptSessionStorageStage::kWriteBothReadOnlyClear: {
      // Read the cleartext backend first and save the result so that we can
      // compare it to the result of the read from the encrypted backend.
      auto saved_cleartext_result = base::MakeRefCounted<
          base::RefCountedData<CommandStorageBackend::ReadCommandsResult>>();
      backend_task_runner_->PostTaskAndReplyWithResult(
          FROM_HERE,
          base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                         backend_.get()),
          base::BindOnce(&OnBackendReadFinished, saved_cleartext_result,
                         std::move(callback)));
      if (encrypted_backend_) {
        backend_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                           encrypted_backend_.get()),
            base::BindOnce(&CompareCleartextAndEncryptedReadResults,
                           saved_cleartext_result));
      } else {
        // This should be uncommon, but could occur if OnEncryptorReady is slow.
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kGetLastSessionCommands);
      }
      break;
    }
    case EncryptSessionStorageStage::kWriteBothReadPreferEncrypted: {
      if (encrypted_backend_) {
        // Read the cleartext backend first and save the result so that we can
        // compare it to the result of the read from the encrypted backend.
        auto saved_cleartext_result = base::MakeRefCounted<
            base::RefCountedData<CommandStorageBackend::ReadCommandsResult>>();
        backend_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                           backend_.get()),
            base::BindOnce(&OnBackendReadFinished, saved_cleartext_result,
                           base::DoNothing()));
        backend_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                           encrypted_backend_.get()),
            base::BindOnce(&OnEncryptedBackendReadFinishedWithPreferEncrypted,
                           saved_cleartext_result, backend_task_runner_,
                           backend_, std::move(callback)));
      } else {
        // This should be uncommon, but could occur if OnEncryptorReady is slow.
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kGetLastSessionCommands);
        pending_encrypted_ops_.push_back(
            base::BindOnce(&CommandStorageManager::GetLastSessionCommands,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
      }
      break;
    }
    case EncryptSessionStorageStage::kWriteEncryptedReadPreferEncrypted: {
      if (encrypted_backend_) {
        backend_task_runner_->PostTaskAndReplyWithResult(
            FROM_HERE,
            base::BindOnce(&CommandStorageBackend::ReadLastSessionCommands,
                           encrypted_backend_.get()),
            base::BindOnce(&OnEncryptedBackendReadFinishedWithPreferEncrypted,
                           /*saved_cleartext_result=*/nullptr,
                           backend_task_runner_, backend_,
                           std::move(callback)));
      } else {
        LogEncryptedBackendUninitialized(
            SessionEncryptedBackendUninitialized::kGetLastSessionCommands);
        pending_encrypted_ops_.push_back(
            base::BindOnce(&CommandStorageManager::GetLastSessionCommands,
                           weak_factory_.GetWeakPtr(), std::move(callback)));
      }
      break;
    }
  }
}

#if DCHECK_IS_ON()
base::Value CommandStorageManager::ToDebugValue() const {
  base::DictValue debug_value;
  for (const std::unique_ptr<SessionCommand>& command : pending_commands_) {
    debug_value.EnsureList("pending_commands")
        ->Append(CommandToDebugValue(*command));
  }
  debug_value.Set("pending_reset", pending_reset_);
  debug_value.Set("commands_since_reset", commands_since_reset_);
  for (const base::Value& log_item : written_commands_reverse_debug_log_) {
    debug_value.EnsureList("written_commands_reverse_debug_log")
        ->Append(log_item.Clone());
  }
  return base::Value(std::move(debug_value));
}
#endif  // DCHECK_IS_ON()

base::FilePath CommandStorageManager::GetBackendDirectoryForTesting(
    bool is_encrypted) {
  return file_path_.Append(is_encrypted ? kEncryptedSessionsDirectory
                                        : kSessionsDirectory);
}

void CommandStorageManager::OnErrorWritingToFile() {
  delegate_->OnErrorWritingSessionCommands();
}

}  // namespace sessions
