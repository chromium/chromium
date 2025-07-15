// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

namespace {

using CardSaveAndFillDialogUserDecision =
    PaymentsAutofillClient::CardSaveAndFillDialogUserDecision;
using UserProvidedCardSaveAndFillDetails =
    PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails;

}  // namespace

SaveAndFillManagerImpl::SaveAndFillManagerImpl(
    PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

SaveAndFillManagerImpl::~SaveAndFillManagerImpl() = default;

void SaveAndFillManagerImpl::OnDidAcceptCreditCardSaveAndFillSuggestion() {
  // TODO(crbug.com/378164165): Attempt to offer upload Save and Fill first and
  // fall back to the local version.
  OfferLocalSaveAndFill();
}

void SaveAndFillManagerImpl::OfferLocalSaveAndFill() {
  payments_autofill_client_->ShowCreditCardLocalSaveAndFillDialog(
      base::BindOnce(&SaveAndFillManagerImpl::OnUserDidDecideOnLocalSave,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SaveAndFillManagerImpl::OnUserDidDecideOnLocalSave(
    CardSaveAndFillDialogUserDecision user_decision,
    const UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  autofill::CreditCard card_save_candidate;
  switch (user_decision) {
    case CardSaveAndFillDialogUserDecision::kAccepted:
      PopulateCreditCardInfo(card_save_candidate,
                             user_provided_card_save_and_fill_details);
      // Clear the CVC value from the `card_save_candidate` if CVC storage
      // isn't enabled.
      if (!card_save_candidate.cvc().empty() &&
          !payments_autofill_client_->GetPaymentsDataManager()
               .IsPaymentCvcStorageEnabled()) {
        card_save_candidate.clear_cvc();
      }
      payments_autofill_client_->GetPaymentsDataManager()
          .OnAcceptedLocalCreditCardSave(card_save_candidate);
      break;
    case CardSaveAndFillDialogUserDecision::kDeclined:
      break;
  }
}

void SaveAndFillManagerImpl::PopulateCreditCardInfo(
    autofill::CreditCard& card,
    const UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  const std::string app_locale =
      payments_autofill_client_->GetPaymentsDataManager().app_locale();

  card.SetInfo(CREDIT_CARD_NUMBER,
               user_provided_card_save_and_fill_details.card_number,
               app_locale);
  card.SetInfo(CREDIT_CARD_NAME_FULL,
               user_provided_card_save_and_fill_details.cardholder_name,
               app_locale);
  card.SetInfo(
      CREDIT_CARD_VERIFICATION_CODE,
      user_provided_card_save_and_fill_details.security_code.value_or(u""),
      app_locale);
  card.SetInfo(CREDIT_CARD_EXP_MONTH,
               user_provided_card_save_and_fill_details.expiration_date_month,
               app_locale);
  card.SetInfo(CREDIT_CARD_EXP_2_DIGIT_YEAR,
               user_provided_card_save_and_fill_details.expiration_date_year,
               app_locale);
}

}  // namespace autofill::payments
