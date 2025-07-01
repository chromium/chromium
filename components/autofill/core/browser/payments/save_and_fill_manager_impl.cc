// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "components/autofill/core/browser/payments/save_and_fill_manager_impl.h"

#include "base/check_deref.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"

namespace autofill::payments {

namespace {

using CardSaveAndFillDialogUserDecision =
    PaymentsAutofillClient::CardSaveAndFillDialogUserDecision;

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
    const PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails&
        user_provided_card_save_and_fill_details) {
  switch (user_decision) {
    case CardSaveAndFillDialogUserDecision::kAccepted:
    // TODO(crbug.com/378164516): Process user input and save the credit card
    // locally.
    case CardSaveAndFillDialogUserDecision::kDeclined:
      break;
  }
}

}  // namespace autofill::payments
