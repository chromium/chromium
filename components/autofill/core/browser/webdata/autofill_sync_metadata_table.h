// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_

#include "components/sync/base/model_type.h"
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
//   model_type         An int value corresponding to syncer::ModelType enum.
//                      Added in version 78.
//   storage_key        A string that uniquely identifies the metadata record
//                      as well as the corresponding autofill record.
//   value              The serialized EntityMetadata record.
// -----------------------------------------------------------------------------
// autofill_model_type_state
//                      Contains sync ModelTypeStates for autofill model types.
//
//   model_type         An int value corresponding to syncer::ModelType enum.
//                      Added in version 78. Previously, the table was used only
//                      for one model type, there was an id column with value 1
//                      for the single entry.
//   value              The serialized ModelTypeState record.
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

  // Checks if the `model_type` corresponds to one of the Autofill-related
  // model types that store their metadata in `AutofillSyncMetadataTable`.
  static bool SupportsMetadataForModelType(syncer::ModelType model_type);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Read all the stored metadata for |model_type| and fill |metadata_batch|
  // with it.
  bool GetAllSyncMetadata(syncer::ModelType model_type,
                          syncer::MetadataBatch* metadata_batch);

  // Deletes all metadata for |model_type|.
  bool DeleteAllSyncMetadata(syncer::ModelType model_type);

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

 private:
  int GetKeyValueForModelType(syncer::ModelType model_type) const;

  bool GetAllSyncEntityMetadata(syncer::ModelType model_type,
                                syncer::MetadataBatch* metadata_batch);

  bool GetModelTypeState(syncer::ModelType model_type,
                         sync_pb::ModelTypeState* state);

  bool InitAutofillSyncMetadataTable();
  bool InitModelTypeStateTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_SYNC_METADATA_TABLE_H_
