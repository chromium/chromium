// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_backend.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace syncer {
struct EntityData;
}  // namespace syncer

namespace autofill {

class AutofillTable;
class AutofillWebDataService;

// Sync bridge responsible for propagating local changes to the processor and
// applying remote changes to the local database.
class AutofillWalletMetadataSyncBridge
    : public base::SupportsUserData::Data,
      public syncer::ModelTypeSyncBridge,
      public AutofillWebDataServiceObserverOnDBSequence {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      const std::string& app_locale,
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static AutofillWalletMetadataSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  AutofillWalletMetadataSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);
  ~AutofillWalletMetadataSyncBridge() override;

  base::WeakPtr<AutofillWalletMetadataSyncBridge> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // ModelTypeSyncBridge implementation.
  std::unique_ptr<syncer::MetadataChangeList> CreateMetadataChangeList()
      override;
  base::Optional<syncer::ModelError> MergeSyncData(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data) override;
  base::Optional<syncer::ModelError> ApplySyncChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_changes) override;
  void GetData(StorageKeyList storage_keys, DataCallback callback) override;
  void GetAllDataForDebugging(DataCallback callback) override;
  std::string GetClientTag(const syncer::EntityData& entity_data) override;
  std::string GetStorageKey(const syncer::EntityData& entity_data) override;
  void ApplyStopSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                delete_metadata_change_list) override;

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;
  void CreditCardChanged(const CreditCardChange& change) override;

 private:
  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable();

  // Synchronously load the sync data into |cache_| and sync metadata from the
  // autofill table and pass the latter to the processor so that it can start
  // tracking changes.
  void LoadDataCacheAndMetadata();

  // Deletes old metadata entities that have no corresponding data entities.
  // This routine is here to help with really corner-case scenarios, e.g.
  //  - having one client create a metadata entity M for new data D while other
  //  clients are off;
  //  - switch off this client forever and remove the entity D from Wallet;
  //  - turn on other clients so that they receive M from sync;
  //  - these other clients never knew about D and thus they have no reason to
  //  delete M when they receive an update from the Walllet server.
  void DeleteOldOrphanMetadata();

  // Reads local wallet metadata from the database and passes them into
  // |callback|. If |storage_keys_set| is not set, it returns all data entries.
  // Otherwise, it returns only entries with storage key in |storage_keys_set|.
  void GetDataImpl(
      base::Optional<std::unordered_set<std::string>> storage_keys_set,
      DataCallback callback);

  // Uploads local data that is not part of |entity_data| sent from the server
  // during initial MergeSyncData().
  void UploadInitialLocalData(syncer::MetadataChangeList* metadata_change_list,
                              const syncer::EntityChangeList& entity_data);

  // Merges remote changes, specified in |entity_data|, with the local DB and,
  // potentially, writes changes to the local DB and/or commits updates of
  // entities from |entity_data| up to sync.
  base::Optional<syncer::ModelError> MergeRemoteChanges(
      std::unique_ptr<syncer::MetadataChangeList> metadata_change_list,
      syncer::EntityChangeList entity_data);

  // Reacts to a local |change| of an entry of type |type|.
  template <class DataType>
  void LocalMetadataChanged(sync_pb::WalletMetadataSpecifics::Type type,
                            AutofillDataModelChange<DataType> change);

  // AutofillWalletMetadataSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  ScopedObserver<AutofillWebDataBackend,
                 AutofillWebDataServiceObserverOnDBSequence>
      scoped_observer_{this};

  // Cache of the local data that allows figuring out the diff for local
  // changes; keyed by storage keys.
  std::map<std::string, AutofillMetadata> cache_;

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AutofillWalletMetadataSyncBridge> weak_ptr_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridge);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNC_BRIDGE_H_
