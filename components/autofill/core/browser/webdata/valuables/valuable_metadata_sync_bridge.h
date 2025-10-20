// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_METADATA_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_METADATA_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_sync_metadata_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/valuables/valuables_table.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/entity_change.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"
#include "components/sync/protocol/autofill_valuable_metadata_specifics.pb.h"
#include "components/sync/protocol/entity_data.h"

namespace autofill {

class AutofillWebDataService;

class ValuableMetadataSyncBridge : public base::SupportsUserData::Data,
                                   public syncer::DataTypeSyncBridge {
 public:
  ValuableMetadataSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* backend);
  ~ValuableMetadataSyncBridge() override;

  ValuableMetadataSyncBridge(const ValuableMetadataSyncBridge&) = delete;
  ValuableMetadataSyncBridge& operator=(const ValuableMetadataSyncBridge&) =
      delete;

  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* web_data_backend,
      AutofillWebDataService* web_data_service);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::DataTypeSyncBridge:
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  std::optional<syncer::ModelError> MergeFullSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  std::optional<syncer::ModelError> ApplyIncrementalSyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  std::unique_ptr<syncer::DataBatch> GetDataForCommit(
      StorageKeyList storage_keys) override;
  std::unique_ptr<syncer::DataBatch> GetAllDataForDebugging() override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;
  std::string GetClientTag(
      const syncer::EntityData& entity_data) const override;
  std::string GetStorageKey(
      const syncer::EntityData& entity_data) const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  sync_pb::EntitySpecifics TrimAllSupportedFieldsFromRemoteSpecifics(
      const sync_pb::EntitySpecifics& entity_specifics) const override;

 private:
  // Merges remote changes, specified in `entity_data`, with the local DB and,
  // potentially, writes changes to the local DB and/or commits updates of
  // entities from `entity_data` up to sync.
  std::optional<syncer::ModelError> MergeRemoteChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data);

  // Returns the `ValuablesTable` associated with the `web_data_backend_`.
  ValuablesTable* GetValuablesTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Queries all valuable metadata from the database.
  // These are converted to their `AutofillValuableMetadataSpecifics`
  // representation and returned as a `syncer::MutableDataBatch`.
  std::unique_ptr<syncer::MutableDataBatch> GetAllData();

  // The bridge should be used on the same sequence where it has been
  // constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  // ValuableMetadataSyncBridge is owned by `web_data_backend_` through
  // SupportsUserData, so it's guaranteed to outlive `this`.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_VALUABLES_VALUABLE_METADATA_SYNC_BRIDGE_H_
