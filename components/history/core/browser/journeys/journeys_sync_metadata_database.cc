// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_sync_metadata_database.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace history::journeys {

namespace {

// Serialization of syncer::DataTypeState used as key in the meta_table.
// This table tracks the overall sync state of synced datatypes (like JOURNEY).
// For syncer::JOURNEY:
//  key     Value of kJourneyDataTypeStateKey.
//  value   Serialized `DataTypeState`, which stores sync progress
//          markers/tokens, cache guid and encryption state.
//          components/sync/protocol/data_type_state.proto
constexpr char kJourneyDataTypeStateKey[] = "journey_data_type_state";

// Table name for the journey sync metadata table in SQLite.
// This table tracks the entity metadata for each synced journey entity:
//  storage_key   The id of the Journey, as set by the server, which is a
//                unique identifier by design.
//  value         Serialized sync `EntityMetadata`, which tracks the sync
//                state of each journey entity.
//                components/sync/protocol/entity_metadata.proto
// This table is not to be confused with the meta_table, which is a global,
// sync data-type level table, used to keep track of the DataTypeState.
constexpr char kSyncMetadataTableName[] = "journey_sync_metadata";

}  // namespace

JourneysSyncMetadataDatabase::JourneysSyncMetadataDatabase(
    sql::Database* db,
    sql::MetaTable* meta_table)
    : db_(db), meta_table_(meta_table) {}

JourneysSyncMetadataDatabase::~JourneysSyncMetadataDatabase() = default;

bool JourneysSyncMetadataDatabase::Init() {
  if (!db_->DoesTableExist(kSyncMetadataTableName)) {
    if (!db_->Execute(
            base::StrCat({"CREATE TABLE ", kSyncMetadataTableName,
                          " (storage_key VARCHAR PRIMARY KEY NOT NULL, "
                          "value BLOB NOT NULL)"}))) {
      return false;
    }
  }
  return true;
}

// Called by JourneysSyncBridge.
bool JourneysSyncMetadataDatabase::GetAllSyncMetadata(
    syncer::MetadataBatch* metadata_batch) {
  CHECK(metadata_batch);
  if (!GetAllEntityMetadata(metadata_batch)) {
    return false;
  }

  sync_pb::DataTypeState data_type_state;
  if (!GetDataTypeState(&data_type_state)) {
    return false;
  }

  metadata_batch->SetDataTypeState(data_type_state);
  return true;
}

bool JourneysSyncMetadataDatabase::ClearAllEntityMetadata() {
  sql::Statement s(db_->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kSyncMetadataTableName})));
  return s.Run();
}

bool JourneysSyncMetadataDatabase::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK_EQ(data_type, syncer::JOURNEY)
      << "Only the JOURNEY data type is supported";
  DCHECK(!storage_key.empty());

  sql::Statement s(db_->GetUniqueStatement(
      base::StrCat({"INSERT OR REPLACE INTO ", kSyncMetadataTableName,
                    " (storage_key, value) VALUES(?, ?)"})));
  s.BindString(0, storage_key);
  s.BindBlob(1, metadata.SerializeAsString());

  return s.Run();
}

bool JourneysSyncMetadataDatabase::ClearEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key) {
  DCHECK_EQ(data_type, syncer::JOURNEY)
      << "Only the JOURNEY data type is supported";
  DCHECK(!storage_key.empty());

  sql::Statement s(db_->GetUniqueStatement(base::StrCat(
      {"DELETE FROM ", kSyncMetadataTableName, " WHERE storage_key=?"})));
  s.BindString(0, storage_key);

  return s.Run();
}

bool JourneysSyncMetadataDatabase::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  DCHECK_EQ(data_type, syncer::JOURNEY)
      << "Only the JOURNEY data type is supported";
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);

  std::string serialized_state = data_type_state.SerializeAsString();
  return meta_table_->SetValue(kJourneyDataTypeStateKey, serialized_state);
}

bool JourneysSyncMetadataDatabase::ClearDataTypeState(
    syncer::DataType data_type) {
  DCHECK_EQ(data_type, syncer::JOURNEY)
      << "Only the JOURNEY data type is supported";
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);
  return meta_table_->DeleteKey(kJourneyDataTypeStateKey);
}

bool JourneysSyncMetadataDatabase::GetAllEntityMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);
  sql::Statement s(db_->GetUniqueStatement(base::StrCat(
      {"SELECT storage_key, value FROM ", kSyncMetadataTableName})));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string_view serialized_metadata = s.ColumnStringView(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (!entity_metadata->ParseFromString(serialized_metadata)) {
      DLOG(WARNING) << "Failed to deserialize JOURNEY data type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
    metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
  }
  return true;
}

bool JourneysSyncMetadataDatabase::GetDataTypeState(
    sync_pb::DataTypeState* state) {
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);
  std::string serialized_state;
  if (!meta_table_->GetValue(kJourneyDataTypeStateKey, &serialized_state)) {
    *state = sync_pb::DataTypeState();
    return true;
  }

  return state->ParseFromString(serialized_state);
}

}  // namespace history::journeys
