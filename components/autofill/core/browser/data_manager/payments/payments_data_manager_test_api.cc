// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_manager/payments/payments_data_manager_test_api.h"

namespace autofill {

void PaymentsDataManagerTestApi::AddServerCreditCard(
    const CreditCard& credit_card) {
  CHECK_EQ(CreditCard::RecordType::kMaskedServerCard,
           credit_card.record_type());
  CHECK(!credit_card.IsEmpty(payments_data_manager_->app_locale_));
  CHECK(!credit_card.server_id().empty());
  CHECK(payments_data_manager_->GetServerDatabase())
      << "Adding server card without server storage.";

  // Don't add a duplicate.
  // TODO(crbug.com/332562243): Tests shouldn't be adding duplicate cards this
  // way, so convert this logic to a CHECK using exact equality or just remove.
  if (std::ranges::any_of(payments_data_manager_->server_credit_cards_,
                          [&](const auto& element) {
                            return element->guid() == credit_card.guid();
                          }) ||
      std::ranges::any_of(payments_data_manager_->server_credit_cards_,
                          [&](const auto& element) {
                            return element->Compare(credit_card) == 0;
                          })) {
    return;
  }

  // Add the new credit card to the web database.
  payments_data_manager_->GetServerDatabase()->AddServerCreditCardForTesting(
      credit_card);

  // Refresh our local cache and send notifications to observers.
  payments_data_manager_->Refresh();
}

void PaymentsDataManagerTestApi::AddOfferData(
    std::unique_ptr<AutofillOfferData> offer_data) {
  payments_data_manager_->autofill_offer_data_.push_back(std::move(offer_data));
}

void PaymentsDataManagerTestApi::AddBnplIssuer(const BnplIssuer& bnpl_issuer) {
  std::vector<BnplIssuer>& linked_issuers =
      payments_data_manager_->linked_bnpl_issuers_;
  std::vector<BnplIssuer>& unlinked_issuers =
      payments_data_manager_->unlinked_bnpl_issuers_;

  // No duplicated issuer should be inserted into the BNPL issuer list.
  CHECK(std::ranges::none_of(
      linked_issuers, [&](const BnplIssuer& saved_bnpl_issuer) {
        return saved_bnpl_issuer.issuer_id() == bnpl_issuer.issuer_id();
      }));
  CHECK(std::ranges::none_of(
      unlinked_issuers, [&](const BnplIssuer& saved_bnpl_issuer) {
        return saved_bnpl_issuer.issuer_id() == bnpl_issuer.issuer_id();
      }));

  if (bnpl_issuer.payment_instrument().has_value()) {
    linked_issuers.push_back(bnpl_issuer);
  } else {
    unlinked_issuers.push_back(bnpl_issuer);
  }
}

bool PaymentsDataManagerTestApi::ShouldBlockCardBenefitSuggestionLabels(
    const CreditCard& credit_card,
    const url::Origin& origin,
    const AutofillOptimizationGuide* optimization_guide) {
  return payments_data_manager_->ShouldBlockCardBenefitSuggestionLabels(
      credit_card, origin, std::move(optimization_guide));
}

bool PaymentsDataManagerTestApi::ShouldSuggestServerPaymentMethods() {
  return payments_data_manager_->ShouldSuggestServerPaymentMethods();
}

}  // namespace autofill
