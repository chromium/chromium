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
class AutofillWebDataBackend;
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

  // Determines whether this bridge should be monitoring the Wallet data. This
  // should be called whenever the data bridge sync state changes.
  void OnWalletDataTrackingStateChanged(bool is_tracking);

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

  // AutofillWebDataServiceObserverOnDBSequence implementation.
  void AutofillProfileChanged(const AutofillProfileChange& change) override;
  void CreditCardChanged(const CreditCardChange& change) override;
  void AutofillMultipleChanged() override;

 private:
  // Syncs up an updated entity |entity_after_change| (if needed).
  void SyncUpUpdatedEntity(
      std::unique_ptr<syncer::EntityData> entity_after_change);

  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable();

  // Synchronously load |cache_| and sync metadata from the autofill table
  // and pass the latter to the processor so that it can start tracking changes.
  void LoadDataCacheAndMetadata();

  // Reads local wallet metadata from the database and passes them into
  // |callback|. If |storage_keys_set| is not set, it returns all data entries.
  // Otherwise, it returns only entries with storage key in |storage_keys_set|.
  void GetDataImpl(
      base::Optional<std::unordered_set<std::string>> storage_keys_set,
      DataCallback callback);

  // AutofillWalletMetadataSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  ScopedObserver<AutofillWebDataBackend, AutofillWalletMetadataSyncBridge>
      scoped_observer_;

  // Cache of the data (local data + data that hasn't synced down yet); keyed by
  // storage keys. Needed for figuring out what to sync up when larger changes
  // happen in the local database.
  std::unordered_map<std::string, sync_pb::WalletMetadataSpecifics> cache_;

  // Indicates whether we should rely on wallet data being actively synced. If
  // true, the bridge will prune metadata entries without corresponding wallet
  // data entry.
  bool track_wallet_data_;

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<AutofillWalletMetadataSyncBridge> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletMetadataSyncBridge);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_METADATA_SYNC_BRIDGE_H_
