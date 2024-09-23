// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/indexed_db/scopes/leveldb_scopes_tasks.h"

#include <cinttypes>
#include <memory>
#include <string>
#include <utility>

#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "components/services/storage/indexed_db/scopes/leveldb_scopes_coding.h"
#include "components/services/storage/indexed_db/scopes/scopes_metadata.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

namespace content::indexed_db {
namespace {
bool IsKeyBeforeEndOfRange(const leveldb::Comparator* comparator,
                           const leveldb::Slice& key,
                           const leveldb::Slice& end) {
  return comparator->Compare(key, end) < 0;
}
}  // namespace

LevelDBScopesTask::LevelDBScopesTask(scoped_refptr<LevelDBState> level_db,
                                     size_t max_write_batch_size_bytes)
    : level_db_(std::move(level_db)),
      max_write_batch_size_bytes_(max_write_batch_size_bytes) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

LevelDBScopesTask::~LevelDBScopesTask() = default;

leveldb::Status LevelDBScopesTask::SubmitWriteBatch(
    const leveldb::WriteOptions& write_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status s = level_db_->db()->Write(write_options, &write_batch_);
  // Clear the |write_batch_| unconditionally. If the operation failed, then the
  // entire task will be abandoned anyways, and retried when the database is
  // reopened.
  write_batch_.Clear();
  return s;
}

leveldb::Status LevelDBScopesTask::MaybeSubmitWriteBatch(
    const leveldb::WriteOptions& write_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::Status s = leveldb::Status::OK();
  if (write_batch_.ApproximateSize() > max_write_batch_size_bytes_)
    s = SubmitWriteBatch(write_options);
  return s;
}

leveldb::Status LevelDBScopesTask::DeleteRange(
    leveldb::Slice range_start,
    leveldb::Slice range_end,
    const leveldb::ReadOptions& read_options,
    const leveldb::WriteOptions& write_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<leveldb::Iterator> iterator =
      base::WrapUnique(level_db_->db()->NewIterator(read_options));
  iterator->Seek(range_start);
  leveldb::Status s;

  for (; iterator->Valid() && IsKeyBeforeEndOfRange(level_db_->comparator(),
                                                    iterator->key(), range_end);
       iterator->Next()) {
    write_batch_.Delete(iterator->key());
    s = MaybeSubmitWriteBatch(write_options);
    if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
      return s;
    }
  }
  if (!iterator->status().ok())
    return iterator->status();
  return SubmitWriteBatch(write_options);
}

CleanupScopeTask::CleanupScopeTask(scoped_refptr<LevelDBState> level_db,
                                   std::vector<uint8_t> metadata_prefix,
                                   int64_t scope_number,
                                   CleanupMode mode,
                                   size_t max_write_batch_size_bytes)
    : LevelDBScopesTask(std::move(level_db), max_write_batch_size_bytes),
      metadata_prefix_(std::move(metadata_prefix)),
      scope_number_(scope_number),
      mode_(mode) {}
CleanupScopeTask::~CleanupScopeTask() = default;

leveldb::Status CleanupScopeTask::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (level_db_->destruction_requested()) [[unlikely]] {
    return leveldb::Status::OK();
  }
  leveldb::ReadOptions read_options;
  // Since the range being iterated will never be used again, don't fill the
  // cache.
  read_options.fill_cache = false;
  read_options.verify_checksums = true;
  leveldb::WriteOptions write_options;
  // The cleanup range will never be used again, so sync is not necessary. If
  // any changes are dropped during a crash, cleanup will resume on the next
  // database open.
  write_options.sync = false;
  ScopesEncoder scopes_encoder;
  leveldb::Status s;

#if DCHECK_IS_ON()
  // Check that the metadata's mode matches the mode of this task.
  std::string metadata_value;
  s = level_db_->db()->Get(
      read_options,
      scopes_encoder.ScopeMetadataKey(metadata_prefix_, scope_number_),
      &metadata_value);
  if (s.IsNotFound()) {
    return leveldb::Status::Corruption(base::StringPrintf(
        "Unable to find scopes metadata for scope %" PRId64 ".",
        scope_number_));
  }
  if (!s.ok()) [[unlikely]] {
    return s;
  }

  LevelDBScopesScopeMetadata metadata;
  if (!metadata.ParseFromString(metadata_value)) [[unlikely]] {
    return leveldb::Status::Corruption("Unable to parse scope metadata.");
  }
  if (metadata.ignore_cleanup_tasks() !=
      (mode_ == CleanupMode::kIgnoreCleanupTasks)) [[unlikely]] {
    return leveldb::Status::Corruption("Invalid cleanup mode on disk.");
  }
#endif  // DCHECK_IS_ON()

  switch (mode_) {
    case CleanupMode::kIgnoreCleanupTasks:
      // Delete all tasks.
      s = DeletePrefixedRange(
          scopes_encoder.TasksKeyPrefix(metadata_prefix_, scope_number_),
          read_options, write_options);
      if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
        return s;
      }
      break;
    case CleanupMode::kExecuteCleanupTasks:
      s = DeletePrefixedRange(
          scopes_encoder.UndoTaskKeyPrefix(metadata_prefix_, scope_number_),
          read_options, write_options);
      if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
        return s;
      }
      s = ExecuteAndDeleteCleanupTasks(read_options, write_options);
      if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
        return s;
      }
      break;
  }

  write_batch_.Delete(
      scopes_encoder.ScopeMetadataKey(metadata_prefix_, scope_number_));
  return SubmitWriteBatch(write_options);
}

leveldb::Status CleanupScopeTask::ExecuteAndDeleteCleanupTasks(
    const leveldb::ReadOptions& read_options,
    const leveldb::WriteOptions& write_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ScopesEncoder scopes_encoder;
  leveldb::Status s;
  // Iterate the cleanup tasks and execute them.
  leveldb::Slice cleanup_tasks_prefix =
      scopes_encoder.CleanupTaskKeyPrefix(metadata_prefix_, scope_number_);
  std::unique_ptr<leveldb::Iterator> iterator =
      base::WrapUnique(level_db_->db()->NewIterator(read_options));
  iterator->Seek(cleanup_tasks_prefix);

  LevelDBScopesCleanupTask cleanup_task;
  for (; iterator->Valid() && iterator->key().starts_with(cleanup_tasks_prefix);
       iterator->Next()) {
    leveldb::Slice value = iterator->value();
    if (!cleanup_task.ParseFromArray(value.data(), value.size()))
      return leveldb::Status::Corruption("Invalid cleanup operation value.");

    switch (cleanup_task.operation_case()) {
      case LevelDBScopesCleanupTask::kDeleteRange: {
        auto range = cleanup_task.delete_range();
        s = DeleteRange(range.begin(), range.end(), read_options,
                        write_options);
        if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
          return s;
        }
        break;
      }
      case LevelDBScopesCleanupTask::kDeleteRangeAndCompact: {
        auto range = cleanup_task.delete_range_and_compact();
        leveldb::Slice begin(range.begin());
        leveldb::Slice end(range.end());
        s = DeleteRange(begin, end, read_options, write_options);
        if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
          return s;
        }
        level_db_->db()->CompactRange(&begin, &end);
        break;
      }
      // The protobuf code generator is to blame for this style mismatch.
      case LevelDBScopesCleanupTask::OPERATION_NOT_SET:
        return leveldb::Status::Corruption("Invalid cleanup operation type.");
    }
    write_batch_.Delete(iterator->key());

    s = MaybeSubmitWriteBatch(write_options);
    if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
      return s;
    }
  }
  return iterator->status();
}

leveldb::Status CleanupScopeTask::DeletePrefixedRange(
    leveldb::Slice prefix,
    const leveldb::ReadOptions& read_options,
    const leveldb::WriteOptions& write_options) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::unique_ptr<leveldb::Iterator> iterator =
      base::WrapUnique(level_db_->db()->NewIterator(read_options));
  iterator->Seek(prefix);
  leveldb::Status s;

  for (; iterator->Valid() && iterator->key().starts_with(prefix);
       iterator->Next()) {
    write_batch_.Delete(iterator->key());
    s = MaybeSubmitWriteBatch(write_options);
    if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
      return s;
    }
  }
  if (!iterator->status().ok()) [[unlikely]] {
    return iterator->status();
  }
  return MaybeSubmitWriteBatch(write_options);
}

RevertScopeTask::RevertScopeTask(scoped_refptr<LevelDBState> level_db,
                                 std::vector<uint8_t> metadata_prefix,
                                 int64_t scope_number,
                                 size_t max_write_batch_size_bytes)
    : LevelDBScopesTask(std::move(level_db), max_write_batch_size_bytes),
      metadata_prefix_(std::move(metadata_prefix)),
      scope_number_(scope_number) {}
RevertScopeTask::~RevertScopeTask() = default;

leveldb::Status RevertScopeTask::Run() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (level_db_->destruction_requested()) [[unlikely]] {
    return leveldb::Status::OK();
  }
  leveldb::ReadOptions read_options;
  // After this job the scope's cleanup log entries will be read again for
  // deletion by a CleanupTask, so fill the cache here.
  read_options.fill_cache = true;
  read_options.verify_checksums = true;
  leveldb::WriteOptions write_options;
  // The revert range will never be used again, so sync is not necessary. If
  // any changes are dropped during a crash, reverting will resume on the next
  // database open, and it is OK to re-apply undo changes.
  write_options.sync = false;
  ScopesEncoder scopes_encoder;
  leveldb::Status s;

  leveldb::Slice undo_log_prefix =
      scopes_encoder.UndoTaskKeyPrefix(metadata_prefix_, scope_number_);
  std::unique_ptr<leveldb::Iterator> iterator =
      base::WrapUnique(level_db_->db()->NewIterator(read_options));
  iterator->Seek(undo_log_prefix);

  LevelDBScopesUndoTask undo_operation;

  // Iterate through the undo log, applying the changes & deleting the undo
  // entries.
  for (; iterator->Valid() && iterator->key().starts_with(undo_log_prefix);
       iterator->Next()) {
    leveldb::Slice value = iterator->value();
    if (!undo_operation.ParseFromArray(value.data(), value.size()))
      return leveldb::Status::Corruption("Invalid undo operation value.");

    switch (undo_operation.operation_case()) {
      case LevelDBScopesUndoTask::kPut:
        write_batch_.Put(undo_operation.put().key(),
                         undo_operation.put().value());
        break;
      case LevelDBScopesUndoTask::kDelete:
        write_batch_.Delete(undo_operation.delete_().key());
        break;
      case LevelDBScopesUndoTask::kDeleteRange: {
        auto range = undo_operation.delete_range();
        s = DeleteRange(range.begin(), range.end(), read_options,
                        write_options);
        if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
          return s;
        }
        break;
      }
      case LevelDBScopesUndoTask::OPERATION_NOT_SET:
        return leveldb::Status::Corruption("Invalid undo operation type.");
    }
    // The undo entry must be deleted in the same write batch that applies the
    // undo change to keep a consistent state on disk.
    write_batch_.Delete(iterator->key());

    s = MaybeSubmitWriteBatch(write_options);
    if (!s.ok() || level_db_->destruction_requested()) [[unlikely]] {
      return s;
    }
  }
  if (!iterator->status().ok()) [[unlikely]] {
    return iterator->status();
  }

  // Finally, overwrite the metadata to signal the revert is over.
  LevelDBScopesScopeMetadata metadata;
  metadata.set_ignore_cleanup_tasks(true);
  write_batch_.Put(
      scopes_encoder.ScopeMetadataKey(metadata_prefix_, scope_number_),
      metadata.SerializeAsString());
  s = SubmitWriteBatch(write_options);
  return s;
}

}  // namespace content::indexed_db
