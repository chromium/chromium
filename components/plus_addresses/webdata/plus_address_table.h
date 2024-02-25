// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_

#include <vector>

#include "components/plus_addresses/plus_address_types.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

namespace plus_addresses {

// Manages plus_address related tables in the Chrome profile scoped "Web Data"
// SQLite database.
// Owned by the `WebDatabaseBackend` managing the "Web Data" database, which is
// owned by the `WebDataServiceWrapper` keyed service.
//
// Schema:
// plus_addresses     A collection of confirmed `PlusProfile`s.
//  facet             The origin which this plus_address is associated with.
//                    Primary key.
//  plus_address      The email address associated with the facet. Even though
//                    this is guaranteed to be a valid email address, the
//                    database layer doesn't enforce this.
class PlusAddressTable : public WebDatabaseTable {
 public:
  PlusAddressTable();
  PlusAddressTable(const PlusAddressTable&) = delete;
  PlusAddressTable& operator=(const PlusAddressTable&) = delete;
  ~PlusAddressTable() override;

  // Retrieves the `PlusAddressTable` owned by `db`.
  static PlusAddressTable* FromWebDatabase(WebDatabase* db);

  // Returns all stored PlusProfiles - or an empty vector if reading fails.
  std::vector<PlusProfile> GetPlusProfiles() const;

  // Adds `profile` to the database and returns true if the operation succeeded.
  // Trying to add a `profile` for an already existing facet will fail.
  bool AddPlusProfile(const PlusProfile& profile);

  // Deletes all stored PlusProfiles, returning true if the operation succeeded.
  bool ClearPlusProfiles();

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

 private:
  // Creates the table of the given name in the newest version of the schema,
  // unless it already exists. Returns true if the table exists now.
  bool CreatePlusAddressesTable();

  // Migration logic to a specific version. Returns true if the migration
  // succeeded.
  bool MigrateToVersion126();
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
