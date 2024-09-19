// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_data_cleaner.h"

#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/common/autofill_constants.h"

namespace autofill {

PaymentsDataCleaner::PaymentsDataCleaner(
    PaymentsDataManager* payments_data_manager)
    : payments_data_manager_(payments_data_manager) {}

PaymentsDataCleaner::~PaymentsDataCleaner() = default;

void PaymentsDataCleaner::CleanupPaymentsData() {
  DeleteDisusedCreditCards();
  ClearCreditCardNonSettingsOrigins();
}

void PaymentsDataCleaner::ClearCreditCardNonSettingsOrigins() {
  bool has_updated = false;

  for (CreditCard* card : payments_data_manager_->GetLocalCreditCards()) {
    if (card->origin() != kSettingsOrigin && !card->origin().empty()) {
      card->set_origin(std::string());
      payments_data_manager_->GetLocalDatabase()->UpdateCreditCard(*card);
      has_updated = true;
    }
  }

  // Refresh the local cache and send notifications to observers if a changed
  // was made.
  if (has_updated) {
    payments_data_manager_->Refresh();
  }
}

bool PaymentsDataCleaner::DeleteDisusedCreditCards() {
  // Only delete local cards, as server cards are managed by Payments.
  auto cards = payments_data_manager_->GetLocalCreditCards();

  // Early exit when there is no local cards.
  if (cards.empty()) {
    return true;
  }

  std::vector<CreditCard> cards_to_delete;
  for (const CreditCard* card : cards) {
    if (card->IsDeletable()) {
      cards_to_delete.push_back(*card);
    }
  }

  size_t num_deleted_cards = cards_to_delete.size();

  if (num_deleted_cards > 0) {
    payments_data_manager_->DeleteLocalCreditCards(cards_to_delete);
  }

  AutofillMetrics::LogNumberOfCreditCardsDeletedForDisuse(num_deleted_cards);

  return true;
}

}  // namespace autofill
