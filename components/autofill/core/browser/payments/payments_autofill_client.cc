// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_autofill_client.h"

#include "build/buildflag.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"

#if !BUILDFLAG(IS_IOS)
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
