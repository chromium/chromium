// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"

#include <cstddef>
#include <cstdint>
#include <utility>

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/pass_key.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb_utils.h"
#include "storage/common/database/leveldb_status_helper.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

DomStorageBatchOperationLevelDB::DomStorageBatchOperationLevelDB(
    base::WeakPtr<DomStorageDatabaseLevelDB> database)
    : database_(std::move(database)) {}

DomStorageBatchOperationLevelDB::~DomStorageBatchOperationLevelDB() = default;

void DomStorageBatchOperationLevelDB::Put(KeyView key, ValueView value) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  write_batch_.Put(MakeSlice(key), MakeSlice(value));
}

void DomStorageBatchOperationLevelDB::Delete(KeyView key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  write_batch_.Delete(MakeSlice(key));
}

DbStatus DomStorageBatchOperationLevelDB::DeletePrefixed(KeyView prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::DB* db = database_
                        ? database_->GetLevelDBDatabase(
                              base::PassKey<DomStorageBatchOperationLevelDB>())
                        : nullptr;
  if (!database_ || !db) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  DbStatus status = ForEachWithPrefix(
      db, prefix, [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        write_batch_.Delete(key);
      });
  return status;
}

DbStatus DomStorageBatchOperationLevelDB::CopyPrefixed(KeyView prefix,
                                                       KeyView new_prefix) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::DB* db = database_
                        ? database_->GetLevelDBDatabase(
                              base::PassKey<DomStorageBatchOperationLevelDB>())
                        : nullptr;
  if (!database_ || !db) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  Key new_key(new_prefix.begin(), new_prefix.end());
  DbStatus status = ForEachWithPrefix(
      db, prefix, [&](const leveldb::Slice& key, const leveldb::Slice& value) {
        DCHECK_GE(key.size(), prefix.size());  // By definition.
        KeyView key_view = base::as_byte_span(key);
        KeyView suffix_view = key_view.subspan(prefix.size());
        new_key.resize(new_prefix.size() + suffix_view.size());
        base::span<uint8_t> dest_span =
            base::span(new_key).subspan(new_prefix.size());
        dest_span.copy_from_nonoverlapping(suffix_view);
        write_batch_.Put(MakeSlice(new_key), value);
      });
  return status;
}

DbStatus DomStorageBatchOperationLevelDB::Commit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  leveldb::DB* db = database_
                        ? database_->GetLevelDBDatabase(
                              base::PassKey<DomStorageBatchOperationLevelDB>())
                        : nullptr;
  if (!database_ || !db) {
    return DbStatus::IOError(kInvalidDatabaseMessage);
  }
  if (database_->ShouldFailAllCommitsForTesting()) {
    return DbStatus::IOError("Simulated I/O Error");
  }
  return FromLevelDBStatus(db->Write(leveldb::WriteOptions(), &write_batch_));
}

size_t DomStorageBatchOperationLevelDB::ApproximateSizeForMetrics() const {
  return write_batch_.ApproximateSize();
}

}  // namespace storage
