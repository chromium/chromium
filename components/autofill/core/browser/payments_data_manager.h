// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/webdata/common/web_data_service_consumer.h"

namespace autofill {

class PaymentsDatabaseHelper;
class PersonalDataManager;
class TestPersonalDataManager;

class PaymentsDataManager : public WebDataServiceConsumer {
 public:
  PaymentsDataManager(scoped_refptr<AutofillWebDataService> profile_database,
                      scoped_refptr<AutofillWebDataService> account_database,
                      PersonalDataManager* pdm);

  PaymentsDataManager(const PaymentsDataManager&) = delete;
  PaymentsDataManager& operator=(const PaymentsDataManager&) = delete;
  ~PaymentsDataManager() override;

  // WebDataServiceConsumer:
  void OnWebDataServiceRequestDone(
      WebDataServiceBase::Handle h,
      std::unique_ptr<WDTypedResult> result) override;

  // TODO(b/322170538): Remove.
  scoped_refptr<AutofillWebDataService> GetLocalDatabase();
  scoped_refptr<AutofillWebDataService> GetServerDatabase();
  void SetUseAccountStorageForServerData(bool use_account_storage);
  bool IsUsingAccountStorageForServerData();

 protected:
  // TODO(b/322170538): Remove dependency.
  friend class PersonalDataManager;
  friend class TestPersonalDataManager;

  // Loads the saved credit cards from the web database.
  void LoadCreditCards();

  // Loads the saved credit card cloud token data from the web database.
  void LoadCreditCardCloudTokenData();

  // Loads the saved IBANs from the web database.
  void LoadIbans();

  // Loads the payments customer data from the web database.
  void LoadPaymentsCustomerData();

  // Loads the autofill offer data from the web database.
  void LoadAutofillOffers();

  // Loads the virtual card usage data from the web database.
  void LoadVirtualCardUsageData();

  // Loads the credit card benefits from the web database.
  void LoadCreditCardBenefits();

  // Cancels a pending query to the local web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingLocalQuery(WebDataServiceBase::Handle* handle);

  // Cancels a pending query to the server web database.  |handle| is a pointer
  // to the query handle.
  void CancelPendingServerQuery(WebDataServiceBase::Handle* handle);

  // Stores the PaymentsCustomerData obtained from the database.
  std::unique_ptr<PaymentsCustomerData> payments_customer_data_;

  // Cached versions of the local and server credit cards.
  std::vector<std::unique_ptr<CreditCard>> local_credit_cards_;
  std::vector<std::unique_ptr<CreditCard>> server_credit_cards_;

  // Cached versions of the local and server IBANs.
  std::vector<std::unique_ptr<Iban>> local_ibans_;
  std::vector<std::unique_ptr<Iban>> server_ibans_;

  // Cached version of the CreditCardCloudTokenData obtained from the database.
  std::vector<std::unique_ptr<CreditCardCloudTokenData>>
      server_credit_card_cloud_token_data_;

  // Autofill offer data, including card-linked offers for the user's credit
  // cards as well as promo code offers.
  std::vector<std::unique_ptr<AutofillOfferData>> autofill_offer_data_;

  // Virtual card usage data, which contains information regarding usages of a
  // virtual card related to a specific merchant website.
  std::vector<std::unique_ptr<VirtualCardUsageData>>
      autofill_virtual_card_usage_data_;

  // Cached version of the credit card benefits obtained from the database.
  // Including credit-card-linked flat rate benefits, category benefits and
  // merchant benefits that are available for users' online purchases.
  std::vector<CreditCardBenefit> credit_card_benefits_;

  // True if personal data has been loaded from the web database.
  bool is_payments_data_loaded_ = false;

 private:
  // Returns if there are any pending queries to the web database.
  bool HasPendingPaymentQueries() const;

  // Decides which database type to use for server and local cards.
  std::unique_ptr<PaymentsDatabaseHelper> database_helper_;

  // When the manager makes a request from WebDataServiceBase, the database
  // is queried on another sequence, we record the query handle until we
  // get called back.
  WebDataServiceBase::Handle pending_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcards_query_ = 0;
  WebDataServiceBase::Handle pending_server_creditcard_cloud_token_data_query_ =
      0;
  WebDataServiceBase::Handle pending_local_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_server_ibans_query_ = 0;
  WebDataServiceBase::Handle pending_customer_data_query_ = 0;
  WebDataServiceBase::Handle pending_offer_data_query_ = 0;
  WebDataServiceBase::Handle pending_virtual_card_usage_data_query_ = 0;
  WebDataServiceBase::Handle pending_credit_card_benefit_query_ = 0;

  // TODO(b/322170538): Remove dependency.
  raw_ptr<PersonalDataManager> pdm_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_DATA_MANAGER_H_
