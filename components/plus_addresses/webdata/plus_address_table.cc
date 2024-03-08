// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_table.h"

#include <vector>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "components/plus_addresses/plus_address_types.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace plus_addresses {

namespace {

constexpr char kPlusAddressTable[] = "plus_addresses";
constexpr char kProfileId[] = "profile_id";
constexpr char kFacet[] = "facet";
constexpr char kPlusAddress[] = "plus_address";

constexpr char kSyncModelTypeState[] = "plus_address_sync_model_type_state";
constexpr char kModelType[] = "model_type";
constexpr char kValue[] = "value";

constexpr char kSyncEntityMetadata[] = "plus_address_sync_entity_metadata";
// kModelType
constexpr char kStorageKey[] = "storage_key";
// kValue

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
  return CreatePlusAddressesTable() && CreateSyncModelTypeStateTable() &&
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
  }
  return true;
}

std::vector<PlusProfile> PlusAddressTable::GetPlusProfiles() const {
  sql::Statement query(db_->GetUniqueStatement(
      base::StringPrintf("SELECT %s, %s, %s FROM %s", kProfileId, kFacet,
                         kPlusAddress, kPlusAddressTable)
          .c_str()));
  std::vector<PlusProfile> result;
  while (query.Step()) {
    result.push_back({
        .profile_id = query.ColumnInt(0),
        .facet = query.ColumnString(1),
        .plus_address = query.ColumnString(2),
        .is_confirmed = true,
    });
  }
  return result;
}

bool PlusAddressTable::AddPlusProfile(const PlusProfile& profile) {
  CHECK(profile.is_confirmed);
  sql::Statement query(db_->GetUniqueStatement(
      base::StringPrintf("INSERT INTO %s (%s, %s, %s) VALUES (?, ?, ?)",
                         kPlusAddressTable, kProfileId, kFacet, kPlusAddress)
          .c_str()));
  query.BindInt64(0, profile.profile_id);
  query.BindString(1, profile.facet);
  query.BindString(2, profile.plus_address);
  return query.Run();
}

bool PlusAddressTable::ClearPlusProfiles() {
  return db_->Execute(
      base::StrCat({"DELETE FROM ", kPlusAddressTable}).c_str());
}

bool PlusAddressTable::CreatePlusAddressesTable() {
  return db_->DoesTableExist(kPlusAddressTable) ||
         db_->Execute(base::StringPrintf("CREATE TABLE %s (%s INTEGER PRIMARY "
                                         "KEY, %s VARCHAR, %s VARCHAR)",
                                         kPlusAddressTable, kProfileId, kFacet,
                                         kPlusAddress)
                          .c_str());
}

bool PlusAddressTable::CreateSyncModelTypeStateTable() {
  return db_->DoesTableExist(kSyncModelTypeState) ||
         db_->Execute(base::StringPrintf(
                          "CREATE TABLE %s (%s INTEGER PRIMARY KEY, %s BLOB)",
                          kSyncModelTypeState, kModelType, kValue)
                          .c_str());
}

bool PlusAddressTable::CreateSyncEntityMetadataTable() {
  return db_->DoesTableExist(kSyncEntityMetadata) ||
         db_->Execute(base::StringPrintf(
                          "CREATE TABLE %s (%s INTEGER, %s VARCHAR, %s BLOB, "
                          "PRIMARY KEY (%s, %s))",
                          kSyncEntityMetadata, kModelType, kStorageKey, kValue,
                          kModelType, kStorageKey)
                          .c_str());
}

bool PlusAddressTable::MigrateToVersion126_InitialSchema() {
  return db_->Execute(
      base::StringPrintf("CREATE TABLE %s (%s VARCHAR PRIMARY KEY, %s VARCHAR)",
                         kPlusAddressTable, kFacet, kPlusAddress)
          .c_str());
}

bool PlusAddressTable::MigrateToVersion127_SyncSupport() {
  sql::Transaction transaction(db_);
  // The migration logic drops the existing `kPlusAddressTable` and recreates it
  // with a new schema. No data needs to be migrated between the tables, since
  // the table was not used yet.
  // Then, new `kSyncModelTypeState` and `kSyncEntityMetadata` tables are
  // created.
  return transaction.Begin() &&
         db_->Execute(
             base::StrCat({"DROP TABLE ", kPlusAddressTable}).c_str()) &&
         db_->Execute(base::StringPrintf("CREATE TABLE %s (%s INTEGER PRIMARY "
                                         "KEY, %s VARCHAR, %s VARCHAR)",
                                         kPlusAddressTable, kProfileId, kFacet,
                                         kPlusAddress)
                          .c_str()) &&
         db_->Execute(base::StringPrintf(
                          "CREATE TABLE %s (%s INTEGER PRIMARY KEY, %s BLOB)",
                          kSyncModelTypeState, kModelType, kValue)
                          .c_str()) &&
         db_->Execute(base::StringPrintf(
                          "CREATE TABLE %s (%s INTEGER, %s VARCHAR, %s BLOB, "
                          "PRIMARY KEY (%s, %s))",
                          kSyncEntityMetadata, kModelType, kStorageKey, kValue,
                          kModelType, kStorageKey)
                          .c_str()) &&
         transaction.Commit();
}

}  // namespace plus_addresses
