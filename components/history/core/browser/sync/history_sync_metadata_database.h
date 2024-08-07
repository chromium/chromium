// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_METADATA_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_METADATA_DATABASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_metadata_store.h"

namespace base {
class Time;
}

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace syncer {
class MetadataBatch;
}

namespace history {

// A sync metadata database maintains two things: the entity metadata table and
// the datatype state, stored in the MetaTable. The entity metadata table
// contains metadata (sync states) for each entity. The datatype state is the
// overall state of the history sync datatype.
class HistorySyncMetadataDatabase : public syncer::SyncMetadataStore {
 public:
  // After construction, Init() must be called before doing anything else to
  // make sure the database is initialized.
  HistorySyncMetadataDatabase(sql::Database* db, sql::MetaTable* meta_table);

  HistorySyncMetadataDatabase(const HistorySyncMetadataDatabase&) = delete;
  HistorySyncMetadataDatabase& operator=(const HistorySyncMetadataDatabase&) =
      delete;

  ~HistorySyncMetadataDatabase() override;

  // Makes sure the tables and indices are properly set up. Must be called
  // before anything else.
  bool Init();

  // Reads all stored metadata for History (both DataTypeState and
  // EntityMetadata) and fills `metadata_batch` with it.
  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch);

  // Deletes all EntityMetadata (but leaves the DataTypeState alone).
  bool ClearAllEntityMetadata();

  // syncer::SyncMetadataStore implementation.
  bool UpdateEntityMetadata(syncer::DataType data_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) override;
  bool ClearEntityMetadata(syncer::DataType data_type,
                           const std::string& storage_key) override;
  bool UpdateDataTypeState(
      syncer::DataType data_type,
      const sync_pb::DataTypeState& data_type_state) override;
  bool ClearDataTypeState(syncer::DataType data_type) override;

  // Conversion between timestamps and storage keys, exposed so that the bridge
  // (and tests) can access this.
  static uint64_t StorageKeyToMicrosSinceWindowsEpoch(
      const std::string& storage_key);
  static std::string StorageKeyFromMicrosSinceWindowsEpoch(uint64_t micros);

  static base::Time StorageKeyToVisitTime(const std::string& storage_key);
  static std::string StorageKeyFromVisitTime(base::Time visit_time);

 private:
  // Reads all sync_pb::EntityMetadata for History and fills `metadata_batch`
  // with it.
  bool GetAllEntityMetadata(syncer::MetadataBatch* metadata_batch);

  // Reads sync_pb::DataTypeState for History and fills `state` with it.
  bool GetDataTypeState(sync_pb::DataTypeState* state);

  const raw_ptr<sql::Database> db_;
  const raw_ptr<sql::MetaTable> meta_table_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_HISTORY_SYNC_METADATA_DATABASE_H_
