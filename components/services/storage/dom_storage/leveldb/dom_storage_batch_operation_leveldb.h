// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

class DomStorageDatabaseLevelDB;

// Wraps LevelDB's `WriteBatch`, adding convenience functions to copy or delete
// all keys that start with a `prefix`.
class DomStorageBatchOperationLevelDB {
 public:
  using Key = DomStorageDatabase::Key;
  using KeyView = DomStorageDatabase::KeyView;
  using Value = DomStorageDatabase::Value;
  using ValueView = DomStorageDatabase::ValueView;

  explicit DomStorageBatchOperationLevelDB(
      base::WeakPtr<DomStorageDatabaseLevelDB> database);
  ~DomStorageBatchOperationLevelDB();

  void Put(KeyView key, ValueView value);
  void Delete(KeyView key);
  [[nodiscard]] DbStatus DeletePrefixed(KeyView prefix);
  [[nodiscard]] DbStatus CopyPrefixed(KeyView prefix, KeyView new_prefix);
  [[nodiscard]] DbStatus Commit();
  size_t ApproximateSizeForMetrics() const;

 private:
  base::WeakPtr<DomStorageDatabaseLevelDB> database_;
  leveldb::WriteBatch write_batch_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_
