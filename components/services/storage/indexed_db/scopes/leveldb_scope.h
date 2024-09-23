// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_H_

#include <stdint.h>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/checked_math.h"
#include "base/sequence_checker.h"
#include "components/services/storage/indexed_db/leveldb/leveldb_state.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scope_deletion_mode.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/scopes_metadata.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"
#include "third_party/leveldatabase/src/include/leveldb/status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace content::indexed_db {

// LevelDBScope is a specialized type of transaction used only for writing data,
// and is created using the |LevelDBScopes::CreateScope| method.
// It has optimizations like:
// * Supporting deferred deletion for ranges that will never be accessed again.
//   This guarantees the data will be deleted, but it will delete it on a
//   different thread sometime in the future.
// * Storing changes in leveldb instead of in memory, so the user doesn't have
//   to worry about having a set of changes that gets too big.
// * Not bothering looking up 'undo' optimizations for empty ranges.
// To support this, the following invariants have to be followed:
// * The locks the scope was created with should protect it from colliding with
//   modifications by other parties.
// * All 'Get' operations in the range/s locked by this scope will first call
//   |WriteChangesAndUndoLog()| so all writes are submitted to LevelDB.
// * Any ranges deleted with |kDeferred| or |kDeferredWithCompaction| could
//   continue to have the data for a while, so they should never be accessed
//   again.
//
// Error handling on |Put|, |Delete*|, and |WriteChangesAndUndoLog|:
// * After a corruption error (Status::IsCorruption() returns true), all
//   operations on the scope must cease, commit cannot be called, and the whole
//   database state must be wiped.
// * After an IO error (Status::IsIOError() returns true), all operations on the
//   scope must cease, commit cannot be called, but the scope can attempt to be
//   reverted. If the revert fails for any reason, this is treated as a
//   corruption. A better route is to immediately abort all usage of the leveldb
//   state and close the database. A fresh database open could resolve the IO
//   error.
//
// This class is not thread safe, and should be used on the same sequence as
// |LevelDBScopes::CreateScope|.
class LevelDBScope {
 public:
  using RollbackCallback =
      base::OnceCallback<void(int64_t scope_id,
                              std::vector<PartitionedLock> locks)>;
  using TearDownCallback = base::RepeatingCallback<void(leveldb::Status)>;
  using CleanupCallback = base::OnceCallback<void(int64_t scope_id)>;

  LevelDBScope(const LevelDBScope&) = delete;
  LevelDBScope& operator=(const LevelDBScope&) = delete;

  ~LevelDBScope();

  int64_t scope_id() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return scope_id_;
  }

  [[nodiscard]] leveldb::Status Put(const leveldb::Slice& key,
                                    const leveldb::Slice& value);
  [[nodiscard]] leveldb::Status Delete(const leveldb::Slice& key);

  // Deletes the range. |begin| is always inclusive. See
  // |LevelDBScopeDeletionMode| for the different types of range deletion.
  [[nodiscard]] leveldb::Status DeleteRange(const leveldb::Slice& begin,
                                            const leveldb::Slice& end,
                                            LevelDBScopeDeletionMode mode);
  // Submits pending changes & the undo log to LevelDB. Required to be able to
  // read any keys that have been submitted to |Put|, |Delete|, or
  // |DeleteRange|.
  [[nodiscard]] leveldb::Status WriteChangesAndUndoLog();

  // In the case of LevelDBScopes being in the mode
  // TaskRunnerMode::kUseCurrentSequence, rollbacks happen synchronously. The
  // status of this possibly synchronous rollback is returned.
  void Rollback();

  uint64_t GetMemoryUsage() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return buffer_batch_.ApproximateSize();
  }

  uint64_t GetApproximateBytesWritten() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return approximate_bytes_written_.ValueOrDie();
  }

 private:
  friend class LevelDBScopes;
  class UndoLogWriter;

  enum class Mode { kInMemory, kUndoLogOnDisk };

  bool IsUndoLogMode() const { return mode_ == Mode::kUndoLogOnDisk; }

  LevelDBScope(int64_t scope_id,
               std::vector<uint8_t> prefix,
               size_t write_batch_size,
               scoped_refptr<LevelDBState> level_db,
               std::vector<PartitionedLock> locks,
               RollbackCallback rollback_callback);

  // Called by LevelDBScopes. Saves all data, release all locks, and returns the
  // status & the mode of this scope. The caller (LevelDBScopes) is expected to
  // queue up a cleanup task if the mode is kUndoLogOnDisk. This instance should
  // not be used after this call.
  [[nodiscard]] std::pair<leveldb::Status, Mode> Commit(bool sync_on_commit);

  // Submits pending changes & the undo log to LevelDB. Required to be able to
  // read any keys that have been submitted to Put, Delete, or
  // DeleteRange. |sync| makes the write a synchronous write.
  leveldb::Status WriteChangesAndUndoLogInternal(bool sync);

  void AddUndoPutTask(std::string key, std::string value);
  void AddUndoDeleteTask(std::string key);
  void AddUndoDeleteRangeTask(std::string begin, std::string end);
  // Writes the current |undo_task_buffer_| to the |write_batch_|, and
  // decrements the |undo_sequence_number_|.
  void AddBufferedUndoTask();

  void AddCleanupDeleteRangeTask(std::string begin, std::string end);
  void AddCleanupDeleteAndCompactRangeTask(std::string begin, std::string end);
  // Writes the current |cleanup_task_buffer_| to the |write_batch_|, and
  // decrements the |cleanup_sequence_number_|.
  void AddBufferedCleanupTask();

  void SetModeToUndoLog();

  // Returns true if the the given key is either part of the scope metadata
  // (starts with |prefix_|) or is in a known empty range. If true, then this
  // key should NOT have an undo entry generated.
  // On the first match of an empty range, the DELETE_RANGE undo log entry is
  // written to the buffer_batch_. Since that range is known to be empty, all
  // keys within it can be reversed by a delete range operation, and only one
  // delete range per empty range is needed.
  bool CanSkipWritingUndoEntry(const leveldb::Slice& key);

  void AddCommitPoint();
  [[nodiscard]] leveldb::Status WriteBufferBatch(bool sync);

#if DCHECK_IS_ON()
  std::vector<std::pair<std::string, std::string>> deferred_delete_ranges_;
  bool IsInDeferredDeletionRange(const leveldb::Slice& key);
#endif

  SEQUENCE_CHECKER(sequence_checker_);
  const int64_t scope_id_;
  // The undo tasks are written in reverse, as they are executed in ascending
  // order (and they need to be executed in reverse).
  int64_t undo_sequence_number_ = leveldb_scopes::kFirstSequenceNumberToWrite;
  // The cleanup tasks are written in order, and will be executed in the same
  // order.
  int64_t cleanup_sequence_number_ = 0ll;
  Mode mode_ = Mode::kInMemory;
  const std::vector<uint8_t> prefix_;
  const size_t write_batch_size_;
  const scoped_refptr<LevelDBState> level_db_;
  std::vector<PartitionedLock> locks_;
  RollbackCallback rollback_callback_;

  leveldb::WriteBatch buffer_batch_;
  bool buffer_batch_empty_ = true;
  base::CheckedNumeric<uint64_t> approximate_bytes_written_ = 0;
  bool has_written_to_disk_ = false;
  bool committed_ = false;

  LevelDBScopesUndoTask undo_task_buffer_;
  LevelDBScopesCleanupTask cleanup_task_buffer_;
  ScopesEncoder key_encoder_;
  std::string value_buffer_;
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPE_H_
