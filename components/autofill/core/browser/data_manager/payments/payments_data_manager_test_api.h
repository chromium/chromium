// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_API_H_

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"

namespace autofill {

// Provides testing functions for `PaymentsDataManager`.
class PaymentsDataManagerTestApi {
 public:
  explicit PaymentsDataManagerTestApi(
      PaymentsDataManager& payments_data_manager)
      : payments_data_manager_(payments_data_manager) {}

  void NotifyObservers() { payments_data_manager_->NotifyObservers(); }

  // Adds `credit_card` to the web database as a server card.
  //
  // In production code, server cards are set via the Chrome Sync process, and
  // not via PaymentsDataManager. However integration tests and unittests of
  // PersonalDataManager/PaymentsDataManager may need to directly add cards.
  void AddServerCreditCard(const CreditCard& credit_card);

  // Adds the offer data to local cache. This does not affect data in the
  // database.
  void AddOfferData(std::unique_ptr<AutofillOfferData> offer_data);

  // Adds 'bnpl_issuer' to the local cache of linked/unlinked buy-now-pay-later
  // issuers based on the value of the issuer.
  void AddBnplIssuer(const BnplIssuer& bnpl_issuer);

  // Returns the number of credit card benefits.
  size_t GetCreditCardBenefitsCount() {
    return payments_data_manager_->credit_card_benefits_.size();
  }

  // Returns whether Autofill card benefit suggestion labels should be blocked.
  bool ShouldBlockCardBenefitSuggestionLabels();

  void SetImageFetcher(AutofillImageFetcherBase* image_fetcher) {
    payments_data_manager_->image_fetcher_ = image_fetcher;
  }

  void SetAutofillOptimizationGuideDecider(
      AutofillOptimizationGuideDecider* autofill_optimization_guide_decider) {
    payments_data_manager_->autofill_optimization_guide_decider_ =
        autofill_optimization_guide_decider;
  }

  bool ShouldSuggestServerPaymentMethods();

  bool AreBnplIssuersSupported() const {
    return payments_data_manager_->AreBnplIssuersSupported();
  }

 private:
  const raw_ref<PaymentsDataManager> payments_data_manager_;
};

inline PaymentsDataManagerTestApi test_api(
    PaymentsDataManager& payments_data_manager) {
  return PaymentsDataManagerTestApi(payments_data_manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_DATA_MANAGER_PAYMENTS_PAYMENTS_DATA_MANAGER_TEST_API_H_
