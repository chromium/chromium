// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/power_bookmarks/storage/power_bookmark_sync_metadata_database.h"

#include <memory>

#include "base/big_endian.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/statement.h"

namespace power_bookmarks {

namespace {

// Key in sql::MetaTable, the value is a serialization of syner::DataTypeState,
// which tracks the overall sync state of the power bookmark datatype.
const char kPowerBookmarkDataTypeStateKey[] = "power_bookmark_type_state";

}  // namespace

// Description of database table:
//
// sync_metadata
//   storage_key      The guid of an entry in the saves table.
//   value            Serialized sync EntityMetadata, which tracks the sync
//                    state of each power bookmark entity.
PowerBookmarkSyncMetadataDatabase::PowerBookmarkSyncMetadataDatabase(
    sql::Database* db,
    sql::MetaTable* meta_table)
    : db_(db), meta_table_(meta_table) {}

PowerBookmarkSyncMetadataDatabase::~PowerBookmarkSyncMetadataDatabase() =
    default;

bool PowerBookmarkSyncMetadataDatabase::Init() {
  if (!db_->DoesTableExist("sync_metadata")) {
    static constexpr char kInitSql[] =
        // clang-format off
        "CREATE TABLE sync_metadata "
            "(storage_key TEXT PRIMARY KEY NOT NULL, value BLOB)";
    // clang-format on
    DCHECK(db_->IsSQLValid(kInitSql));

    if (!db_->Execute(kInitSql)) {
      return false;
    }
  }
  return true;
}

bool PowerBookmarkSyncMetadataDatabase::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK(!storage_key.empty());

  static constexpr char kUpdateSyncMetadataSql[] =
      // clang-format off
    "INSERT OR REPLACE INTO sync_metadata "
        "(storage_key, value) VALUES(?, ?)";
  // clang-format on
  DCHECK(db_->IsSQLValid(kUpdateSyncMetadataSql));

  sql::Statement s(db_->GetUniqueStatement(kUpdateSyncMetadataSql));
  s.BindString(0, storage_key);
  s.BindString(1, metadata.SerializeAsString());

  return s.Run();
}

bool PowerBookmarkSyncMetadataDatabase::ClearEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key) {
  DCHECK(!storage_key.empty());

  static constexpr char kClearSyncMetadataSql[] =
      // clang-format off
    "DELETE FROM sync_metadata WHERE storage_key=?";
  // clang-format on
  DCHECK(db_->IsSQLValid(kClearSyncMetadataSql));

  sql::Statement s(db_->GetUniqueStatement(kClearSyncMetadataSql));
  s.BindString(0, storage_key);

  return s.Run();
}

bool PowerBookmarkSyncMetadataDatabase::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);

  std::string serialized_state = data_type_state.SerializeAsString();
  return meta_table_->SetValue(kPowerBookmarkDataTypeStateKey,
                               serialized_state);
}

bool PowerBookmarkSyncMetadataDatabase::ClearDataTypeState(
    syncer::DataType data_type) {
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);
  return meta_table_->DeleteKey(kPowerBookmarkDataTypeStateKey);
}

std::unique_ptr<syncer::MetadataBatch>
PowerBookmarkSyncMetadataDatabase::GetAllSyncMetadata() {
  auto metadata_batch = std::make_unique<syncer::MetadataBatch>();
  if (!GetAllSyncEntityMetadata(metadata_batch.get())) {
    return nullptr;
  }

  if (!GetDataTypeState(metadata_batch.get())) {
    return nullptr;
  }

  return metadata_batch;
}

bool PowerBookmarkSyncMetadataDatabase::GetAllSyncEntityMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);

  static constexpr char kGetAllSyncEntityMetadataSql[] =
      // clang-format off
    "SELECT storage_key, value FROM sync_metadata";
  // clang-format on
  DCHECK(db_->IsSQLValid(kGetAllSyncEntityMetadataSql));

  sql::Statement s(db_->GetUniqueStatement(kGetAllSyncEntityMetadataSql));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (!entity_metadata->ParseFromString(serialized_metadata)) {
      DLOG(WARNING) << "Failed to deserialize POWER_BOOKMARK data type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
    metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
  }

  return true;
}

bool PowerBookmarkSyncMetadataDatabase::GetDataTypeState(
    syncer::MetadataBatch* metadata_batch) const {
  sync_pb::DataTypeState state;
  std::string serialized_state;
  meta_table_->GetValue(kPowerBookmarkDataTypeStateKey, &serialized_state);

  if (state.ParseFromString(serialized_state)) {
    metadata_batch->SetDataTypeState(state);
    return true;
  }

  return false;
}

}  // namespace power_bookmarks