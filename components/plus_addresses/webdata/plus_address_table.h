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
// Schema to store PlusProfiles:
// plus_addresses     A collection of confirmed `PlusProfile`s.
//  profile_id        A unique identifier from the PlusAddress backend that
//                    identifies it. Primary key.
//  facet             The origin which this plus_address is associated with.
//  plus_address      The email address associated with the facet. Even though
//                    this is guaranteed to be a valid email address, the
//                    database layer doesn't enforce this.
//
// Schema to implement `syncer::SyncMetadataStore`.
// TODO(b/322147254): Implement the interface.
// Even though plus addresses only use a single model type so far, more might
// be added in the future. For this reason, tables are keyed by model type.
// plus_address_sync_model_type_state
//   model_type         int identifying the ModelType. Primary key.
//   value              A serialized ModelTypeState record.
//
// plus_address_sync_entity_metadata
//   model_type         int identifying the ModelType.
//   storage_key        The storage_key of the sync EntitySpecifics.
//     Composite (model_type, storage_key) primary key.
//   value              A serialized EntityMetadata record.
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
  // Trying to add a `profile` for an already existing profile_id will fail.
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
  bool CreateSyncModelTypeStateTable();
  bool CreateSyncEntityMetadataTable();

  // Migration logic to a specific version. Returns true if the migration
  // succeeded.
  bool MigrateToVersion126_InitialSchema();
  bool MigrateToVersion127_SyncSupport();
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
