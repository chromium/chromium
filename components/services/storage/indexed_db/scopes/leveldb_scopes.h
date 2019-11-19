// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_

#include <stdint.h>
#include <limits>
#include <list>
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/scope_lock.h"
#include "components/services/storage/indexed_db/scopes/scope_lock_range.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content {
class LevelDBScope;
class LevelDBState;
class ScopesLockManager;

class LevelDBScopes {
 public:
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using EmptyRange = std::pair<std::string, std::string>;
  static constexpr const size_t kDefaultMaxWriteBatchSizeBytes = 1024 * 1024;

  enum class TaskRunnerMode {
    // No new sequence runners are created. Both the cleanup and the revert
    // tasks are run using the sequence runner that is calling this class.
    kUseCurrentSequence,
    // A new sequence runner is created for both the cleanup tasks and the
    // revert tasks.
    kNewCleanupAndRevertSequences,
  };

  // |lock_manager| is expected to be alive during the lifetime of this class.
  // |tear_down_callback| will not be called after the destruction of this
  // class.
  LevelDBScopes(std::vector<uint8_t> metadata_key_prefix,
                size_t max_write_batch_size_bytes_bytes,
                scoped_refptr<LevelDBState> level_db,
                ScopesLockManager* lock_manager,
                TearDownCallback tear_down_callback);
  ~LevelDBScopes();

  // This method needs to be called before any other method on this class. If
  // unsuccessful, the class cannot be used. Note, this will acquire locks for
  // the revert tasks if necessary.
  leveldb::Status Initialize();

  // This starts (or adopts) the task runners associated with aborting and
  // cleaning up previous logs based on the given |mode|, and schedules any
  // pending cleanup or revert tasks.
  // Returns any errors that might occur during revert if |mode| is
  // kUseCurrentSequence.
  leveldb::Status StartRecoveryAndCleanupTasks(TaskRunnerMode mode);

  // In |empty_ranges|, |pair.first| is the inclusive range begin, and
  // |pair.end| is the exclusive range end. The ranges must be disjoint (they
  // cannot overlap).
  std::unique_ptr<LevelDBScope> CreateScope(
      std::vector<ScopeLock> locks,
      std::vector<EmptyRange> empty_ranges);

  leveldb::Status Commit(std::unique_ptr<LevelDBScope> scope,
                         bool sync_on_commit);

  // |on_complete| will be called when the cleanup task for the scope has
  // finished operating.
  leveldb::Status Commit(std::unique_ptr<LevelDBScope> scope,
                         bool sync_on_commit,
                         base::OnceClosure on_complete);

  base::SequencedTaskRunner* RevertRunnerForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return revert_runner_.get();
  }

  base::SequencedTaskRunner* CleanupRunnerForTesting() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return cleanup_runner_.get();
  }

  const std::vector<uint8_t>& metadata_key_prefix() const {
    return metadata_key_prefix_;
  }

  const TearDownCallback& tear_down_callback() { return tear_down_callback_; }

 private:
  enum class StartupCleanupType { kExecuteCleanupTasks, kIgnoreCleanupTasks };
  using StartupScopeToRevert = std::pair<int64_t, std::vector<ScopeLock>>;
  using StartupScopeToCleanup = std::pair<int64_t, StartupCleanupType>;
  using RecoveryLocksList = std::list<std::vector<ScopeLock>>;

  leveldb::Status InitializeGlobalMetadata(
      const leveldb::ReadOptions& read_options,
      const leveldb::WriteOptions& write_options);
  leveldb::Status InitializeScopesAndTasks(
      const leveldb::ReadOptions& read_options,
      const leveldb::WriteOptions& write_options);

  // If the mode is TaskRunnerMode::kUseCurrentSequence, then the result of the
  // revert task is returned.
  leveldb::Status Rollback(int64_t scope_id, std::vector<ScopeLock> locks);

  void OnCleanupTaskResult(base::OnceClosure on_complete,
                           leveldb::Status result);

  void StartRevertTask(int64_t scope_id, std::vector<ScopeLock> locks);

  void OnRevertTaskResult(int64_t scope_id,
                          std::vector<ScopeLock> locks,
                          leveldb::Status result);

  SEQUENCE_CHECKER(sequence_checker_);
  const std::vector<uint8_t> metadata_key_prefix_;
  const size_t max_write_batch_size_bytes_;
  std::vector<StartupScopeToCleanup> startup_scopes_to_clean_;
  std::vector<StartupScopeToRevert> startup_scopes_to_revert_;
  scoped_refptr<base::SequencedTaskRunner> revert_runner_;
  scoped_refptr<base::SequencedTaskRunner> cleanup_runner_;

  // This gets set to |true| when |Initialize()| succeeds.
  bool recovery_finished_ = false;
  int next_scope_id_ = 0;
  scoped_refptr<LevelDBState> level_db_;
  // The |lock_manager_| is expected to outlive this class.
  ScopesLockManager* lock_manager_;
  TearDownCallback tear_down_callback_;

#if DCHECK_IS_ON()
  bool initialize_called_ = false;
#endif

  base::WeakPtrFactory<LevelDBScopes> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(LevelDBScopes);
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_
