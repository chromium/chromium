// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scope.h"

#include <limits>
#include <memory>
#include <sstream>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/debug/stack_trace.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "third_party/leveldatabase/src/include/leveldb/comparator.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace content::indexed_db {
namespace {

#if DCHECK_IS_ON()
leveldb::Slice Uint8VectorToSlice(const std::vector<uint8_t>& vector) {
  return leveldb::Slice(reinterpret_cast<const char*>(vector.data()),
                        vector.size());
}
#endif

// Tests if the given key is before the end of a range.
bool IsKeyBeforeEndOfRange(const leveldb::Comparator* comparator,
                           const leveldb::Slice& key,
                           const leveldb::Slice& end,
                           bool end_exclusive) {
  return (end_exclusive ? comparator->Compare(key, end) < 0
                        : comparator->Compare(key, end) <= 0);
}
}  // namespace

// Adds undo log tasks to the given LevelDBScope for every entry found in the
// WriteBatch that this is iterating.
// Taken partially, the resulting undo log is technically incorrect. Two
// operations for the same key, for example Put(K, V1) and Put(K, V2), will
// result in an undo log containing either Put(K, old_value_for_k) twice or
// Delete(K) twice. This is OK, because recovery always applies the entire undo
// log, so it only matters that each key's final operation is correct.
class LevelDBScope::UndoLogWriter : public leveldb::WriteBatch::Handler {
 public:
  UndoLogWriter(LevelDBScope* scope, leveldb::DB* db)
      : scope_(scope), db_(db) {}
  ~UndoLogWriter() override = default;

  void Put(const leveldb::Slice& key, const leveldb::Slice& value) override {
    DCHECK(scope_->IsUndoLogMode());
    if (!error_.ok()) [[unlikely]] {
      return;
    }
    if (scope_->CanSkipWritingUndoEntry(key))
      return;
    leveldb::ReadOptions read_options;
    read_options.verify_checksums = true;
    // Since the values being read here are going to be overwritten as soon as
    // the write batch is written, the block will basically be obsolete. Thus,
    // don't bother caching.
    read_options.fill_cache = false;
    read_buffer_.clear();
    leveldb::Status s = db_->Get(read_options, key, &read_buffer_);
    if (s.IsNotFound()) {
      scope_->AddUndoDeleteTask(key.ToString());
      return;
    }
    if (!s.ok()) [[unlikely]] {
      error_ = std::move(s);
      return;
    }
    scope_->AddUndoPutTask(key.ToString(), std::move(read_buffer_));
  }

  void Delete(const leveldb::Slice& key) override {
    DCHECK(scope_->IsUndoLogMode());
    if (!error_.ok()) [[unlikely]] {
      return;
    }
    if (scope_->CanSkipWritingUndoEntry(key))
      return;
    leveldb::ReadOptions read_options;
    read_options.verify_checksums = true;
    // Since the values being read here are going to be overwritten as soon as
    // the write batch is written, the block will basically be obsolete. Thus,
    // don't bother caching.
    read_options.fill_cache = false;
    read_buffer_.clear();
    leveldb::Status s = db_->Get(read_options, key, &read_buffer_);
    if (s.IsNotFound())
      return;
    if (!s.ok()) [[unlikely]] {
      error_ = std::move(s);
      return;
    }
    scope_->AddUndoPutTask(key.ToString(), std::move(read_buffer_));
  }

  const leveldb::Status& error() const { return error_; }

 private:
  const raw_ptr<LevelDBScope> scope_;
  const raw_ptr<leveldb::DB> db_;
  std::string read_buffer_;
  leveldb::Status error_ = leveldb::Status::OK();
};

LevelDBScope::LevelDBScope(int64_t scope_id,
                           std::vector<uint8_t> prefix,
                           size_t write_batch_size,
                           scoped_refptr<LevelDBState> level_db,
                           std::vector<PartitionedLock> locks,
                           RollbackCallback rollback_callback)
    : scope_id_(scope_id),
      prefix_(std::move(prefix)),
      write_batch_size_(write_batch_size),
      level_db_(std::move(level_db)),
      locks_(std::move(locks)),
      rollback_callback_(std::move(rollback_callback)) {
  DCHECK(!locks_.empty());
}

LevelDBScope::~LevelDBScope() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (has_written_to_disk_ && !committed_ && rollback_callback_) [[unlikely]] {
    DCHECK(undo_sequence_number_ < std::numeric_limits<int64_t>::max() ||
           cleanup_sequence_number_ > 0)
        << "A reverting scope that has written to disk must have either an "
           "undo or cleanup task written to it. undo_sequence_number_: "
        << undo_sequence_number_
        << ", cleanup_sequence_number_: " << cleanup_sequence_number_;
    std::move(rollback_callback_).Run(scope_id_, std::move(locks_));
  }
}

leveldb::Status LevelDBScope::Put(const leveldb::Slice& key,
                                  const leveldb::Slice& value) {
  // This has to be used for IsInDeferredDeletionRange, so it might as well
  // surround all the DCHECKs.
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.starts_with(Uint8VectorToSlice(prefix_)));
  DCHECK(!committed_);
  DCHECK(!locks_.empty());
  DCHECK(!IsInDeferredDeletionRange(key))
      << "Cannot put a value in a range that will be deleted later.";
#endif
  buffer_batch_.Put(key, value);
  buffer_batch_empty_ = false;
  if (GetMemoryUsage() > write_batch_size_)
    return WriteChangesAndUndoLogInternal(false);
  return leveldb::Status::OK();
}

leveldb::Status LevelDBScope::Delete(const leveldb::Slice& key) {
  // This has to be used for IsInDeferredDeletionRange, so it might as well
  // surround all the DCHECKs.
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!key.starts_with(Uint8VectorToSlice(prefix_)));
  DCHECK(!committed_);
  DCHECK(!locks_.empty());
  DCHECK(!IsInDeferredDeletionRange(key))
      << "Cannot delete value in a range that will be deleted later.";
#endif
  buffer_batch_.Delete(key);
  buffer_batch_empty_ = false;
  if (GetMemoryUsage() > write_batch_size_)
    return WriteChangesAndUndoLogInternal(false);
  return leveldb::Status::OK();
}

leveldb::Status LevelDBScope::DeleteRange(const leveldb::Slice& begin,
                                          const leveldb::Slice& end,
                                          LevelDBScopeDeletionMode mode) {
#if DCHECK_IS_ON()
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!begin.starts_with(Uint8VectorToSlice(prefix_)));
  DCHECK(!end.starts_with(Uint8VectorToSlice(prefix_)));
  DCHECK(!committed_);
  DCHECK(!locks_.empty());
  switch (mode) {
    case LevelDBScopeDeletionMode::kDeferred:
    case LevelDBScopeDeletionMode::kDeferredWithCompaction:
      deferred_delete_ranges_.emplace_back(begin.ToString(), end.ToString());
      break;
    default:
      break;
  }
#endif
  bool end_exclusive = true;
  switch (mode) {
    case LevelDBScopeDeletionMode::kDeferred:
      SetModeToUndoLog();
      AddCleanupDeleteRangeTask(begin.ToString(), end.ToString());
      return leveldb::Status::OK();
    case LevelDBScopeDeletionMode::kDeferredWithCompaction:
      SetModeToUndoLog();
      AddCleanupDeleteAndCompactRangeTask(begin.ToString(), end.ToString());
      return leveldb::Status::OK();
    case LevelDBScopeDeletionMode::kImmediateWithRangeEndExclusive:
      break;
    case LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive:
      end_exclusive = false;
      break;
  }
  // This method uses its own algorithm to generate the undo log tasks because
  // it can take advantage of the current |value()| already loaded in the
  // iterator. So process any existing changes (and generate any needed undo log
  // tasks) to start with an empty |buffer_batch_|.
  leveldb::Status s = WriteChangesAndUndoLogInternal(false);
  DCHECK(!s.IsNotFound());
  if (!s.ok() && !s.IsNotFound()) [[unlikely]] {
    return s;
  }
  leveldb::ReadOptions options;
  options.verify_checksums = true;
  // Since these are keys that are being deleted, this should not fill the
  // block cache (as the data will be immediately stale).
  options.fill_cache = false;
  const std::unique_ptr<leveldb::Iterator> it =
      base::WrapUnique(level_db_->db()->NewIterator(options));

  const leveldb::Comparator* comparator = level_db_->comparator();
  for (it->Seek(begin);
       (s = it->status(), s.ok()) && it->Valid() &&
       IsKeyBeforeEndOfRange(comparator, it->key(), end, end_exclusive);
       it->Next()) {
    // To avoid setting mode to UndoLog if there are no keys to delete, call the
    // function here inside of the loop.
    SetModeToUndoLog();
    // Undo log.
    AddUndoPutTask(it->key().ToString(), it->value().ToString());
    // Removal.
    buffer_batch_.Delete(it->key());
    buffer_batch_empty_ = false;
    // Make sure our buffer batch isn't getting too big.
    if (GetMemoryUsage() > write_batch_size_) {
      s = WriteBufferBatch(false);
      DCHECK(!s.IsNotFound());
      if (!s.ok() && !s.IsNotFound()) [[unlikely]] {
        return s;
      }
    }
  }
  if (!s.ok() && !s.IsNotFound()) [[unlikely]] {
    return s;
  }
  // This could happen if there were no keys found in the range.
  if (buffer_batch_empty_)
    return leveldb::Status::OK();
  return WriteBufferBatch(false);
}

leveldb::Status LevelDBScope::WriteChangesAndUndoLog() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return WriteChangesAndUndoLogInternal(false);
}

void LevelDBScope::Rollback() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!committed_);
  DCHECK(rollback_callback_);
  if (!has_written_to_disk_) {
    buffer_batch_.Clear();
    buffer_batch_empty_ = true;
    return;
  }
  DCHECK(undo_sequence_number_ < std::numeric_limits<int64_t>::max() ||
         cleanup_sequence_number_ > 0)
      << "A reverting scope that has written to disk must have either an "
         "undo or cleanup task written to it. undo_sequence_number_: "
      << undo_sequence_number_
      << ", cleanup_sequence_number_: " << cleanup_sequence_number_;
  std::move(rollback_callback_).Run(scope_id_, std::move(locks_));
}

std::pair<leveldb::Status, LevelDBScope::Mode> LevelDBScope::Commit(
    bool sync_on_commit) {
  DCHECK(!locks_.empty());
  DCHECK(!committed_);
  DCHECK(rollback_callback_);
  leveldb::Status s;
  switch (mode_) {
    case Mode::kInMemory:
      // Don't bother hitting disk if we don't have anything.
      if (!buffer_batch_empty_)
        s = WriteBufferBatch(sync_on_commit);
      break;
    case Mode::kUndoLogOnDisk:
      AddCommitPoint();
      s = WriteChangesAndUndoLogInternal(sync_on_commit);
      break;
    default:
      NOTREACHED_IN_MIGRATION();
      return {leveldb::Status::NotSupported("Unknown scopes mode."), mode_};
  }
  locks_.clear();
  committed_ = true;
  return {s, mode_};
}

leveldb::Status LevelDBScope::WriteChangesAndUndoLogInternal(bool sync) {
  if (buffer_batch_empty_)
    return leveldb::Status::OK();
  SetModeToUndoLog();

  leveldb::WriteBatch changes = buffer_batch_;
  UndoLogWriter undo_writer(this, level_db_->db());
  changes.Iterate(&undo_writer);
  changes.Clear();

  return WriteBufferBatch(sync);
}

void LevelDBScope::AddUndoPutTask(std::string key, std::string value) {
  DCHECK(undo_task_buffer_.operation_case() ==
         LevelDBScopesUndoTask::OPERATION_NOT_SET);
  auto* const put = undo_task_buffer_.mutable_put();
  put->set_key(std::move(key));
  put->set_value(std::move(value));
  AddBufferedUndoTask();
}

void LevelDBScope::AddUndoDeleteTask(std::string key) {
  DCHECK(undo_task_buffer_.operation_case() ==
         LevelDBScopesUndoTask::OPERATION_NOT_SET);
  auto* const del = undo_task_buffer_.mutable_delete_();
  del->set_key(std::move(key));
  AddBufferedUndoTask();
}

void LevelDBScope::AddUndoDeleteRangeTask(std::string begin, std::string end) {
  DCHECK(undo_task_buffer_.operation_case() ==
         LevelDBScopesUndoTask::OPERATION_NOT_SET);
  auto* const range = undo_task_buffer_.mutable_delete_range();
  range->set_begin(std::move(begin));
  range->set_end(std::move(end));
  AddBufferedUndoTask();
}

void LevelDBScope::AddBufferedUndoTask() {
  undo_task_buffer_.SerializeToString(&value_buffer_);
  buffer_batch_.Put(
      key_encoder_.UndoTaskKey(prefix_, scope_id_, undo_sequence_number_),
      value_buffer_);
  DCHECK_GT(cleanup_sequence_number_, std::numeric_limits<int64_t>::min());
  --undo_sequence_number_;
  buffer_batch_empty_ = false;
  undo_task_buffer_.Clear();
}

void LevelDBScope::AddCleanupDeleteRangeTask(std::string begin,
                                             std::string end) {
  DCHECK(cleanup_task_buffer_.operation_case() ==
         LevelDBScopesCleanupTask::OPERATION_NOT_SET);
  auto* const range = cleanup_task_buffer_.mutable_delete_range();
  range->set_begin(std::move(begin));
  range->set_end(std::move(end));
  AddBufferedCleanupTask();
}

void LevelDBScope::AddCleanupDeleteAndCompactRangeTask(std::string begin,
                                                       std::string end) {
  DCHECK(cleanup_task_buffer_.operation_case() ==
         LevelDBScopesCleanupTask::OPERATION_NOT_SET);
  auto* const range = cleanup_task_buffer_.mutable_delete_range_and_compact();
  range->set_begin(std::move(begin));
  range->set_end(std::move(end));
  AddBufferedCleanupTask();
}

void LevelDBScope::AddBufferedCleanupTask() {
  cleanup_task_buffer_.SerializeToString(&value_buffer_);
  buffer_batch_.Put(
      key_encoder_.CleanupTaskKey(prefix_, scope_id_, cleanup_sequence_number_),
      value_buffer_);
  DCHECK_LT(cleanup_sequence_number_, std::numeric_limits<int64_t>::max());
  ++cleanup_sequence_number_;
  buffer_batch_empty_ = false;
  cleanup_task_buffer_.Clear();
}

void LevelDBScope::SetModeToUndoLog() {
  if (mode_ == Mode::kUndoLogOnDisk)
    return;
  mode_ = Mode::kUndoLogOnDisk;

  LevelDBScopesScopeMetadata metadata;
  for (PartitionedLock& lock : locks_) {
    auto* lock_proto = metadata.add_locks();
    lock_proto->set_partition(lock.lock_id().partition);
    auto* key = lock_proto->mutable_key();
    key->set_key(lock.lock_id().key);
  }
  metadata.SerializeToString(&value_buffer_);
  buffer_batch_.Put(key_encoder_.ScopeMetadataKey(prefix_, scope_id_),
                    value_buffer_);
  buffer_batch_empty_ = false;
}

bool LevelDBScope::CanSkipWritingUndoEntry(const leveldb::Slice& key) {
  return key.starts_with(leveldb::Slice(
      reinterpret_cast<const char*>(prefix_.data()), prefix_.size()));
}

void LevelDBScope::AddCommitPoint() {
  DCHECK(mode_ == Mode::kUndoLogOnDisk);
  // Remove the lock ranges from the metadata, which is the 'commit point' and
  // means that this scope is committed.
  LevelDBScopesScopeMetadata metadata;
  metadata.SerializeToString(&value_buffer_);
  buffer_batch_.Put(key_encoder_.ScopeMetadataKey(prefix_, scope_id_),
                    value_buffer_);
  buffer_batch_empty_ = false;
}

leveldb::Status LevelDBScope::WriteBufferBatch(bool sync) {
  leveldb::WriteOptions write_options;
  write_options.sync = sync;
  approximate_bytes_written_ += buffer_batch_.ApproximateSize();
  leveldb::Status s = level_db_->db()->Write(write_options, &buffer_batch_);
  // We intentionally clear the write batch, even if the write fails, as this
  // class is expected to be treated as invalid after a failure and shouldn't be
  // used.
  buffer_batch_.Clear();
  has_written_to_disk_ = true;
  buffer_batch_empty_ = true;
  return s;
}

#if DCHECK_IS_ON()
bool LevelDBScope::IsInDeferredDeletionRange(const leveldb::Slice& key) {
  const leveldb::Comparator* comparator = level_db_->comparator();
  for (const auto& range : deferred_delete_ranges_) {
    int beginCompare = comparator->Compare(range.first, key);
    if (beginCompare > 0)
      continue;
    if (beginCompare == 0)
      return true;
    int endCompare = comparator->Compare(range.second, key);
    if (endCompare < 0)
      continue;
    return endCompare > 0;
  }
  return false;
}
#endif

}  // namespace content::indexed_db
