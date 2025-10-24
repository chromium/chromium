// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include <optional>
#include <vector>

#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/card_unmask_delegate.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/single_field_fillers/payments/merchant_promo_code_manager.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/ui/payments/bubble_show_options.h"
#include "components/autofill/core/browser/ui/payments/card_unmask_prompt_options.h"

#if !BUILDFLAG(IS_IOS)
#include "components/webauthn/core/browser/internal_authenticator.h"
#endif  // !BUILDFLAG(IS_IOS)

namespace autofill::payments {

PaymentsAutofillClient::~PaymentsAutofillClient() = default;

PaymentsAutofillClient::UserProvidedCardDetails::UserProvidedCardDetails() =
    default;

PaymentsAutofillClient::UserProvidedCardDetails::UserProvidedCardDetails(
    const UserProvidedCardDetails&) = default;

PaymentsAutofillClient::UserProvidedCardDetails&
PaymentsAutofillClient::UserProvidedCardDetails::operator=(
    const UserProvidedCardDetails&) = default;

PaymentsAutofillClient::UserProvidedCardDetails::UserProvidedCardDetails(
    UserProvidedCardDetails&&) = default;

PaymentsAutofillClient::UserProvidedCardDetails&
PaymentsAutofillClient::UserProvidedCardDetails::operator=(
    UserProvidedCardDetails&&) = default;

PaymentsAutofillClient::UserProvidedCardDetails::~UserProvidedCardDetails() =
    default;

PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails::
    UserProvidedCardSaveAndFillDetails() = default;

PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails::
    UserProvidedCardSaveAndFillDetails(
        const UserProvidedCardSaveAndFillDetails&) = default;

PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails&
PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails::operator=(
    const UserProvidedCardSaveAndFillDetails&) = default;

PaymentsAutofillClient::UserProvidedCardSaveAndFillDetails::
    ~UserProvidedCardSaveAndFillDetails() = default;

const AutofillOfferManager* PaymentsAutofillClient::GetAutofillOfferManager()
    const {
  return const_cast<PaymentsAutofillClient*>(this)->GetAutofillOfferManager();
}

const PaymentsDataManager& PaymentsAutofillClient::GetPaymentsDataManager()
    const {
  return const_cast<PaymentsAutofillClient*>(this)->GetPaymentsDataManager();
}

const payments::SaveAndFillManager*
PaymentsAutofillClient::GetSaveAndFillManager() const {
  return const_cast<PaymentsAutofillClient*>(this)->GetSaveAndFillManager();
}

}  // namespace autofill::payments
