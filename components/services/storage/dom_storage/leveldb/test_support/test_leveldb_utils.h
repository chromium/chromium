// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_TEST_SUPPORT_TEST_LEVELDB_UTILS_H_
#define COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_TEST_SUPPORT_TEST_LEVELDB_UTILS_H_

#include <vector>

#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_batch_operation_leveldb.h"
#include "components/services/storage/dom_storage/leveldb/dom_storage_database_leveldb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace storage {

// Populate the LevelDB with test values.  Asserts success.
template <typename TDatabase>
void WriteEntries(TDatabase& database,
                  std::vector<DomStorageDatabase::KeyValuePair> entries) {
  std::unique_ptr<DomStorageBatchOperationLevelDB> batch =
      database.GetLevelDB().CreateBatchOperation();

  for (const DomStorageDatabase::KeyValuePair& entry : entries) {
    batch->Put(entry.key, entry.value);
  }

  DbStatus status = batch->Commit();
  ASSERT_TRUE(status.ok()) << status.ToString();
}

// Returns a copy of `source`.
std::vector<uint8_t> ToBytes(base::span<const uint8_t> source);

}  // namespace storage

#endif  // COMPONENTS_SERVICES_STORAGE_DOM_STORAGE_LEVELDB_TEST_SUPPORT_TEST_LEVELDB_UTILS_H_
