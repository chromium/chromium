// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes.h"

#include <memory>
#include <string>
#include <utility>

#include "base/barrier_closure.h"
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
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content {

LevelDBScopes::LevelDBScopes(std::vector<uint8_t> metadata_key_prefix,
                             size_t max_write_batch_size,
                             scoped_refptr<LevelDBState> level_db,
                             PartitionedLockManager* lock_manager,
                             TearDownCallback tear_down_callback)
    : metadata_key_prefix_(std::move(metadata_key_prefix)),
      max_write_batch_size_bytes_(max_write_batch_size),
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
  if (UNLIKELY(!s.ok() && !s.IsNotFound()))
    return s;

  LevelDBScopesMetadata metadata;
  if (s.IsNotFound()) {
    metadata.set_version(leveldb_scopes::kCurrentVersion);
    // This is the only 'write' operation that is done in this method, so a
    // leveldb::WriteBatch isn't necessary.
    s = level_db_->db()->Put(write_options, metadata_key,
                             metadata.SerializeAsString());
    if (UNLIKELY(!s.ok()))
      return s;
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
    if (UNLIKELY(!success)) {
      return leveldb::Status::Corruption(base::StrCat(
          {"Could not read scope metadata key: ", iterator->key().ToString()}));
    }
    if (UNLIKELY(!scope_metadata.ParseFromArray(iterator->value().data(),
                                                iterator->value().size()))) {
      return leveldb::Status::Corruption(base::StrCat(
          {"Could not parse scope value key: ", iterator->value().ToString()}));
    }

    // The 'commit point' is not having any lock ranges in scope_metadata. If
    // lock ranges aren't present then it was committed, and the scope only
    // needs to be cleaned up.
    if (LIKELY(scope_metadata.locks_size() == 0)) {
      startup_scopes_to_clean_.emplace_back(
          scope_id, scope_metadata.ignore_cleanup_tasks()
                        ? StartupCleanupType::kIgnoreCleanupTasks
                        : StartupCleanupType::kExecuteCleanupTasks);
      continue;
    }

    // The commit point isn't there, so that scope needs to be reverted.
    // Acquire all locks necessary to undo the scope to prevent user-created
    // scopes for reading or writing changes that will be undone.
    PartitionedLockId lock_id;
    base::flat_set<PartitionedLockManager::PartitionedLockRequest>
        lock_requests;
    lock_requests.reserve(scope_metadata.locks().size());
    for (const auto& lock : scope_metadata.locks()) {
      lock_id.partition = lock.partition();
      lock_id.key = lock.key().key();
      lock_requests.emplace(lock_id,
                            PartitionedLockManager::LockType::kExclusive);
      if (UNLIKELY(
              lock_manager_->TestLock(
                  {lock_id, PartitionedLockManager::LockType::kExclusive}) !=
              PartitionedLockManager::TestLockResult::kFree)) {
        return leveldb::Status::Corruption("Invalid locks on disk.");
      }
    }
    PartitionedLockHolder receiver;
    lock_manager_->AcquireLocks(std::move(lock_requests),
                                receiver.weak_factory.GetWeakPtr(),
                                base::DoNothing());

    // AcquireLocks should grant the locks synchronously because
    // 1. There should be no locks acquired before calling this method, and
    // 2. All locks that were are being loaded from disk were previously 'held'
    //    by this system. If they conflict, this is an invalid state on disk.
    if (UNLIKELY(receiver.locks.empty()))
      return leveldb::Status::Corruption("Invalid lock ranges on disk.");

    startup_scopes_to_revert_.emplace_back(scope_id, std::move(receiver.locks));
  }
  if (LIKELY(iterator->status().ok()))
    recovery_finished_ = true;
  return s;
}

void LevelDBScopes::StartRecoveryAndCleanupTasks() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!revert_runner_) << "StartRecoveryAndCleanupTasks() already called.";
  DCHECK(!cleanup_runner_);

  // There are many choices for how to run these tasks. They technically could
  // be done on a threadpool, where each task is in its own thread. Because both
  // of these task types are triggered by the code on a webpage, it is dangerous
  // to let them completely fill up a threadpool.
  // The cleanup tasks are important to run because they will result in disk
  // space shrinkage, especially when they have compaction tasks. This affects
  // the webpage quota.
  // The revert tasks are very important because they still hold a lock to that
  // object store or database. This can completely block website database
  // operations from happening.
  // The compromise here is:
  // It is OK to mark these priorities as somewhat high(blocking and visible)
  // as long as each task type only uses one sequence. This makes sure that the
  // tasks cannot monopolize the entire thread pool, and that they will be run
  // reasonably soon.
  // Finally, it is also required that these block shutdown. This is because
  // these tasks will own a reference to a LevelDBState object, which is MUST be
  // destructed on shutdown as it will be joined with the IO thread on shutdown.
  // To compensate here, all tasks cooperatively exit by checking
  // `LevelDBState::is_destruction_requested()`
  revert_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
       base::TaskPriority::USER_BLOCKING});
  cleanup_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::MayBlock(), base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::BLOCK_SHUTDOWN,
       base::TaskPriority::USER_VISIBLE});

  // Schedule all pending revert tasks ASAP.
  for (StartupScopeToRevert& revert_scope_data : startup_scopes_to_revert_) {
    Rollback(revert_scope_data.first, std::move(revert_scope_data.second));
  }
  startup_scopes_to_revert_.clear();

  // Schedule all committed scopes to be cleaned up.
  for (auto& cleanup_scope_data : startup_scopes_to_clean_) {
    auto cleanup_task = std::make_unique<CleanupScopeTask>(
        level_db_, metadata_key_prefix_, cleanup_scope_data.first,
        cleanup_scope_data.second == StartupCleanupType::kExecuteCleanupTasks
            ? CleanupScopeTask::CleanupMode::kExecuteCleanupTasks
            : CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
        max_write_batch_size_bytes_);
    cleanup_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&CleanupScopeTask::Run, std::move(cleanup_task)),
        base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                       weak_factory_.GetWeakPtr(), base::OnceClosure()));
  }
  startup_scopes_to_clean_.clear();
}

std::unique_ptr<LevelDBScope> LevelDBScopes::CreateScope(
    std::vector<PartitionedLock> locks,
    std::vector<std::pair<std::string, std::string>> empty_ranges) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(recovery_finished_);
  int scope_id = next_scope_id_;
  ++next_scope_id_;
  auto rollback_callback = base::BindOnce(
      [](base::WeakPtr<LevelDBScopes> scopes, int64_t scope_id,
         std::vector<PartitionedLock> locks) {
        if (scopes) {
          scopes->Rollback(scope_id, std::move(locks));
        }
      },
      weak_factory_.GetWeakPtr());
  return base::WrapUnique(new LevelDBScope(
      scope_id, metadata_key_prefix_, max_write_batch_size_bytes_, level_db_,
      std::move(locks), std::move(empty_ranges), std::move(rollback_callback),
      tear_down_callback_));
}

leveldb::Status LevelDBScopes::Commit(std::unique_ptr<LevelDBScope> scope,
                                      bool sync_on_commit) {
  return Commit(std::move(scope), sync_on_commit, base::OnceClosure());
}

leveldb::Status LevelDBScopes::Commit(std::unique_ptr<LevelDBScope> scope,
                                      bool sync_on_commit,
                                      base::OnceClosure on_complete) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(recovery_finished_);
  DCHECK(cleanup_runner_);
  auto [status, scopes_mode] = scope->Commit(sync_on_commit);
  if (scopes_mode == LevelDBScope::Mode::kUndoLogOnDisk) {
    auto task = std::make_unique<CleanupScopeTask>(
        level_db_, metadata_key_prefix_, scope->scope_id(),
        CleanupScopeTask::CleanupMode::kExecuteCleanupTasks,
        max_write_batch_size_bytes_);
    cleanup_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&CleanupScopeTask::Run, std::move(task)),
        base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                       weak_factory_.GetWeakPtr(), std::move(on_complete)));
  }
  return status;
}

void LevelDBScopes::Rollback(int64_t scope_id,
                             std::vector<PartitionedLock> locks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  auto task = std::make_unique<RevertScopeTask>(
      level_db_, metadata_key_prefix_, scope_id, max_write_batch_size_bytes_);

  revert_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&RevertScopeTask::Run, std::move(task)),
      base::BindOnce(&LevelDBScopes::OnRevertTaskResult,
                     weak_factory_.GetWeakPtr(), scope_id, std::move(locks)));
}

void LevelDBScopes::OnCleanupTaskResult(base::OnceClosure on_complete,
                                        leveldb::Status result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (UNLIKELY(!result.ok()))
    tear_down_callback_.Run(result);
  if (on_complete)
    std::move(on_complete).Run();
}

void LevelDBScopes::OnRevertTaskResult(int64_t scope_id,
                                       std::vector<PartitionedLock> locks,
                                       leveldb::Status result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (UNLIKELY(!result.ok())) {
    tear_down_callback_.Run(result);
    return;
  }
  auto task = std::make_unique<CleanupScopeTask>(
      level_db_, metadata_key_prefix_, scope_id,
      CleanupScopeTask::CleanupMode::kIgnoreCleanupTasks,
      max_write_batch_size_bytes_);
  cleanup_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&CleanupScopeTask::Run, std::move(task)),
      base::BindOnce(&LevelDBScopes::OnCleanupTaskResult,
                     weak_factory_.GetWeakPtr(), base::OnceClosure()));
}

}  // namespace content
