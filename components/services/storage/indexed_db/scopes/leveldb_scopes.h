// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_

#include <stdint.h>
#include <list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "third_party/leveldatabase/src/include/leveldb/options.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"

namespace content::indexed_db {
class LevelDBScope;
class LevelDBState;
class PartitionedLockManager;

class LevelDBScopes {
 public:
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using EmptyRange = std::pair<std::string, std::string>;

  // |lock_manager| is expected to be alive during the lifetime of this class.
  // |tear_down_callback| will not be called after the destruction of this
  // class.
  LevelDBScopes(std::vector<uint8_t> metadata_key_prefix,
                size_t max_write_batch_size_bytes,
                scoped_refptr<LevelDBState> level_db,
                PartitionedLockManager* lock_manager,
                TearDownCallback tear_down_callback);

  LevelDBScopes(const LevelDBScopes&) = delete;
  LevelDBScopes& operator=(const LevelDBScopes&) = delete;

  ~LevelDBScopes();

  // This method needs to be called before any other method on this class. If
  // unsuccessful, the class cannot be used. Note, this will acquire locks for
  // the revert tasks if necessary.
  leveldb::Status Initialize();

  // Schedules any pending cleanup or revert tasks.
  void StartRecoveryAndCleanupTasks();

  std::unique_ptr<LevelDBScope> CreateScope(std::vector<PartitionedLock> locks);

  // `on_commit_complete` will be called after the commit for `scope` completes
  // but before the cleanup task (if applicable) is scheduled.
  // `on_cleanup_complete` will be called when the cleanup task completes. It
  // will not be called if the task was not scheduled at all.
  leveldb::Status Commit(
      std::unique_ptr<LevelDBScope> scope,
      bool sync_on_commit,
      base::OnceClosure on_commit_complete = base::OnceClosure(),
      base::OnceClosure on_cleanup_complete = base::OnceClosure());

  const std::vector<uint8_t>& metadata_key_prefix() const {
    return metadata_key_prefix_;
  }

  const TearDownCallback& tear_down_callback() { return tear_down_callback_; }

 private:
  enum class StartupCleanupType { kExecuteCleanupTasks, kIgnoreCleanupTasks };
  using StartupScopeToRevert = std::pair<int64_t, std::vector<PartitionedLock>>;
  using StartupScopeToCleanup = std::pair<int64_t, StartupCleanupType>;
  using RecoveryLocksList = std::list<std::vector<PartitionedLock>>;

  void Rollback(int64_t scope_id, std::vector<PartitionedLock> locks);

  void OnCleanupTaskResult(base::OnceClosure on_complete,
                           leveldb::Status result);

  SEQUENCE_CHECKER(sequence_checker_);
  const std::vector<uint8_t> metadata_key_prefix_;
  const size_t max_write_batch_size_bytes_;
  std::vector<StartupScopeToCleanup> startup_scopes_to_clean_;
  std::vector<StartupScopeToRevert> startup_scopes_to_revert_;

  // This gets set to |true| when |Initialize()| succeeds.
  bool recovery_finished_ = false;
  int next_scope_id_ = 0;
  scoped_refptr<LevelDBState> level_db_;
  // The |lock_manager_| is expected to outlive this class.
  raw_ptr<PartitionedLockManager> lock_manager_;
  TearDownCallback tear_down_callback_;

#if DCHECK_IS_ON()
  bool initialize_called_ = false;
#endif

  base::WeakPtrFactory<LevelDBScopes> weak_factory_{this};
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_H_
