// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/sync/history_sync_metadata_database.h"

#include <memory>

#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/time/time.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/meta_table.h"
#include "sql/statement.h"

namespace history {

namespace {

// Key in sql::MetaTable, the value is a serialization of syner::DataTypeState,
// which tracks the overall sync state of the history datatype. Note that the
// table name uses the legacy name "model type state" as a historic artifact to
// avoid a data migration.
const char kHistoryDataTypeStateKey[] = "history_model_type_state";

}  // namespace

// Description of database table:
//
// history_sync_metadata
//   storage_key      The visit_time of an entry in the visits table (in
//                    microseconds since the windows epoch, serialized into a
//                    string in big-endian order), used to look up native data
//                    with sync metadata records.
//   value            Serialized sync EntityMetadata, which tracks the sync
//                    state of each history entity.

HistorySyncMetadataDatabase::HistorySyncMetadataDatabase(
    sql::Database* db,
    sql::MetaTable* meta_table)
    : db_(db), meta_table_(meta_table) {}

HistorySyncMetadataDatabase::~HistorySyncMetadataDatabase() = default;

bool HistorySyncMetadataDatabase::Init() {
  if (!db_->DoesTableExist("history_sync_metadata")) {
    if (!db_->Execute(
            "CREATE TABLE history_sync_metadata "
            "(storage_key INTEGER PRIMARY KEY NOT NULL, value BLOB)")) {
      return false;
    }
  }
  return true;
}

bool HistorySyncMetadataDatabase::GetAllSyncMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);
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

bool HistorySyncMetadataDatabase::ClearAllEntityMetadata() {
  sql::Statement s(
      db_->GetUniqueStatement("DELETE FROM history_sync_metadata"));

  return s.Run();
}

bool HistorySyncMetadataDatabase::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK_EQ(data_type, syncer::HISTORY)
      << "Only the HISTORY data type is supported";
  DCHECK(!storage_key.empty());

  sql::Statement s(
      db_->GetUniqueStatement("INSERT OR REPLACE INTO history_sync_metadata "
                              "(storage_key, value) VALUES(?, ?)"));
  s.BindInt64(0, StorageKeyToMicrosSinceWindowsEpoch(storage_key));
  s.BindString(1, metadata.SerializeAsString());

  return s.Run();
}

bool HistorySyncMetadataDatabase::ClearEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key) {
  DCHECK_EQ(data_type, syncer::HISTORY)
      << "Only the HISTORY data type is supported";
  DCHECK(!storage_key.empty());

  sql::Statement s(db_->GetUniqueStatement(
      "DELETE FROM history_sync_metadata WHERE storage_key=?"));
  s.BindInt64(0, StorageKeyToMicrosSinceWindowsEpoch(storage_key));

  return s.Run();
}

bool HistorySyncMetadataDatabase::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  DCHECK_EQ(data_type, syncer::HISTORY)
      << "Only the HISTORY data type is supported";
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);

  std::string serialized_state = data_type_state.SerializeAsString();
  return meta_table_->SetValue(kHistoryDataTypeStateKey, serialized_state);
}

bool HistorySyncMetadataDatabase::ClearDataTypeState(
    syncer::DataType data_type) {
  DCHECK_EQ(data_type, syncer::HISTORY)
      << "Only the HISTORY data type is supported";
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);
  return meta_table_->DeleteKey(kHistoryDataTypeStateKey);
}

// static
uint64_t HistorySyncMetadataDatabase::StorageKeyToMicrosSinceWindowsEpoch(
    const std::string& storage_key) {
  // TODO(danakj): This method could receive a span<const char, 8u> (or
  // span<const uint8_t, 8u>) instead of checking this size at runtime.
  DCHECK_EQ(storage_key.size(), sizeof(uint64_t));
  return base::U64FromBigEndian(
      base::as_byte_span(storage_key).first<sizeof(uint64_t)>());
}

// static
std::string HistorySyncMetadataDatabase::StorageKeyFromMicrosSinceWindowsEpoch(
    uint64_t micros) {
  std::array<uint8_t, sizeof(uint64_t)> storage_key =
      base::U64ToBigEndian(micros);
  return std::string(storage_key.begin(), storage_key.end());
}

// static
base::Time HistorySyncMetadataDatabase::StorageKeyToVisitTime(
    const std::string& storage_key) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(StorageKeyToMicrosSinceWindowsEpoch(storage_key)));
}

// static
std::string HistorySyncMetadataDatabase::StorageKeyFromVisitTime(
    base::Time visit_time) {
  return StorageKeyFromMicrosSinceWindowsEpoch(
      visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
}

bool HistorySyncMetadataDatabase::GetAllEntityMetadata(
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(metadata_batch);
  sql::Statement s(db_->GetUniqueStatement(
      "SELECT storage_key, value FROM history_sync_metadata"));

  while (s.Step()) {
    std::string storage_key =
        StorageKeyFromMicrosSinceWindowsEpoch(s.ColumnInt64(0));
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (!entity_metadata->ParseFromString(serialized_metadata)) {
      DLOG(WARNING) << "Failed to deserialize HISTORY data type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
    metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
  }
  return true;
}

bool HistorySyncMetadataDatabase::GetDataTypeState(
    sync_pb::DataTypeState* state) {
  DCHECK_GT(meta_table_->GetVersionNumber(), 0);
  std::string serialized_state;
  if (!meta_table_->GetValue(kHistoryDataTypeStateKey, &serialized_state)) {
    *state = sync_pb::DataTypeState();
    return true;
  }

  return state->ParseFromString(serialized_state);
}

}  // namespace history
