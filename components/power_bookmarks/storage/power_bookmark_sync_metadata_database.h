// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_METADATA_DATABASE_H_
#define COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_METADATA_DATABASE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_metadata_store.h"
#include "sql/meta_table.h"

namespace sql {
class Database;
class MetaTable;
}  // namespace sql

namespace syncer {
class MetadataBatch;
}  // namespace syncer

namespace power_bookmarks {

class PowerBookmarkSyncMetadataDatabaseTest;

// A sync metadata database maintains two things: the entity metadata table and
// the datatype state, stored in the MetaTable. The entity metadata table
// contains metadata (sync states) for each entity. The datatype state is the
// overall state of the power bookmarks datatype.
class PowerBookmarkSyncMetadataDatabase : public syncer::SyncMetadataStore {
 public:
  // After construction, Init() must be called before doing anything else to
  // make sure the database is initialized.
  PowerBookmarkSyncMetadataDatabase(sql::Database* db,
                                    sql::MetaTable* meta_table);

  PowerBookmarkSyncMetadataDatabase(const PowerBookmarkSyncMetadataDatabase&) =
      delete;
  PowerBookmarkSyncMetadataDatabase& operator=(
      const PowerBookmarkSyncMetadataDatabase&) = delete;

  ~PowerBookmarkSyncMetadataDatabase() override;

  // Makes sure the tables and indices are properly set up. Must be called
  // before anything else.
  bool Init();

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

  // Creates a sync::MetadataBatch and fills it with entity metadata and
  // DataTypeState information.
  std::unique_ptr<syncer::MetadataBatch> GetAllSyncMetadata();

 private:
  friend class PowerBookmarkSyncMetadataDatabaseTest;

  // Reads all sync_pb::EntityMetadata and fills `metadata_batch` with it.
  // Returns whether the call succeeded.
  bool GetAllSyncEntityMetadata(syncer::MetadataBatch* metadata_batch);

  // Reads the stored DataTypeState and fills `metadata_batch` with it.
  // Returns whether the call succeeded.
  bool GetDataTypeState(syncer::MetadataBatch* metadata_batch) const;

  const raw_ptr<sql::Database> db_;
  const raw_ptr<sql::MetaTable> meta_table_;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_STORAGE_POWER_BOOKMARK_SYNC_METADATA_DATABASE_H_