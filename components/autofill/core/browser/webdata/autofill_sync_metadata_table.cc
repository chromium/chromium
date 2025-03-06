// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"

#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"

namespace autofill {

namespace {

constexpr std::string_view kAutofillSyncMetadataTable =
    "autofill_sync_metadata";
constexpr std::string_view kModelType = "model_type";
constexpr std::string_view kStorageKey = "storage_key";
constexpr std::string_view kValue = "value";

// The table name uses the legacy name "model type state" as a historic artifact
// to avoid a data migration.
constexpr std::string_view kAutofillDataTypeStateTable =
    "autofill_model_type_state";
// kModelType = "model_type"
// kValue = "value"

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

}  // namespace

AutofillSyncMetadataTable::AutofillSyncMetadataTable() = default;

AutofillSyncMetadataTable::~AutofillSyncMetadataTable() = default;

// static
AutofillSyncMetadataTable* AutofillSyncMetadataTable::FromWebDatabase(
    WebDatabase* db) {
  return static_cast<AutofillSyncMetadataTable*>(db->GetTable(GetKey()));
}

// static
bool AutofillSyncMetadataTable::SupportsMetadataForDataType(
    syncer::DataType data_type) {
  return data_type == syncer::AUTOFILL ||
         data_type == syncer::AUTOFILL_PROFILE ||
         data_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
         data_type == syncer::AUTOFILL_WALLET_DATA ||
         data_type == syncer::AUTOFILL_WALLET_METADATA ||
         data_type == syncer::AUTOFILL_WALLET_OFFER ||
         data_type == syncer::AUTOFILL_WALLET_USAGE ||
         data_type == syncer::CONTACT_INFO;
}

WebDatabaseTable::TypeKey AutofillSyncMetadataTable::GetTypeKey() const {
  return GetKey();
}

bool AutofillSyncMetadataTable::CreateTablesIfNecessary() {
  return InitAutofillSyncMetadataTable() && InitDataTypeStateTable();
}

bool AutofillSyncMetadataTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  if (!db()->is_open()) {
    return false;
  }
  return true;
}

bool AutofillSyncMetadataTable::GetAllSyncMetadata(
    syncer::DataType data_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";
  DCHECK(metadata_batch);
  if (!GetAllSyncEntityMetadata(data_type, metadata_batch)) {
    return false;
  }

  sync_pb::DataTypeState data_type_state;
  if (!GetDataTypeState(data_type, &data_type_state)) {
    return false;
  }

  metadata_batch->SetDataTypeState(data_type_state);
  return true;
}

bool AutofillSyncMetadataTable::DeleteAllSyncMetadata(
    syncer::DataType data_type) {
  return DeleteWhereColumnEq(db(), kAutofillSyncMetadataTable, kModelType,
                             GetKeyValueForDataType(data_type));
}

bool AutofillSyncMetadataTable::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";

  sql::Statement s;
  InsertBuilder(db(), s, kAutofillSyncMetadataTable,
                {kModelType, kStorageKey, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForDataType(data_type));
  s.BindString(1, storage_key);
  s.BindString(2, metadata.SerializeAsString());

  return s.Run();
}

bool AutofillSyncMetadataTable::ClearEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db(), s, kAutofillSyncMetadataTable,
                "model_type=? AND storage_key=?");
  s.BindInt(0, GetKeyValueForDataType(data_type));
  s.BindString(1, storage_key);

  return s.Run();
}

bool AutofillSyncMetadataTable::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";

  // Hardcode the id to force a collision, ensuring that there remains only a
  // single entry.
  sql::Statement s;
  InsertBuilder(db(), s, kAutofillDataTypeStateTable, {kModelType, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForDataType(data_type));
  s.BindString(1, data_type_state.SerializeAsString());

  return s.Run();
}

bool AutofillSyncMetadataTable::ClearDataTypeState(syncer::DataType data_type) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db(), s, kAutofillDataTypeStateTable, "model_type=?");
  s.BindInt(0, GetKeyValueForDataType(data_type));

  return s.Run();
}

int AutofillSyncMetadataTable::GetKeyValueForDataType(
    syncer::DataType data_type) const {
  return syncer::DataTypeToStableIdentifier(data_type);
}

bool AutofillSyncMetadataTable::GetAllSyncEntityMetadata(
    syncer::DataType data_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";
  DCHECK(metadata_batch);

  sql::Statement s;
  SelectBuilder(db(), s, kAutofillSyncMetadataTable, {kStorageKey, kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForDataType(data_type));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize AUTOFILL data type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
  }
  return true;
}

bool AutofillSyncMetadataTable::GetDataTypeState(
    syncer::DataType data_type,
    sync_pb::DataTypeState* state) {
  DCHECK(SupportsMetadataForDataType(data_type))
      << "Data type " << data_type << " not supported for metadata";

  sql::Statement s;
  SelectBuilder(db(), s, kAutofillDataTypeStateTable, {kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForDataType(data_type));

  if (!s.Step()) {
    return true;
  }

  std::string serialized_state = s.ColumnString(0);
  return state->ParseFromString(serialized_state);
}

bool AutofillSyncMetadataTable::InitAutofillSyncMetadataTable() {
  return CreateTableIfNotExists(db(), kAutofillSyncMetadataTable,
                                {{kModelType, "INTEGER NOT NULL"},
                                 {kStorageKey, "VARCHAR NOT NULL"},
                                 {kValue, "BLOB"}},
                                {kModelType, kStorageKey});
}

bool AutofillSyncMetadataTable::InitDataTypeStateTable() {
  return CreateTableIfNotExists(
      db(), kAutofillDataTypeStateTable,
      {{kModelType, "INTEGER NOT NULL PRIMARY KEY"}, {kValue, "BLOB"}});
}

}  // namespace autofill
