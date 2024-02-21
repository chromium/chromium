// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_table.h"

#include "base/strings/stringprintf.h"

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
