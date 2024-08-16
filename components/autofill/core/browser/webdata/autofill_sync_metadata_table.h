// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_

#include "components/sync/base/data_type.h"
#include "components/sync/model/sync_metadata_store.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace syncer {
class MetadataBatch;
}

namespace autofill {

// This class acts as a SyncMetadataStore for components/autofill. It stores the
// metadata in two tables inside the SQLite database passed to the constructor.
// It expects the following schemas:
// -----------------------------------------------------------------------------
// autofill_sync_metadata
//                      Sync-specific metadata for autofill records.
//
//   model_type         An int value corresponding to syncer::DataType enum.
//                      Added in version 78.
//   storage_key        A string that uniquely identifies the metadata record
//                      as well as the corresponding autofill record.
//   value              The serialized EntityMetadata record.
// -----------------------------------------------------------------------------
// autofill_model_type_state
//                      Contains sync DataTypeStates for autofill data types.
//
//   model_type         An int value corresponding to syncer::DataType enum.
//                      Added in version 78. Previously, the table was used only
//                      for one data type, there was an id column with value 1
//                      for the single entry.
//   value              The serialized DataTypeState record.
// -----------------------------------------------------------------------------
class AutofillSyncMetadataTable : public WebDatabaseTable,
                                  public syncer::SyncMetadataStore {
 public:
  AutofillSyncMetadataTable();

  AutofillSyncMetadataTable(const AutofillSyncMetadataTable&) = delete;
  AutofillSyncMetadataTable& operator=(const AutofillSyncMetadataTable&) =
      delete;

  ~AutofillSyncMetadataTable() override;

  // Retrieves the AutofillSyncMetadataTable* owned by |db|.
  static AutofillSyncMetadataTable* FromWebDatabase(WebDatabase* db);

  // Checks if the `data_type` corresponds to one of the Autofill-related
  // data types that store their metadata in `AutofillSyncMetadataTable`.
  static bool SupportsMetadataForDataType(syncer::DataType data_type);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Read all the stored metadata for |data_type| and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::DataType data_type,
                          syncer::MetadataBatch* metadata_batch);

  // Deletes all metadata for |data_type|.
  bool DeleteAllSyncMetadata(syncer::DataType data_type);

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

 private:
  int GetKeyValueForDataType(syncer::DataType data_type) const;

  bool GetAllSyncEntityMetadata(syncer::DataType data_type,
                                syncer::MetadataBatch* metadata_batch);

  bool GetDataTypeState(syncer::DataType data_type,
                        sync_pb::DataTypeState* state);

  bool InitAutofillSyncMetadataTable();
  bool InitDataTypeStateTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_
