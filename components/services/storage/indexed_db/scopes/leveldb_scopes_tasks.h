// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TASKS_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TASKS_H_

#include <stdint.h>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace leveldb {
struct ReadOptions;
struct WriteOptions;
}  // namespace leveldb

namespace content {

// This base class is thread-compatible, and is designed to be constructed on
// one thread, and then used & destroyed on another.
class LevelDBScopesTask {
 public:
  explicit LevelDBScopesTask(scoped_refptr<LevelDBState> level_db,
                             size_t max_write_batch_size_bytes);
  ~LevelDBScopesTask();

 protected:
  // Submits the in-progress WriteBatch to LevelDB, no matter what size the
  // batch is.
  leveldb::Status SubmitWriteBatch(const leveldb::WriteOptions& options)
      WARN_UNUSED_RESULT;
  // Submits thein-progress WriteBatch to LevelDB only if the approximate size
  // of the batch is > |max_write_batch_size_|.
  leveldb::Status MaybeSubmitWriteBatch(const leveldb::WriteOptions& options)
      WARN_UNUSED_RESULT;

  leveldb::Status DeleteRange(leveldb::Slice range_start,
                              leveldb::Slice range_end,
                              const leveldb::ReadOptions& read_options,
                              const leveldb::WriteOptions& write_options)
      WARN_UNUSED_RESULT;

  SEQUENCE_CHECKER(sequence_checker_);

  const scoped_refptr<LevelDBState> level_db_;
  const size_t max_write_batch_size_bytes_;
  leveldb::WriteBatch write_batch_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LevelDBScopesTask);
};

// Deletes the undo log for a given scope, and optionally executes the cleanup
// tasks recorded in the scope.
//
// Note: Tasks are constructed on one thread and then Run() and destroyed on a
// separate thread.
class CleanupScopeTask : private LevelDBScopesTask {
 public:
  enum class CleanupMode {
    // Used after a scope is committed.
    kExecuteCleanupTasks,
    // Used after a scope is reverted.
    kIgnoreCleanupTasks,
  };

  CleanupScopeTask(scoped_refptr<LevelDBState> level_db,
                   std::vector<uint8_t> metadata_prefix,
                   int64_t scope_number,
                   CleanupMode mode,
                   size_t max_write_batch_size_bytes);
  ~CleanupScopeTask();

  leveldb::Status Run() WARN_UNUSED_RESULT;

 private:
  leveldb::Status ExecuteAndDeleteCleanupTasks(
      const leveldb::ReadOptions& read_options,
      const leveldb::WriteOptions& write_options) WARN_UNUSED_RESULT;
  leveldb::Status DeletePrefixedRange(
      leveldb::Slice prefix,
      const leveldb::ReadOptions& read_options,
      const leveldb::WriteOptions& write_options) WARN_UNUSED_RESULT;

  const std::vector<uint8_t> metadata_prefix_;
  const int64_t scope_number_;
  const CleanupMode mode_;
};

// This task executes & deletes the undo tasks for the given scope, and then
// marks the scope as committed. It should then be cleaned up by a
// CleanupScopeTask that ignores the cleanup tasks (kIgnoreCleanupTasks).
//
// Note: Tasks are constructed on one thread and then |Run| and destroyed on a
// separate thread.
class RevertScopeTask : private LevelDBScopesTask {
 public:
  RevertScopeTask(scoped_refptr<LevelDBState> level_db,
                  std::vector<uint8_t> metadata_prefix,
                  int64_t scope_number,
                  size_t max_write_batch_size_bytes);
  ~RevertScopeTask();

  leveldb::Status Run() WARN_UNUSED_RESULT;

 private:
  const std::vector<uint8_t> metadata_prefix_;
  const int64_t scope_number_;
};

}  // namespace content

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_TASKS_H_
