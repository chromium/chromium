// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_tasks.h"
#include "components/services/storage/indexed_db/scopes/scopes_metadata.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content::indexed_db {

// Cleanup tasks generally run in the background since they're just internal
// bookkeeping that shouldn't block other IDB operations. It has to block
// shutdown because the tasks will own a reference to a LevelDBState object,
// which MUST be destructed on shutdown as it will be joined with the IO
// thread on shutdown. To compensate here, all tasks cooperatively exit by
// checking `LevelDBState::is_destruction_requested()`.
static constexpr base::TaskTraits kCleanupTaskTraits = {
    base::MayBlock(), base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
    base::TaskPriority::USER_VISIBLE};

LevelDBScopes::LevelDBScopes(std::vector<uint8_t> metadata_key_prefix,
                             size_t max_write_batch_size_bytes,
                             scoped_refptr<LevelDBState> level_db,
                             PartitionedLockManager* lock_manager,
                             TearDownCallback tear_down_callback)
    : metadata_key_prefix_(std::move(metadata_key_prefix)),
      max_write_batch_size_bytes_(max_write_batch_size_bytes),
      level_db_(std::move(level_db)),
      lock_manager_(lock_manager),
      tear_down_callback_(std::move(tear_down_callback)) {}

LevelDBScopes::~LevelDBScopes() = default;

leveldb::Status LevelDBScopes::Initialize() {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(level_db_);
  DCHECK(!initialize_called_) << "Initialize() already called";
  initialize_called_ = true;
#endif  // DCHECK_IS_ON()

  leveldb::ReadOptions read_options;
  read_options.fill_cache = true;
  read_options.verify_checksums = true;
  leveldb::WriteOptions write_options;
  write_options.sync = false;
  ScopesEncoder key_encoder;
  leveldb::Status s;

  // This method loads the global metadata, reads in all of the scopes still on
  // disk, and stores their information for later cleanup or reverting. For all
  // reverting scopes, the appropriate locks are acquired now.

  // Step 1 - Load & initialize global metadata.
  std::string metadata_value;
  leveldb::Slice metadata_key =
      key_encoder.GlobalMetadataKey(metadata_key_prefix_);
  s = level_db_->db()->Get(read_options, metadata_key, &metadata_value);
  if (!s.ok() && !s.IsNotFound()) [[unlikely]] {
    return s;
  }

  LevelDBScopesMetadata metadata;
  if (s.IsNotFound()) {
    metadata.set_version(leveldb_scopes::kCurrentVersion);
    // This is the only 'write' operation that is done in this method, so a
    // leveldb::WriteBatch isn't necessary.
    s = level_db_->db()->Put(write_options, metadata_key,
                             metadata.SerializeAsString());
    if (!s.ok()) [[unlikely]] {
      return s;
    }
  } else {
    if (!metadata.ParseFromString(metadata_value)) {
      return leveldb::Status::Corruption(
          "Could not parse LevelDBScopes Metadata.");
    }
    if (metadata.version() < leveldb_scopes::kMinSupportedVersion ||
        metadata.version() > leveldb_scopes::kCurrentVersion) {
      return leveldb::Status::Corruption(
          base::StrCat({"Unsupported scopes metadata version ",
                        base::NumberToString(metadata.version())}));
    }
  }

  // Step 2 - Load scopes metadata & queue up revert or cleanup tasks, to be run
  // when StartRecoveryAndCleanupTasks() is called. All locks for the revert
  // tasks are acquired now.

  DCHECK(startup_scopes_to_clean_.empty());
  DCHECK(startup_scopes_to_revert_.empty());
  const std::unique_ptr<leveldb::Iterator> iterator =
      base::WrapUnique(level_db_->db()->NewIterator(read_options));
  leveldb::Slice prefix_key =
      key_encoder.ScopeMetadataPrefix(metadata_key_prefix_);
  iterator->Seek(prefix_key);
  LevelDBScopesScopeMetadata scope_metadata;
  for (; iterator->Valid() && iterator->key().starts_with(prefix_key);
       iterator->Next()) {
    // Parse the key & value.
    auto [success, scope_id] = leveldb_scopes::ParseScopeMetadataId(
        iterator->key(), metadata_key_prefix_);
    if (!success) [[unlikely]] {
      return leveldb::Status::Corruption(base::StrCat(
          {"Could not read scope metadata key: ", iterator->key().ToString()}));
    }
    if (!scope_metadata.ParseFromArray(iterator->value().data(),
                                       iterator->value().size())) [[unlikely]] {
      return leveldb::Status::Corruption(base::StrCat(
          {"Could not parse scope value key: ", iterator->value().ToString()}));
    }

    // The 'commit point' is not having any lock ranges in scope_metadata. If
    // lock ranges aren't present then it was committed, and the scope only
    // needs to be cleaned up.
    if (scope_metadata.locks_size() == 0) [[likely]] {
      startup_scopes_to_clean_.emplace_back(
          scope_id, scope_metadata.ignore_cleanup_tasks()
                        ? StartupCleanupType::kIgnoreCleanupTasks
                        : StartupCleanupType::kExecuteCleanupTasks);
      continue;
    }

    // The commit point isn't there, so that scope needs to be reverted.
    // Acquire all locks necessary to undo the scope to prevent user-created
    // scopes for reading or writing changes that will be undone.
    base::flat_set<PartitionedLockManager::PartitionedLockRequest>
        lock_requests;
    lock_requests.reserve(scope_metadata.locks().size());
    for (const auto& lock : scope_metadata.locks()) {
      PartitionedLockId lock_id;
      lock_id.partition = lock.partition();
      lock_id.key = lock.key().key();
      lock_requests.emplace(lock_id,
                            PartitionedLockManager::LockType::kExclusive);
      if (lock_manager_->TestLock(
              {lock_id, PartitionedLockManager::LockType::kExclusive}) !=
          PartitionedLockManager::TestLockResult::kFree) [[unlikely]] {
        return leveldb::Status::Corruption("Invalid locks on disk.");
      }
    }
    PartitionedLockHolder receiver;
    lock_manager_->AcquireLocks(std::move(lock_requests), receiver,
                                base::DoNothing());

    // AcquireLocks should grant the locks synchronously because
    // 1. There should be no locks acquired before calling this method, and
    // 2. All locks that were are being loaded from disk were previously 'held'
    //    by this system. If they conflict, this is an invalid state on disk.
    if (receiver.locks.empty()) [[unlikely]] {
      return leveldb::Status::Corruption("Invalid lock ranges on disk.");
    }

    startup_scopes_to_revert_.emplace_back(scope_id, std::move(receiver.locks));
  }
  if (iterator->status().ok()) [[likely]] {
    recovery_finished_ = true;
  }
  return s;
}

void LevelDBScopes::StartRecoveryAndCleanupTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Schedule all pending revert tasks ASAP. This is a delayed task so that it
  // doesn't delay IndexedDB::Open(), but it does need to be executed before any
  // other IDB operations/transactions execute.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](std::vector<StartupScopeToRevert> startup_scopes_to_revert,
             base::WeakPtr<LevelDBScopes> scopes) {
            if (!scopes) {
              return;
            }
            for (auto& [scope_id, locks] : startup_scopes_to_revert) {
              scopes->Rollback(scope_id, std::move(locks));
            }
          },
          std::move(startup_scopes_to_revert_), weak_factory_.GetWeakPtr()));

  // Schedule all committed scopes to be cleaned up.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kCleanupTaskTraits,
      base::BindOnce(
          [](std::vector<StartupScopeToCleanup> startup_scopes_to_clean,
             scoped_refptr<LevelDBState> level_db,
             std::vector<uint8_t> metadata_key_prefix,
             size_t max_write_batch_size_bytes) {
            for (auto& [scope_id, cleanup_mode] : startup_scopes_to_clean) {
              leveldb::Status result =
                  CleanupScopeTask(
                      level_db, metadata_key_prefix, scope_id,
                      cleanup_mode == StartupCleanupType::kExecuteCleanupTasks
                          ? CleanupScopeTask::CleanupMode::kExecuteCleanupTasks
                          : CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
                      max_write_batch_size_bytes)
                      .Run();
              if (!result.ok()) [[unlikely]] {
                return result;
              }
            }
            return leveldb::Status::OK();
          },
          std::move(startup_scopes_to_clean_), level_db_, metadata_key_prefix_,
          max_write_batch_size_bytes_),
      base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                     weak_factory_.GetWeakPtr(), base::OnceClosure()));
}

std::unique_ptr<LevelDBScope> LevelDBScopes::CreateScope(
    std::vector<PartitionedLock> locks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(recovery_finished_);
  int scope_id = next_scope_id_;
  ++next_scope_id_;
  auto rollback_callback =
      base::BindOnce(&LevelDBScopes::Rollback, weak_factory_.GetWeakPtr());
  return base::WrapUnique(new LevelDBScope(
      scope_id, metadata_key_prefix_, max_write_batch_size_bytes_, level_db_,
      std::move(locks), std::move(rollback_callback)));
}

leveldb::Status LevelDBScopes::Commit(std::unique_ptr<LevelDBScope> scope,
                                      bool sync_on_commit,
                                      base::OnceClosure on_commit_complete,
                                      base::OnceClosure on_cleanup_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(recovery_finished_);
  auto [status, scopes_mode] = scope->Commit(sync_on_commit);
  if (on_commit_complete) {
    std::move(on_commit_complete).Run();
  }
  if (scopes_mode == LevelDBScope::Mode::kUndoLogOnDisk) {
    auto task = std::make_unique<CleanupScopeTask>(
        level_db_, metadata_key_prefix_, scope->scope_id(),
        CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
        max_write_batch_size_bytes_);
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, kCleanupTaskTraits,
        base::BindOnce(&CleanupScopeTask::Run, std::move(task)),
        base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                       weak_factory_.GetWeakPtr(),
                       std::move(on_cleanup_complete)));
  }
  return status;
}

void LevelDBScopes::Rollback(int64_t scope_id,
                             std::vector<PartitionedLock> locks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status result =
      RevertScopeTask(level_db_, metadata_key_prefix_, scope_id,
                      max_write_batch_size_bytes_)
          .Run();
  if (!result.ok()) [[unlikely]] {
    // Prospective fix for crbug.com/350196532: synchronous teardown seems to
    // cause issues.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(tear_down_callback_, result));
    return;
  }

  auto task = std::make_unique<CleanupScopeTask>(
      level_db_, metadata_key_prefix_, scope_id,
      CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
      max_write_batch_size_bytes_);
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kCleanupTaskTraits,
      base::BindOnce(&CleanupScopeTask::Run, std::move(task)),
      base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                     weak_factory_.GetWeakPtr(), base::OnceClosure()));
}

void LevelDBScopes::OnCleanupTaskResult(base::OnceClosure on_complete,
                                        leveldb::Status result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!result.ok()) [[unlikely]] {
    tear_down_callback_.Run(result);
  }
  if (on_complete) {
    std::move(on_complete).Run();
  }
}

}  // namespace content::indexed_db
