// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_OFFER_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_OFFER_SYNC_BRIDGE_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace autofill {

class AutofillSyncMetadataTable;
class AutofillWebDataBackend;
class AutofillWebDataService;
class PaymentsAutofillTable;

// Sync bridge responsible for applying remote changes of offer data to the
// local database.
class AutofillWalletOfferSyncBridge : public base::SupportsUserData::Data,
                                      public syncer::DataTypeSyncBridge {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  AutofillWalletOfferSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);
  ~AutofillWalletOfferSyncBridge() override;

  AutofillWalletOfferSyncBridge(const AutofillWalletOfferSyncBridge&) = delete;
  AutofillWalletOfferSyncBridge& operator=(
      const AutofillWalletOfferSyncBridge&) = delete;

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
  bool SupportsIncrementalUpdates() const override;
  void ApplyDisableSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                   delete_metadata_change_list) override;

 private:
  // Helper function to retrieve all offer data.
  std::unique_ptr<syncer::DataBatch> GetAllDataImpl();

  // Merges synced remote offer data.
  void MergeRemoteData(const syncer::EntityChangeList& entity_data);

  // Returns the table associated with the |web_data_backend_|.
  PaymentsAutofillTable* GetAutofillTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadAutofillOfferMetadata();

  // AutofillWalletOfferSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_OFFER_SYNC_BRIDGE_H_
