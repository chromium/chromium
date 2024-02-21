// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager.h"

#include <memory>

#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_observer.h"
#include "components/sync/base/model_type.h"

namespace autofill {

namespace {

// Receives the loaded profiles from the web data service and stores them in
// |*dest|. The pending handle is the address of the pending handle
// corresponding to this request type. This function is used to save both server
// and local profiles and credit cards.
template <typename ValueType>
void ReceiveLoadedDbValues(WebDataServiceBase::Handle h,
                           WDTypedResult* result,
                           WebDataServiceBase::Handle* pending_handle,
                           std::vector<ValueType>* dest) {
  DCHECK_EQ(*pending_handle, h);
  *pending_handle = 0;

  *dest = std::move(
      static_cast<WDResult<std::vector<ValueType>>*>(result)->GetValue());
}

}  // namespace

// Helper class to abstract the switching between account and profile storage
// for server cards away from the rest of PaymentsDataManager.
class PaymentsDatabaseHelper {
 public:
  PaymentsDatabaseHelper(PaymentsDataManager* payments_data_manager,
                         scoped_refptr<AutofillWebDataService> profile_database,
                         scoped_refptr<AutofillWebDataService> account_database)
      : profile_database_(profile_database),
        account_database_(account_database),
        payments_data_manager_(payments_data_manager) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }

    // Start observing the profile database. Don't observe the account database
    // until we know that we should use it.
    profile_database_->AddObserver(payments_data_manager_);

    // If we don't have an account_database , we always use the profile database
    // for server data.
    if (!account_database_) {
      server_database_ = profile_database_;
    } else {
      // Wait for the call to SetUseAccountStorageForServerData to decide
      // which database to use for server data.
      server_database_ = nullptr;
    }
  }

  PaymentsDatabaseHelper(const PaymentsDatabaseHelper&) = delete;
  PaymentsDatabaseHelper& operator=(const PaymentsDatabaseHelper&) = delete;

  ~PaymentsDatabaseHelper() {
    if (profile_database_) {
      profile_database_->RemoveObserver(payments_data_manager_);
    }

    // If we have a different server database, also remove its observer.
    if (server_database_ && server_database_ != profile_database_) {
      server_database_->RemoveObserver(payments_data_manager_);
    }
  }

  // Returns the database that should be used for storing local data.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase() {
    return profile_database_;
  }

  // Returns the database that should be used for storing server data.
  scoped_refptr<AutofillWebDataService> GetServerDatabase() {
    return server_database_;
  }

  // Whether we're currently using the ephemeral account storage for saving
  // server data.
  bool IsUsingAccountStorageForServerData() {
    return server_database_ != profile_database_;
  }

  // Set whether this should use the passed in account storage for server
  // addresses. If false, this will use the profile_storage.
  // It's an error to call this if no account storage was passed in at
  // construction time.
  void SetUseAccountStorageForServerData(
      bool use_account_storage_for_server_cards) {
    if (!profile_database_) {
      // In some tests, there are no dbs.
      return;
    }
    scoped_refptr<AutofillWebDataService> new_server_database =
        use_account_storage_for_server_cards ? account_database_
                                             : profile_database_;
    DCHECK(new_server_database != nullptr)
        << "SetUseAccountStorageForServerData("
        << use_account_storage_for_server_cards << "): storage not available.";

    if (new_server_database == server_database_) {
      // Nothing to do :)
      return;
    }

    if (server_database_ != nullptr) {
      if (server_database_ != profile_database_) {
        // Remove the previous observer if we had any.
        server_database_->RemoveObserver(payments_data_manager_);
      }
      payments_data_manager_->CancelPendingServerQueries();
    }
    server_database_ = new_server_database;
    // We don't need to add an observer if server_database_ is equal to
    // profile_database_, because we're already observing that.
    if (server_database_ != profile_database_) {
      server_database_->AddObserver(payments_data_manager_);
    }
    // Notify the manager that the database changed.
    payments_data_manager_->Refresh();
  }

 private:
  scoped_refptr<AutofillWebDataService> profile_database_;
  scoped_refptr<AutofillWebDataService> account_database_;

  // The database that should be used for server data. This will always be equal
  // to either profile_database_, or account_database_.
  scoped_refptr<AutofillWebDataService> server_database_;

  raw_ptr<PaymentsDataManager> payments_data_manager_;
};

PaymentsDataManager::PaymentsDataManager(
    scoped_refptr<AutofillWebDataService> profile_database,
    scoped_refptr<AutofillWebDataService> account_database,
    PersonalDataManager* pdm)
    : pdm_(pdm) {
  database_helper_ = std::make_unique<PaymentsDatabaseHelper>(
      this, profile_database, account_database);
}

PaymentsDataManager::~PaymentsDataManager() {
  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQueries();
}

void PaymentsDataManager::OnAutofillChangedBySync(
    syncer::ModelType model_type) {
  if (model_type == syncer::AUTOFILL_WALLET_CREDENTIAL ||
      model_type == syncer::AUTOFILL_WALLET_DATA ||
      model_type == syncer::AUTOFILL_WALLET_METADATA ||
      model_type == syncer::AUTOFILL_WALLET_OFFER ||
      model_type == syncer::AUTOFILL_WALLET_USAGE) {
    Refresh();
  }
}

void PaymentsDataManager::OnWebDataServiceRequestDone(
    WebDataServiceBase::Handle h,
    std::unique_ptr<WDTypedResult> result) {
  DCHECK(pending_creditcards_query_ || pending_server_creditcards_query_ ||
         pending_server_creditcard_cloud_token_data_query_ ||
         pending_local_ibans_query_ || pending_server_ibans_query_ ||
         pending_customer_data_query_ || pending_offer_data_query_ ||
         pending_virtual_card_usage_data_query_ ||
         pending_credit_card_benefit_query_);

  if (!result) {
    // Error from the web database.
    if (h == pending_creditcards_query_) {
      pending_creditcards_query_ = 0;
    } else if (h == pending_server_creditcards_query_) {
      pending_server_creditcards_query_ = 0;
    } else if (h == pending_server_creditcard_cloud_token_data_query_) {
      pending_server_creditcard_cloud_token_data_query_ = 0;
    } else if (h == pending_local_ibans_query_) {
      pending_local_ibans_query_ = 0;
    } else if (h == pending_server_ibans_query_) {
      pending_server_ibans_query_ = 0;
    } else if (h == pending_customer_data_query_) {
      pending_customer_data_query_ = 0;
    } else if (h == pending_offer_data_query_) {
      pending_offer_data_query_ = 0;
    } else if (h == pending_virtual_card_usage_data_query_) {
      pending_virtual_card_usage_data_query_ = 0;
    } else if (h == pending_credit_card_benefit_query_) {
      pending_credit_card_benefit_query_ = 0;
    }
  } else {
    switch (result->GetType()) {
      case AUTOFILL_CREDITCARDS_RESULT:
        if (h == pending_creditcards_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_creditcards_query_,
                                &local_credit_cards_);
        } else {
          DCHECK_EQ(h, pending_server_creditcards_query_)
              << "received creditcards from invalid request.";
          ReceiveLoadedDbValues(h, result.get(),
                                &pending_server_creditcards_query_,
                                &server_credit_cards_);
          pdm_->OnServerCreditCardsRefreshed();
        }
        break;
      case AUTOFILL_CLOUDTOKEN_RESULT:
        DCHECK_EQ(h, pending_server_creditcard_cloud_token_data_query_)
            << "received credit card cloud token data from invalid request.";
        ReceiveLoadedDbValues(
            h, result.get(), &pending_server_creditcard_cloud_token_data_query_,
            &server_credit_card_cloud_token_data_);
        break;
      case AUTOFILL_IBANS_RESULT:
        if (h == pending_local_ibans_query_) {
          ReceiveLoadedDbValues(h, result.get(), &pending_local_ibans_query_,
                                &local_ibans_);
        } else {
          DCHECK_EQ(h, pending_server_ibans_query_)
              << "received ibans from invalid request.";
          ReceiveLoadedDbValues(h, result.get(), &pending_server_ibans_query_,
                                &server_ibans_);
        }
        break;
      case AUTOFILL_CUSTOMERDATA_RESULT:
        DCHECK_EQ(h, pending_customer_data_query_)
            << "received customer data from invalid request.";
        pending_customer_data_query_ = 0;

        payments_customer_data_ =
            static_cast<WDResult<std::unique_ptr<PaymentsCustomerData>>*>(
                result.get())
                ->GetValue();
        break;
      case AUTOFILL_OFFER_DATA:
        DCHECK_EQ(h, pending_offer_data_query_)
            << "received autofill offer data from invalid request.";
        ReceiveLoadedDbValues(h, result.get(), &pending_offer_data_query_,
                              &autofill_offer_data_);
        break;
      case AUTOFILL_VIRTUAL_CARD_USAGE_DATA:
        DCHECK_EQ(h, pending_virtual_card_usage_data_query_)
            << "received autofill virtual card usage data from invalid "
               "request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_virtual_card_usage_data_query_,
                              &autofill_virtual_card_usage_data_);
        break;
      case CREDIT_CARD_BENEFIT_RESULT:
        DCHECK_EQ(h, pending_credit_card_benefit_query_)
            << "received credit card benefit from invalid request.";
        ReceiveLoadedDbValues(h, result.get(),
                              &pending_credit_card_benefit_query_,
                              &credit_card_benefits_);
        break;
      default:
        NOTREACHED();
    }
  }

  if (HasPendingPaymentQueries()) {
    return;
  }

  if (!database_helper_->GetServerDatabase()) {
    DLOG(WARNING) << "There are no pending queries but the server database "
                     "wasn't set yet, so some data might be missing. Maybe "
                     "SetSyncService() wasn't called yet.";
    return;
  }

  if (!is_payments_data_loaded_) {
    is_payments_data_loaded_ = true;
    pdm_->LogStoredPaymentsDataMetrics();
    pdm_->payments_data_cleaner_->CleanupPaymentsData();
  }

  pdm_->NotifyPersonalDataObserver();
}

void PaymentsDataManager::Refresh() {
  pdm_->LoadCreditCards();
  pdm_->LoadCreditCardCloudTokenData();
  pdm_->LoadIbans();
  pdm_->LoadPaymentsCustomerData();
  pdm_->LoadAutofillOffers();
  pdm_->LoadVirtualCardUsageData();
  pdm_->LoadCreditCardBenefits();
}

scoped_refptr<AutofillWebDataService> PaymentsDataManager::GetLocalDatabase() {
  return database_helper_->GetLocalDatabase();
}
scoped_refptr<AutofillWebDataService> PaymentsDataManager::GetServerDatabase() {
  return database_helper_->GetServerDatabase();
}
void PaymentsDataManager::SetUseAccountStorageForServerData(
    bool use_account_storage) {
  database_helper_->SetUseAccountStorageForServerData(use_account_storage);
}
bool PaymentsDataManager::IsUsingAccountStorageForServerData() {
  return database_helper_->IsUsingAccountStorageForServerData();
}

void PaymentsDataManager::CancelPendingServerQueries() {
  CancelPendingServerQuery(&pending_server_creditcards_query_);
  CancelPendingServerQuery(&pending_customer_data_query_);
  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);
  CancelPendingServerQuery(&pending_offer_data_query_);
  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);
  CancelPendingServerQuery(&pending_credit_card_benefit_query_);
}

void PaymentsDataManager::LoadCreditCards() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_creditcards_query_);
  CancelPendingServerQuery(&pending_server_creditcards_query_);

  pending_creditcards_query_ =
      database_helper_->GetLocalDatabase()->GetCreditCards(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_creditcards_query_ =
        database_helper_->GetServerDatabase()->GetServerCreditCards(this);
  }
}

void PaymentsDataManager::LoadCreditCardCloudTokenData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_server_creditcard_cloud_token_data_query_);

  pending_server_creditcard_cloud_token_data_query_ =
      database_helper_->GetServerDatabase()->GetCreditCardCloudTokenData(this);
}

void PaymentsDataManager::LoadIbans() {
  if (!database_helper_->GetLocalDatabase()) {
    NOTREACHED();
    return;
  }

  CancelPendingLocalQuery(&pending_local_ibans_query_);
  CancelPendingServerQuery(&pending_server_ibans_query_);

  pending_local_ibans_query_ =
      database_helper_->GetLocalDatabase()->GetLocalIbans(this);
  if (database_helper_->GetServerDatabase()) {
    pending_server_ibans_query_ =
        database_helper_->GetServerDatabase()->GetServerIbans(this);
  }
}

void PaymentsDataManager::LoadAutofillOffers() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_offer_data_query_);

  pending_offer_data_query_ =
      database_helper_->GetServerDatabase()->GetAutofillOffers(this);
}

void PaymentsDataManager::LoadVirtualCardUsageData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_virtual_card_usage_data_query_);

  pending_virtual_card_usage_data_query_ =
      database_helper_->GetServerDatabase()->GetVirtualCardUsageData(this);
}

void PaymentsDataManager::LoadCreditCardBenefits() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_credit_card_benefit_query_);

  pending_credit_card_benefit_query_ =
      database_helper_->GetServerDatabase()->GetCreditCardBenefits(this);
}

void PaymentsDataManager::CancelPendingLocalQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetLocalDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetLocalDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PaymentsDataManager::CancelPendingServerQuery(
    WebDataServiceBase::Handle* handle) {
  if (*handle) {
    if (!database_helper_->GetServerDatabase()) {
      NOTREACHED();
      return;
    }
    database_helper_->GetServerDatabase()->CancelRequest(*handle);
  }
  *handle = 0;
}

void PaymentsDataManager::LoadPaymentsCustomerData() {
  if (!database_helper_->GetServerDatabase()) {
    return;
  }

  CancelPendingServerQuery(&pending_customer_data_query_);

  pending_customer_data_query_ =
      database_helper_->GetServerDatabase()->GetPaymentsCustomerData(this);
}

bool PaymentsDataManager::HasPendingPaymentQueries() const {
  return pending_creditcards_query_ != 0 ||
         pending_server_creditcards_query_ != 0 ||
         pending_server_creditcard_cloud_token_data_query_ != 0 ||
         pending_customer_data_query_ != 0 || pending_offer_data_query_ != 0 ||
         pending_virtual_card_usage_data_query_ != 0 ||
         pending_credit_card_benefit_query_ != 0;
}

}  // namespace autofill
