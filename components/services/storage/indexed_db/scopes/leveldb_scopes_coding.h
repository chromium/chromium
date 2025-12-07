// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_CODING_H_
#define COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_CODING_H_

#include <stdint.h>
#include <limits>
#include <string>
#include <tuple>

#include "base/containers/span.h"
#include "third_party/leveldatabase/src/include/leveldb/slice.h"

namespace content::indexed_db {
namespace leveldb_scopes {

inline constexpr uint8_t kGlobalMetadataByte = 0x00;
inline constexpr uint8_t kScopesMetadataByte = 0x01;
inline constexpr uint8_t kLogByte = 0x02;

// One of these bytes follows the |kLogByte| to specify whether the log is for
// undo tasks or a cleanup tasks.
inline constexpr uint8_t kUndoTasksByte = 0x00;
inline constexpr uint8_t kCleanupTasksByte = 0x01;

inline constexpr int64_t kMinSupportedVersion = 1;
inline constexpr int64_t kCurrentVersion = 1;

inline constexpr int64_t kFirstScopeNumber = 0;
inline constexpr int64_t kFirstSequenceNumberToWrite =
    std::numeric_limits<int64_t>::max();

std::tuple<bool /*success*/, int64_t /*scope_id*/> ParseScopeMetadataId(
    leveldb::Slice key,
    base::span<const uint8_t> scopes_prefix);

std::string KeyToDebugString(base::span<const uint8_t> key_without_prefix);

}  // namespace leveldb_scopes

// This class helps the re-use of a common std::string buffer. All calls modify
// the internal std::string buffer and return a slice to it.
// Important: Every call to this class will invalidate any 'old' slices that
// were returned by previous calls.
class ScopesEncoder {
 public:
  ScopesEncoder() = default;
  ~ScopesEncoder() = default;

  // The value on disk is expected to be a LevelDBScopesMetadata.
  leveldb::Slice GlobalMetadataKey(base::span<const uint8_t> scopes_prefix);

  // The value on disk  is expected to be a LevelDBScopesScopeMetadata.
  leveldb::Slice ScopeMetadataKey(base::span<const uint8_t> scopes_prefix,
                                  int64_t scope_number);

  // Returns a key prefix that only scope metadata keys use. This is intended to
  // be used to initialize a LevelDB iterator, where advancing the iterator
  // enumerates all metadata entries. Each metadata entry value is expected to
  // be a LevelDBScopesScopeMetadata.
  leveldb::Slice ScopeMetadataPrefix(base::span<const uint8_t> scopes_prefix);

  // Returns a key prefix for all scope tasks for all scopes. This is intended
  // to be used for unit tests which test for the presence of any scope task
  // data after all cleanup has finished.
  leveldb::Slice TasksKeyPrefix(base::span<const uint8_t> scopes_prefix);

  // Returns a key prefix that only scope tasks keys for the given
  // |scope_number| use. This is intended to be used to delete all tasks, or
  // create an iterator for all tasks. There are two task types under this
  // prefix, undo tasks and cleanup tasks.
  leveldb::Slice TasksKeyPrefix(base::span<const uint8_t> scopes_prefix,
                                int64_t scope_number);

  // Returns a key prefix is only scoped to 'undo' tasks keys for the given
  // |scope_number| use. This is intended to be used to initialize a LevelDB
  // iterator, where advancing the iterator enumerates all of the tasks for the
  // given |scope_number|. The task value is expected to be a
  // LevelDBScopesUndoTask.
  leveldb::Slice UndoTaskKeyPrefix(base::span<const uint8_t> scopes_prefix,
                                   int64_t scope_number);

  // Returns a key prefix is only scoped to 'cleanup' tasks keys for the given
  // |scope_number| use. This is intended to be used to initialize a LevelDB
  // iterator, where advancing the iterator enumerates all of the tasks for the
  // given |scope_number|.  The task value is expected to be a
  // LevelDBScopesCleanupTask.
  leveldb::Slice CleanupTaskKeyPrefix(base::span<const uint8_t> scopes_prefix,
                                      int64_t scope_number);

  // The value on disk is expected to be a LevelDBScopesUndoTask.
  leveldb::Slice UndoTaskKey(base::span<const uint8_t> scopes_prefix,
                             int64_t scope_number,
                             int64_t undo_sequence_number);

  // The value on disk is expected to be a LevelDBScopesCleanupTask.
  leveldb::Slice CleanupTaskKey(base::span<const uint8_t> scopes_prefix,
                                int64_t scope_number,
                                int64_t cleanup_sequence_number);

 private:
  std::string key_buffer_;
};

}  // namespace content::indexed_db

#endif  // COMPONENTS_SERVICES_STORAGE_INDEXED_DB_SCOPES_LEVELDB_SCOPES_CODING_H_
