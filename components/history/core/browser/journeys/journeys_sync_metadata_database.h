// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_METADATA_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_METADATA_DATABASE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_metadata_store.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace syncer {
class MetadataBatch;
}

namespace history::journeys {

// Maintains metadata and sync state for each journey.
class JourneysSyncMetadataDatabase : public syncer::SyncMetadataStore {
 public:
  JourneysSyncMetadataDatabase(sql::Database* db, sql::MetaTable* meta_table);

  JourneysSyncMetadataDatabase(const JourneysSyncMetadataDatabase&) = delete;
  JourneysSyncMetadataDatabase& operator=(const JourneysSyncMetadataDatabase&) =
      delete;

  ~JourneysSyncMetadataDatabase() override;

  // Makes sure the tables and indices are properly set up. Must be called
  // before anything else.
  bool Init();

  // Reads all stored metadata for Journeys (both DataTypeState and
  // EntityMetadata) and fills `metadata_batch` with it.
  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch);

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

 private:
  bool GetAllEntityMetadata(syncer::MetadataBatch* metadata_batch);
  bool GetDataTypeState(sync_pb::DataTypeState* state);

  const raw_ptr<sql::Database> db_;
  const raw_ptr<sql::MetaTable> meta_table_;
};

}  // namespace history::journeys

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_JOURNEYS_JOURNEYS_SYNC_METADATA_DATABASE_H_
