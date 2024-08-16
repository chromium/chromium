// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "components/plus_addresses/plus_address_types.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_table.h"

namespace syncer {
class MetadataBatch;
}

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
// Even though plus addresses only use a single data type so far, more might
// be added in the future. For this reason, tables are keyed by data type.
// plus_address_sync_model_type_state
//   model_type         int identifying the DataType. Primary key.
//   value              A serialized DataTypeState record.
//
// plus_address_sync_entity_metadata
//   model_type         int identifying the DataType.
//   storage_key        The storage_key of the sync EntitySpecifics.
//     Composite (model_type, storage_key) primary key.
//   value              A serialized EntityMetadata record.
class PlusAddressTable : public WebDatabaseTable,
                         public syncer::SyncMetadataStore {
 public:
  PlusAddressTable();
  PlusAddressTable(const PlusAddressTable&) = delete;
  PlusAddressTable& operator=(const PlusAddressTable&) = delete;
  ~PlusAddressTable() override;

  // Retrieves the `PlusAddressTable` owned by `db`.
  static PlusAddressTable* FromWebDatabase(WebDatabase* db);

  // Returns all stored PlusProfiles - or an empty vector if reading fails.
  std::vector<PlusProfile> GetPlusProfiles() const;

  // Returns the profile with the given `profile_id` or std::nullopt if it
  // doesn't exist.
  std::optional<PlusProfile> GetPlusProfileForId(
      std::string_view profile_id) const;

  // Adds `profile` to the database, if a profile with the same `profile_id`
  // doesn't already exist. Otherwise, updates the existing `profile`.
  // Returns true if the operation succeeded.
  bool AddOrUpdatePlusProfile(const PlusProfile& profile);

  // Removes the profile with the given `profile_id` and returns true if the
  // operation succeeded. Trying to remove a non-existing profile is a no-op and
  // not considered a failure.
  bool RemovePlusProfile(std::string_view profile_id);

  // Deletes all stored PlusProfiles, returning true if the operation succeeded.
  bool ClearPlusProfiles();

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // syncer::SyncMetadataStore:
  bool UpdateEntityMetadata(syncer::DataType data_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) override;
  bool ClearEntityMetadata(syncer::DataType data_type,
                           const std::string& storage_key) override;
  bool UpdateDataTypeState(
      syncer::DataType data_type,
      const sync_pb::DataTypeState& data_type_state) override;
  bool ClearDataTypeState(syncer::DataType data_type) override;

  // Populates `metadata_batch` with all stored metadata for the `data_type`.
  // Returns true if all the reads succeeded.
  // If no metadata is stored for the data type, the function will succeed and
  // set the batch's data type state to the default state.
  bool GetAllSyncMetadata(syncer::DataType data_type,
                          syncer::MetadataBatch& metadata_batch);

 private:
  // Creates the table of the given name in the newest version of the schema,
  // unless it already exists. Returns true if the table exists now.
  bool CreatePlusAddressesTable();
  bool CreateSyncDataTypeStateTable();
  bool CreateSyncEntityMetadataTable();

  // Migration logic to a specific version. Returns true if the migration
  // succeeded.
  bool MigrateToVersion126_InitialSchema();
  bool MigrateToVersion127_SyncSupport();
  bool MigrateToVersion128_ProfileIdString();
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
