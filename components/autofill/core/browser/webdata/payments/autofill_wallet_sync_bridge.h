// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_SYNC_BRIDGE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_SYNC_BRIDGE_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/supports_user_data.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/sync/model/data_type_local_change_processor.h"
#include "components/sync/model/data_type_sync_bridge.h"
#include "components/sync/model/metadata_change_list.h"
#include "components/sync/model/model_error.h"

namespace autofill {

class AutofillSyncMetadataTable;
class AutofillWebDataBackend;
class AutofillWebDataService;
class CreditCard;
class Iban;
struct CreditCardCloudTokenData;
class PaymentsAutofillTable;
struct PaymentsCustomerData;

// Sync bridge responsible for propagating local changes to the processor and
// applying remote changes to the local database.
class AutofillWalletSyncBridge : public base::SupportsUserData::Data,
                                 public syncer::DataTypeSyncBridge {
 public:
  // Factory method that hides dealing with change_processor and also stores the
  // created bridge within |web_data_service|. This method should only be
  // called on |web_data_service|'s DB thread.
  static void CreateForWebDataServiceAndBackend(
      const std::string& app_locale,
      AutofillWebDataBackend* webdata_backend,
      AutofillWebDataService* web_data_service);

  static syncer::DataTypeSyncBridge* FromWebDataService(
      AutofillWebDataService* web_data_service);

  explicit AutofillWalletSyncBridge(
      std::unique_ptr<syncer::DataTypeLocalChangeProcessor> change_processor,
      AutofillWebDataBackend* web_data_backend);

  AutofillWalletSyncBridge(const AutofillWalletSyncBridge&) = delete;
  AutofillWalletSyncBridge& operator=(const AutofillWalletSyncBridge&) = delete;

  ~AutofillWalletSyncBridge() override;

  // DataTypeSyncBridge implementation.
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

  // Retrieves all Wallet Data from local table, converts to EntityData and
  // returns it. Keeps all the strings in their original format (whereas
  // GetAllDataForDebugging() has to make them UTF-8).
  std::unique_ptr<syncer::DataBatch> GetAllDataForTesting();

 private:
  // Sends all Wallet Data to the |callback|. If |enforce_utf8|, the string
  // fields that are in non-UTF-8 get encoded so that they conform to UTF-8.
  std::unique_ptr<syncer::DataBatch> GetAllDataImpl(bool enforce_utf8);

  // Sets the wallet data from `entity_data` to this client and records metrics
  // about added/deleted data. If `notify_webdata_backend`, it also notifies
  // the metadata sync bridge about individual changes.
  void SetSyncData(const syncer::EntityChangeList& entity_data,
                   bool notify_webdata_backend);

  // Sets |customer_data| to this client and returns whether any change has been
  // applied (i.e., whether |customer_data| was different from local data).
  bool SetPaymentsCustomerData(std::vector<PaymentsCustomerData> customer_data);

  // Sets `wallet_cards` to this client and returns whether any change has been
  // applied (i.e., whether `wallet_cards` was different from local data). If
  // `notify_webdata_backend`, it also notifies via WebDataBackend about any
  // individual entity changes.
  bool SetWalletCards(std::vector<CreditCard> wallet_cards,
                      bool notify_webdata_backend);

  // Sets `card_benefits` to this client and returns whether any change has
  // been applied (i.e., whether `card_benefits` was different from the local
  // data).
  bool SetCardBenefits(std::vector<CreditCardBenefit> card_benefits);

  // Sets `wallet_ibans` to this client and returns whether any change has been
  // applied (i.e., whether `wallet_ibans` was different from local data). If
  // `notify_webdata_backend` is true, it also notifies via WebDataBackend about
  // any individual entity changes.
  bool SetWalletIbans(std::vector<Iban> wallet_ibans,
                      bool notify_webdata_backend);

  // Sets |cloud_token_data| to this client and returns whether any change has
  // been applied (i.e., whether |cloud_token_data| was different from the local
  // data).
  bool SetCreditCardCloudTokenData(
      const std::vector<CreditCardCloudTokenData>& cloud_token_data);

  // Checks whether `bank_accounts` returned by the server are different from
  // local data and if so, writes the server data to the local database.
  bool SetBankAccountsData(const std::vector<BankAccount>& bank_accounts);

  // Checks whether `payment_instruments` returned by the server are different
  // from local data and if so, writes the server data to the local database.
  bool SetPaymentInstrumentsData(
      const std::vector<sync_pb::PaymentInstrument>& payment_instruments);

  // Returns the table associated with the |web_data_backend_|.
  PaymentsAutofillTable* GetAutofillTable();

  AutofillSyncMetadataTable* GetSyncMetadataStore();

  // Synchronously load sync metadata from the autofill table and pass it to the
  // processor so that it can start tracking changes.
  void LoadMetadata();

  // Logs virtual card metadata changes.
  void LogVirtualCardMetadataChanges(
      const std::vector<std::unique_ptr<CreditCard>>& old_data,
      const std::vector<CreditCard>& new_data);

  // Get the list of CVCs whose corresponding card has been deleted from
  // pay.google.com. When the list of CVCs is retrieved, they are deleted from
  // the local db. A deletion flow to the Chrome Sync server will also be
  // triggered.
  void ReconcileServerCvcForWalletCards();

  // AutofillProfileSyncBridge is owned by |web_data_backend_| through
  // SupportsUserData, so it's guaranteed to outlive |this|.
  const raw_ptr<AutofillWebDataBackend> web_data_backend_;

  // The bridge should be used on the same sequence where it is constructed.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_PAYMENTS_AUTOFILL_WALLET_SYNC_BRIDGE_H_
