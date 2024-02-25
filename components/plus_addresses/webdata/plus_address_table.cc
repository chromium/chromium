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

namespace plus_addresses {

namespace {

constexpr char kPlusAddressTable[] = "plus_addresses";
constexpr char kFacet[] = "facet";
constexpr char kPlusAddress[] = "plus_address";

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
  return CreatePlusAddressesTable();
}

bool PlusAddressTable::MigrateToVersion(int version,
                                        bool* update_compatible_version) {
  switch (version) {
    case 126:
      *update_compatible_version = false;
      return MigrateToVersion126();
  }
  return true;
}

std::vector<PlusProfile> PlusAddressTable::GetPlusProfiles() const {
  sql::Statement query(db_->GetUniqueStatement(
      base::StringPrintf("SELECT %s, %s FROM %s", kFacet, kPlusAddress,
                         kPlusAddressTable)
          .c_str()));
  std::vector<PlusProfile> result;
  while (query.Step()) {
    result.push_back({
        .facet = query.ColumnString(0),
        .plus_address = query.ColumnString(1),
        .is_confirmed = true,
    });
  }
  return result;
}

bool PlusAddressTable::AddPlusProfile(const PlusProfile& profile) {
  CHECK(profile.is_confirmed);
  sql::Statement query(db_->GetUniqueStatement(
      base::StringPrintf("INSERT INTO %s (%s, %s) VALUES (?, ?)",
                         kPlusAddressTable, kFacet, kPlusAddress)
          .c_str()));
  query.BindString(0, profile.facet);
  query.BindString(1, profile.plus_address);
  return query.Run();
}

bool PlusAddressTable::ClearPlusProfiles() {
  return db_->Execute(
      base::StrCat({"DELETE FROM ", kPlusAddressTable}).c_str());
}

bool PlusAddressTable::CreatePlusAddressesTable() {
  return db_->DoesTableExist(kPlusAddressTable) ||
         db_->Execute(
             base::StringPrintf(
                 "CREATE TABLE %s (%s VARCHAR PRIMARY KEY, %s VARCHAR)",
                 kPlusAddressTable, kFacet, kPlusAddress)
                 .c_str());
}

bool PlusAddressTable::MigrateToVersion126() {
  return db_->Execute(
      base::StringPrintf("CREATE TABLE %s (%s VARCHAR PRIMARY KEY, %s VARCHAR)",
                         kPlusAddressTable, kFacet, kPlusAddress)
          .c_str());
}

}  // namespace plus_addresses
