// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/mutable_data_batch.h"

namespace autofill {

class AutofillSyncMetadataTable;
class AutofillWebDataBackend;
class AutofillWebDataService;
class PaymentsAutofillTable;

// Sync bridge responsible for applying changes of autofill wallet
// credential data between the local database and the Chrome sync server.
class AutofillWalletCredentialSyncBridge
    : public base::SupportsUserData::Data,
      public syncer::DataTypeSyncBridge,
      public AutofillWebDataServiceObserverOnDBSequence {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within `web_data_service`. This method should only be
  // called on `web_data_service`'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static AutofillWalletCredentialSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  explicit AutofillWalletCredentialSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);

  AutofillWalletCredentialSyncBridge(
      const AutofillWalletCredentialSyncBridge&) = delete;
  AutofillWalletCredentialSyncBridge& operator=(
      const AutofillWalletCredentialSyncBridge&) = delete;

  ~AutofillWalletCredentialSyncBridge() override;

  // DataTypeSyncBridge
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
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;
  bool IsEntityDataValid(const syncer::EntityData& entity_data) const override;

  // AutofillWebDataServiceObserverOnDBSequence.
  void ServerCvcChanged(const ServerCvcChange& change) override;

 private:
  // AutofillWalletCredentialDataSyncBridge is owned by `web_data_backend_`
  // through SupportsUserData, so it's guaranteed to outlive `this`.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  // Returns the table associated with the `web_data_backend_`.
  PaymentsAutofillTable* GetAutofillTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Syncing the changes on the local storage related to Wallet credentials aka
  // CVC to the Chrome Sync server. `change` has the data which was updated in
  // the local database.
  void ActOnLocalChange(const ServerCvcChange& change);

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  // Returns the `server_cvc_list` as MutableDataBatch.
  std::unique_ptr<syncer::MutableDataBatch> ConvertToDataBatch(
      const std::vector<std::unique_ptr<ServerCvc>>& server_cvc_list);

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  // This is used to keep track of changes on `AutofillWebDataBackend` and
  // allows the trigger for `ServerCvcChanged`.
  base::ScopedObservation<AutofillWebDataBackend,
                          AutofillWebDataServiceObserverOnDBSequence>
      scoped_observation_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_CREDENTIAL_SYNC_BRIDGE_H_
