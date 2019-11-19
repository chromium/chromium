// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"
#include "components/sync/model/model_type_change_processor.h"
#include "components/sync/model/model_type_sync_bridge.h"

namespace autofill {

class AutofillProfile;
class AutofillTable;
class AutofillWebDataBackend;
class AutofillWebDataService;
class CreditCard;
struct PaymentsCustomerData;

// Sync bridge responsible for propagating local changes to the processor and
// applying remote changes to the local database.
class AutofillWalletSyncBridge : public base::SupportsUserData::Data,
                                 public syncer::ModelTypeSyncBridge {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      const std::string& app_locale,
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static syncer::ModelTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  explicit AutofillWalletSyncBridge(
      std::unique_ptr<syncer::ModelTypeChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);
  ~AutofillWalletSyncBridge() override;

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
  bool SupportsIncrementalUpdates() const override;
  void ApplyStopSyncChanges(std::unique_ptr<syncer::MetadataChangeList>
                                delete_metadata_change_list) override;

  // Sends all Wallet Data to the |callback| and keeps all the strings in their
  // original format (whereas GetAllDataForDebugging() has to make them UTF-8).
  void GetAllDataForTesting(DataCallback callback);

 private:
  template <class Item>
  struct AutofillWalletDiff {
    int items_added = 0;
    int items_removed = 0;
    std::vector<AutofillDataModelChange<Item>> changes;

    bool IsEmpty() const { return items_added == 0 && items_removed == 0; }
  };

  // Sends all Wallet Data to the |callback|. If |enforce_utf8|, the string
  // fields that are in non-UTF-8 get encoded so that they conform to UTF-8.
  void GetAllDataImpl(DataCallback callback, bool enforce_utf8);

  // Sets the wallet data from |entity_data| to this client and records metrics
  // about added/deleted data. If |notify_metadata_bridge|, it also notifies
  // the metadata sync bridge about individual changes.
  void SetSyncData(const syncer::EntityChangeList& entity_data,
                   bool notify_metadata_bridge);

  // Sets |customer_data| to this client and returns whether any change has been
  // applied (i.e., whether |customer_data| was different from local data).
  bool SetPaymentsCustomerData(std::vector<PaymentsCustomerData> customer_data);

  // Sets |wallet_cards| to this client and returns whether any change has been
  // applied (i.e., whether |wallet_cards| was different from local data). If
  // |notify_metadata_bridge|, it also notifies via WebDataBackend about any
  // individual entity changes.
  bool SetWalletCards(std::vector<CreditCard> wallet_cards,
                      bool notify_metadata_bridge);

  // Sets |wallet_addresses| to this client and returns whether any change has
  // been applied (i.e., whether |wallet_addresses| was different from local
  // data). If |notify_metadata_bridge|, it also notifies via WebDataBackend
  // about any individual entity changes.
  bool SetWalletAddresses(std::vector<AutofillProfile> wallet_addresses,
                          bool notify_metadata_bridge);

  // Computes a "diff" (items added, items removed) of two vectors of items,
  // which should be either CreditCard or AutofillProfile. This is used for
  // three purposes:
  // 1) Detecting if anything has changed, so that we don't write to disk in the
  //    common case where nothing has changed.
  // 3) Notifying |web_data_backend_| of any changes.
  // 2) Recording metrics on the number of added/removed items.
  template <class Item>
  AutofillWalletDiff<Item> ComputeAutofillWalletDiff(
      const std::vector<std::unique_ptr<Item>>& old_data,
      const std::vector<Item>& new_data);

  // Returns the table associated with the |web_data_backend_|.
  AutofillTable* GetAutofillTable();

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  // AutofillProfileSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  AutofillWebDataBackend* const web_data_backend_;

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AutofillWalletSyncBridge);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_WALLET_SYNC_BRIDGE_H_
