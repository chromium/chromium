// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_METADATA_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_METADATA_DATABASE_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "components/history/core/browser/url_row.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/sync_metadata_store.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace history {

// A sync metadata database needs to maintain two tables: entity metadata table
// and datatype state table. Entity metadata table contains metadata(sync
// states) for each url. Datatype state table contains the state of typed url
// datatype.
class TypedURLSyncMetadataDatabase : public syncer::SyncMetadataStore {
 public:
  // Must call InitVisitTable() before using to make sure the database is
  // initialized.
  TypedURLSyncMetadataDatabase();
  ~TypedURLSyncMetadataDatabase() override;

  // Read all the stored metadata for typed URL and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::MetadataBatch* metadata_batch);

  // syncer::SyncMetadataStore implementation.
  bool UpdateSyncMetadata(syncer::ModelType model_type,
                          const std::string& storage_key,
                          const sync_pb::EntityMetadata& metadata) override;
  bool ClearSyncMetadata(syncer::ModelType model_type,
                         const std::string& storage_key) override;
  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override;
  bool ClearModelTypeState(syncer::ModelType model_type) override;

  static URLID StorageKeyToURLID(const std::string& storage_key);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Returns MetaTable, so this sync can store ModelTypeState in MetaTable.
  // Check if GetMetaTable().GetVersionNumber() is greater than 0 to make sure
  // MetaTable is initialed.
  virtual sql::MetaTable& GetMetaTable() = 0;

  // Called by the derived classes on initialization to make sure the tables
  // and indices are properly set up. Must be called before anything else.
  bool InitSyncTable();

  // Cleans up orphaned metadata for typed URLs, i.e. deletes all metadata
  // entries for rowids not present in |sorted_valid_rowids| (which must be
  // sorted in ascending order). Returns true if the clean up finishes without
  // any DB error.
  bool CleanTypedURLOrphanedMetadataForMigrationToVersion40(
      const std::vector<URLID>& sorted_valid_rowids);

 private:
  // Read all sync_pb::EntityMetadata for typed URL and fill
  // |metadata_records| with it.
  bool GetAllSyncEntityMetadata(syncer::MetadataBatch* metadata_batch);

  // Read sync_pb::ModelTypeState for typed URL and fill |state| with it.
  bool GetModelTypeState(sync_pb::ModelTypeState* state);

  DISALLOW_COPY_AND_ASSIGN(TypedURLSyncMetadataDatabase);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_SYNC_TYPED_URL_SYNC_METADATA_DATABASE_H_
