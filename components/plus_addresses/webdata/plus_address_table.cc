// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_table.h"

#include <optional>
#include <string_view>
#include <vector>

#include "base/check_op.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/base/data_type.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/data_type_state.pb.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace plus_addresses {

namespace {

constexpr char kPlusAddressTable[] = "plus_addresses";
constexpr char kProfileId[] = "profile_id";
constexpr char kFacet[] = "facet";
constexpr char kPlusAddress[] = "plus_address";

// The table name uses the legacy name "model type state" as a historic artifact
// to avoid a data migration.
constexpr char kSyncDataTypeState[] = "plus_address_sync_model_type_state";
constexpr char kModelType[] = "model_type";
constexpr char kValue[] = "value";

constexpr char kSyncEntityMetadata[] = "plus_address_sync_entity_metadata";
// kModelType
constexpr char kStorageKey[] = "storage_key";
// kValue

// Expects that `s` is pointing to a query result containing `kProfileId`,
// `kFacet` and `kPlusAddress`, in that order. Attempts to construct a
// `PlusProfile` from that data, returning nullopt if validation fails.
std::optional<PlusProfile> PlusProfileFromStatement(sql::Statement& s) {
  affiliations::FacetURI facet =
      affiliations::FacetURI::FromPotentiallyInvalidSpec(s.ColumnString(1));
  if (!facet.is_valid()) {
    // Unless modified through external means, the facet is valid.
    return std::nullopt;
  }
  return PlusProfile(/*profile_id=*/s.ColumnString(0), std::move(facet),
                     PlusAddress(s.ColumnString(2)),
                     /*is_confirmed=*/true);
}

// Populates the `metadata_batch`'s data type state with the state stored for
// `data_type`, or the default, if no state is stored.
// Returns false if the data type state is unparsable.
bool GetDataTypeState(sql::Database& db,
                      syncer::DataType data_type,
                      syncer::MetadataBatch& metadata_batch) {
  sql::Statement data_type_state_query(db.GetUniqueStatement(base::StringPrintf(
      "SELECT %s FROM %s WHERE %s=?", kValue, kSyncDataTypeState, kModelType)));
  data_type_state_query.BindInt(0,
                                syncer::DataTypeToStableIdentifier(data_type));
  sync_pb::DataTypeState data_type_state;
  // When the user just started syncing `data_type`, no data type state is
  // persisted yet and `Step()` will fail. Don't treat this as an error, but
  // fallback to the default state instead.
  if (data_type_state_query.Step() &&
      !data_type_state.ParseFromString(data_type_state_query.ColumnString(0))) {
    return false;
  }
  metadata_batch.SetDataTypeState(data_type_state);
  return true;
}

// Adds all entity metadata stored for `data_type` to `metadata_batch`.
// Returns false and aborts if unparsable data is encountered.
bool AddEntityMetadata(sql::Database& db,
                       syncer::DataType data_type,
                       syncer::MetadataBatch& metadata_batch) {
  sql::Statement entity_query(db.GetUniqueStatement(
      base::StringPrintf("SELECT %s, %s FROM %s WHERE %s=?", kStorageKey,
                         kValue, kSyncEntityMetadata, kModelType)));
  entity_query.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  while (entity_query.Step()) {
    auto entity_metadata = std::make_unique<sync_pb::EntityMetadata>();
    if (!entity_metadata->ParseFromString(entity_query.ColumnString(1))) {
      return false;
    }
    metadata_batch.AddMetadata(entity_query.ColumnString(0),
                               std::move(entity_metadata));
  }
  return true;
}

// The `WebDatabase` manages multiple `WebDatabaseTable` in a `TypeKey` -> table
// map. Any unique constant, such as the address of a static suffices as a key.
WebDatabaseTable::TypeKey GetKey() {
  static int table_key = 0;
  return &table_key;
}

}  // namespace

PlusAddressTable::PlusAddressTable() = default;
PlusAddressTable::~PlusAddressTable() = default;

// static
PlusAddressTable* PlusAddressTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<PlusAddressTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey PlusAddressTable::GetTypeKey() const {
  return GetKey();
}

bool PlusAddressTable::CreateTablesIfNecessary() {
  return CreatePlusAddressesTable() && CreateSyncDataTypeStateTable() &&
         CreateSyncEntityMetadataTable();
}

bool PlusAddressTable::MigrateToVersion(int version,
                                        bool* update_compatible_version) {
  switch (version) {
    case 126:
      *update_compatible_version = false;
      return MigrateToVersion126_InitialSchema();
    case 127:
      *update_compatible_version = true;
      return MigrateToVersion127_SyncSupport();
    case 128:
      *update_compatible_version = true;
      return MigrateToVersion128_ProfileIdString();
  }
  return true;
}

std::vector<PlusProfile> PlusAddressTable::GetPlusProfiles() const {
  sql::Statement query(db()->GetUniqueStatement(
      base::StringPrintf("SELECT %s, %s, %s FROM %s", kProfileId, kFacet,
                         kPlusAddress, kPlusAddressTable)));
  std::vector<PlusProfile> result;
  while (query.Step()) {
    if (std::optional<PlusProfile> profile = PlusProfileFromStatement(query)) {
      result.push_back(std::move(*profile));
    }
  }
  return result;
}

std::optional<PlusProfile> PlusAddressTable::GetPlusProfileForId(
    std::string_view profile_id) const {
  sql::Statement query(db()->GetUniqueStatement(
      base::StringPrintf("SELECT %s, %s, %s FROM %s WHERE %s=?", kProfileId,
                         kFacet, kPlusAddress, kPlusAddressTable, kProfileId)));
  query.BindString(0, profile_id);
  if (!query.Step()) {
    return std::nullopt;
  }
  return PlusProfileFromStatement(query);
}

bool PlusAddressTable::AddOrUpdatePlusProfile(const PlusProfile& profile) {
  CHECK(profile.is_confirmed);
  sql::Statement query(db()->GetUniqueStatement(base::StringPrintf(
      "INSERT OR REPLACE INTO %s (%s, %s, %s) VALUES (?, ?, ?)",
      kPlusAddressTable, kProfileId, kFacet, kPlusAddress)));
  query.BindString(0, profile.profile_id.value());
  query.BindString(1, profile.facet.canonical_spec());
  query.BindString(2, *profile.plus_address);
  return query.Run();
}

bool PlusAddressTable::RemovePlusProfile(std::string_view profile_id) {
  sql::Statement query(db()->GetUniqueStatement(base::StringPrintf(
      "DELETE FROM %s WHERE %s=?", kPlusAddressTable, kProfileId)));
  query.BindString(0, profile_id);
  return query.Run();
}

bool PlusAddressTable::ClearPlusProfiles() {
  return db()->Execute(base::StrCat({"DELETE FROM ", kPlusAddressTable}));
}

bool PlusAddressTable::UpdateEntityMetadata(
    syncer::DataType data_type,
    const std::string& storage_key,
    const sync_pb::EntityMetadata& metadata) {
  CHECK_EQ(data_type, syncer::PLUS_ADDRESS);
  sql::Statement query(db()->GetUniqueStatement(base::StringPrintf(
      "INSERT OR REPLACE INTO %s (%s, %s, %s) VALUES (?, ?, ?)",
      kSyncEntityMetadata, kModelType, kStorageKey, kValue)));
  query.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  query.BindString(1, storage_key);
  query.BindBlob(2, metadata.SerializeAsString());
  return query.Run();
}

bool PlusAddressTable::ClearEntityMetadata(syncer::DataType data_type,
                                           const std::string& storage_key) {
  CHECK_EQ(data_type, syncer::PLUS_ADDRESS);
  sql::Statement query(db()->GetUniqueStatement(
      base::StringPrintf("DELETE FROM %s WHERE %s=? AND %s=?",
                         kSyncEntityMetadata, kModelType, kStorageKey)));
  query.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  query.BindString(1, storage_key);
  return query.Run();
}

bool PlusAddressTable::UpdateDataTypeState(
    syncer::DataType data_type,
    const sync_pb::DataTypeState& data_type_state) {
  CHECK_EQ(data_type, syncer::PLUS_ADDRESS);
  sql::Statement query(db()->GetUniqueStatement(
      base::StringPrintf("INSERT OR REPLACE INTO %s (%s, %s) VALUES (?, ?)",
                         kSyncDataTypeState, kModelType, kValue)));
  query.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  query.BindBlob(1, data_type_state.SerializeAsString());
  return query.Run();
}

bool PlusAddressTable::ClearDataTypeState(syncer::DataType data_type) {
  CHECK_EQ(data_type, syncer::PLUS_ADDRESS);
  sql::Statement query(db()->GetUniqueStatement(base::StringPrintf(
      "DELETE FROM %s WHERE %s=?", kSyncDataTypeState, kModelType)));
  query.BindInt(0, syncer::DataTypeToStableIdentifier(data_type));
  return query.Run();
}

bool PlusAddressTable::GetAllSyncMetadata(
    syncer::DataType data_type,
    syncer::MetadataBatch& metadata_batch) {
  CHECK_EQ(data_type, syncer::PLUS_ADDRESS);
  return GetDataTypeState(*db(), data_type, metadata_batch) &&
         AddEntityMetadata(*db(), data_type, metadata_batch);
}

bool PlusAddressTable::CreatePlusAddressesTable() {
  return db()->DoesTableExist(kPlusAddressTable) ||
         db()->Execute(base::StringPrintf("CREATE TABLE %s (%s VARCHAR PRIMARY "
                                          "KEY, %s VARCHAR, %s VARCHAR)",
                                          kPlusAddressTable, kProfileId, kFacet,
                                          kPlusAddress));
}

bool PlusAddressTable::CreateSyncDataTypeStateTable() {
  return db()->DoesTableExist(kSyncDataTypeState) ||
         db()->Execute(base::StringPrintf(
             "CREATE TABLE %s (%s INTEGER PRIMARY KEY, %s BLOB)",
             kSyncDataTypeState, kModelType, kValue));
}

bool PlusAddressTable::CreateSyncEntityMetadataTable() {
  return db()->DoesTableExist(kSyncEntityMetadata) ||
         db()->Execute(base::StringPrintf(
             "CREATE TABLE %s (%s INTEGER, %s VARCHAR, %s BLOB, "
             "PRIMARY KEY (%s, %s))",
             kSyncEntityMetadata, kModelType, kStorageKey, kValue, kModelType,
             kStorageKey));
}

bool PlusAddressTable::MigrateToVersion126_InitialSchema() {
  return db()->Execute(
      base::StringPrintf("CREATE TABLE %s (%s VARCHAR PRIMARY KEY, %s VARCHAR)",
                         kPlusAddressTable, kFacet, kPlusAddress));
}

bool PlusAddressTable::MigrateToVersion127_SyncSupport() {
  sql::Transaction transaction(db());
  // The migration logic drops the existing `kPlusAddressTable` and recreates it
  // with a new schema. No data needs to be migrated between the tables, since
  // the table was not used yet.
  // Then, new `kSyncDataTypeState` and `kSyncEntityMetadata` tables are
  // created.
  return transaction.Begin() &&
         db()->Execute(base::StrCat({"DROP TABLE ", kPlusAddressTable})) &&
         db()->Execute(base::StringPrintf("CREATE TABLE %s (%s INTEGER PRIMARY "
                                          "KEY, %s VARCHAR, %s VARCHAR)",
                                          kPlusAddressTable, kProfileId, kFacet,
                                          kPlusAddress)) &&
         db()->Execute(base::StringPrintf(
             "CREATE TABLE %s (%s INTEGER PRIMARY KEY, %s BLOB)",
             kSyncDataTypeState, kModelType, kValue)) &&
         db()->Execute(base::StringPrintf(
             "CREATE TABLE %s (%s INTEGER, %s VARCHAR, %s BLOB, "
             "PRIMARY KEY (%s, %s))",
             kSyncEntityMetadata, kModelType, kStorageKey, kValue, kModelType,
             kStorageKey)) &&
         transaction.Commit();
}

bool PlusAddressTable::MigrateToVersion128_ProfileIdString() {
  sql::Transaction transaction(db());
  // Recreates `kPlusAddressTable`, with `kProfileId`'s type changed to VARCHAR.
  // No data needs to be migrated, since the table was not used yet.
  return transaction.Begin() &&
         db()->Execute(base::StrCat({"DROP TABLE ", kPlusAddressTable})) &&
         db()->Execute(base::StringPrintf("CREATE TABLE %s (%s VARCHAR PRIMARY "
                                          "KEY, %s VARCHAR, %s VARCHAR)",
                                          kPlusAddressTable, kProfileId, kFacet,
                                          kPlusAddress)) &&
         transaction.Commit();
}

}  // namespace plus_addresses
