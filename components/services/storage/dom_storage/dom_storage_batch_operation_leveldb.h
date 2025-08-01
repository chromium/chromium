// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "storage/common/database/db_status.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"

namespace storage {

class DomStorageDatabaseLevelDB;

// A DomStorageBatchOperation implementation that uses LevelDB's WriteBatch.
class DomStorageBatchOperationLevelDB : public DomStorageBatchOperation {
 public:
  explicit DomStorageBatchOperationLevelDB(
      base::WeakPtr<DomStorageDatabaseLevelDB> database);
  ~DomStorageBatchOperationLevelDB() override;

  // DomStorageBatchOperation implementation.
  void Put(KeyView key, ValueView value) override;
  void Delete(KeyView key) override;
  DbStatus DeletePrefixed(KeyView prefix) override;
  DbStatus CopyPrefixed(KeyView prefix, KeyView new_prefix) override;
  DbStatus Commit() override;
  size_t ApproximateSizeForMetrics() const override;

 private:
  base::WeakPtr<DomStorageDatabaseLevelDB> database_;
  leveldb::WriteBatch write_batch_;
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_DOM_STORAGE_BATCH_OPERATION_LEVELDB_H_
