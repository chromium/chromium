// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments_data_manager_test_api.h"

#include "components/autofill/core/browser/data_model/credit_card_art_image.h"

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

void PaymentsDataManagerTestApi::OnCardArtImagesFetched(
    std::vector<std::unique_ptr<CreditCardArtImage>> images) {
  payments_data_manager_->OnCardArtImagesFetched(std::move(images));
}

bool PaymentsDataManagerTestApi::ShouldSuggestServerPaymentMethods() {
  return payments_data_manager_->ShouldSuggestServerPaymentMethods();
}

}  // namespace autofill
