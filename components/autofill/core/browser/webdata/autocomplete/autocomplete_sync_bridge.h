// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_SYNC_BRIDGE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace autofill {

class AutofillSyncMetadataTable;
class AutofillWebDataService;

class AutocompleteSyncBridge
    : public base::SupportsUserData::Data,
      public syncer::DataTypeSyncBridge,
      public AutofillWebDataServiceObserverOnDBSequence {
 public:
  AutocompleteSyncBridge();
  AutocompleteSyncBridge(
      AutofillWebDataBackend* backend,
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor);

  AutocompleteSyncBridge(const AutocompleteSyncBridge&) = delete;
  AutocompleteSyncBridge& operator=(const AutocompleteSyncBridge&) = delete;

  ~AutocompleteSyncBridge() override;

  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataService* web_data_service,
      AutofillWebDataBackend* web_data_backend);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  // syncer::DataTypeSyncBridge implementation.
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
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutocompleteEntriesChanged(
      const AutocompleteChangeList& changes) override;

 private:
  // Returns the table associated with the |web_data_backend_|.
  AutocompleteTable* GetAutocompleteTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Respond to local autocomplete entries changing by notifying sync of the
  // changes.
  void ActOnLocalChanges(const AutocompleteChangeList& changes);

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  SEQUENCE_CHECKER(sequence_checker_);

  // AutocompleteSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  base::ScopedObservation<AutofillWebDataBackend,
                          AutofillWebDataServiceObserverOnDBSequence>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_SYNC_BRIDGE_H_
