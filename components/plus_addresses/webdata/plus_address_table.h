// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_TABLE_H_

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

  // Adds `profile` to the database and returns true if the operation succeeded.
  // Trying to add a `profile` for an already existing profile_id will fail.
  bool AddPlusProfile(const PlusProfile& profile);

  // Deletes all stored PlusProfiles, returning true if the operation succeeded.
  bool ClearPlusProfiles();

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // syncer::SyncMetadataStore:
  bool UpdateEntityMetadata(syncer::ModelType model_type,
                            const std::string& storage_key,
                            const sync_pb::EntityMetadata& metadata) override;
  bool ClearEntityMetadata(syncer::ModelType model_type,
                           const std::string& storage_key) override;
  bool UpdateModelTypeState(
      syncer::ModelType model_type,
      const sync_pb::ModelTypeState& model_type_state) override;
  bool ClearModelTypeState(syncer::ModelType model_type) override;

  // Populates `metadata_batch` with all stored metadata for the `model_type`.
  // Returns true if all the reads succeeded.
  // If no metadata is stored for the model type, the function will succeed and
  // set the batch's model type state to the default state.
  bool GetAllSyncMetadata(syncer::ModelType model_type,
                          syncer::MetadataBatch& metadata_batch);

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
