// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"

#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"

namespace autofill {

namespace {

constexpr std::string_view kAutofillSyncMetadataTable =
    "autofill_sync_metadata";
constexpr std::string_view kModelType = "model_type";
constexpr std::string_view kStorageKey = "storage_key";
constexpr std::string_view kValue = "value";

constexpr std::string_view kAutofillModelTypeStateTable =
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
bool AutofillSyncMetadataTable::SupportsMetadataForModelType(
    syncer::ModelType model_type) {
  return model_type == syncer::AUTOFILL ||
         model_type == syncer::AUTOFILL_PROFILE ||
         model_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
         model_type == syncer::AUTOFILL_WALLET_DATA ||
         model_type == syncer::AUTOFILL_WALLET_METADATA ||
         model_type == syncer::AUTOFILL_WALLET_OFFER ||
         model_type == syncer::AUTOFILL_WALLET_USAGE ||
         model_type == syncer::CONTACT_INFO;
}

WebDatabaseTable::TypeKey AutofillSyncMetadataTable::GetTypeKey() const {
  return GetKey();
}

bool AutofillSyncMetadataTable::CreateTablesIfNecessary() {
  return InitAutofillSyncMetadataTable() && InitModelTypeStateTable();
}

bool AutofillSyncMetadataTable::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  if (!db_->is_open()) {
    return false;
  }
  return true;
}

bool AutofillSyncMetadataTable::GetAllSyncMetadata(
    syncer::ModelType model_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);
  if (!GetAllSyncEntityMetadata(model_type, metadata_batch)) {
    return false;
  }

  sync_pb::ModelTypeState model_type_state;
  if (!GetModelTypeState(model_type, &model_type_state)) {
    return false;
  }

  metadata_batch->SetModelTypeState(model_type_state);
  return true;
}

bool AutofillSyncMetadataTable::DeleteAllSyncMetadata(
    syncer::ModelType model_type) {
  return DeleteWhereColumnEq(db_, kAutofillSyncMetadataTable, kModelType,
                             GetKeyValueForModelType(model_type));
}

bool AutofillSyncMetadataTable::UpdateEntityMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  InsertBuilder(db_, s, kAutofillSyncMetadataTable,
                {kModelType, kStorageKey, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);
  s.BindString(2, metadata.SerializeAsString());

  return s.Run();
}

bool AutofillSyncMetadataTable::ClearEntityMetadata(
    syncer::ModelType model_type,
    const std::string& storage_key) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db_, s, kAutofillSyncMetadataTable,
                "model_type=? AND storage_key=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, storage_key);

  return s.Run();
}

bool AutofillSyncMetadataTable::UpdateModelTypeState(
    syncer::ModelType model_type,
    const sync_pb::ModelTypeState& model_type_state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  // Hardcode the id to force a collision, ensuring that there remains only a
  // single entry.
  sql::Statement s;
  InsertBuilder(db_, s, kAutofillModelTypeStateTable, {kModelType, kValue},
                /*or_replace=*/true);
  s.BindInt(0, GetKeyValueForModelType(model_type));
  s.BindString(1, model_type_state.SerializeAsString());

  return s.Run();
}

bool AutofillSyncMetadataTable::ClearModelTypeState(
    syncer::ModelType model_type) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  DeleteBuilder(db_, s, kAutofillModelTypeStateTable, "model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  return s.Run();
}

int AutofillSyncMetadataTable::GetKeyValueForModelType(
    syncer::ModelType model_type) const {
  return syncer::ModelTypeToStableIdentifier(model_type);
}

bool AutofillSyncMetadataTable::GetAllSyncEntityMetadata(
    syncer::ModelType model_type,
    syncer::MetadataBatch* metadata_batch) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";
  DCHECK(metadata_batch);

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillSyncMetadataTable, {kStorageKey, kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  while (s.Step()) {
    std::string storage_key = s.ColumnString(0);
    std::string serialized_metadata = s.ColumnString(1);
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (entity_metadata->ParseFromString(serialized_metadata)) {
      metadata_batch->AddMetadata(storage_key, std::move(entity_metadata));
    } else {
      DLOG(WARNING) << "Failed to deserialize AUTOFILL model type "
                       "sync_pb::EntityMetadata.";
      return false;
    }
  }
  return true;
}

bool AutofillSyncMetadataTable::GetModelTypeState(
    syncer::ModelType model_type,
    sync_pb::ModelTypeState* state) {
  DCHECK(SupportsMetadataForModelType(model_type))
      << "Model type " << model_type << " not supported for metadata";

  sql::Statement s;
  SelectBuilder(db_, s, kAutofillModelTypeStateTable, {kValue},
                "WHERE model_type=?");
  s.BindInt(0, GetKeyValueForModelType(model_type));

  if (!s.Step()) {
    return true;
  }

  std::string serialized_state = s.ColumnString(0);
  return state->ParseFromString(serialized_state);
}

bool AutofillSyncMetadataTable::InitAutofillSyncMetadataTable() {
  return CreateTableIfNotExists(db_, kAutofillSyncMetadataTable,
                                {{kModelType, "INTEGER NOT NULL"},
                                 {kStorageKey, "VARCHAR NOT NULL"},
                                 {kValue, "BLOB"}},
                                {kModelType, kStorageKey});
}

bool AutofillSyncMetadataTable::InitModelTypeStateTable() {
  return CreateTableIfNotExists(
      db_, kAutofillModelTypeStateTable,
      {{kModelType, "INTEGER NOT NULL PRIMARY KEY"}, {kValue, "BLOB"}});
}

}  // namespace autofill
