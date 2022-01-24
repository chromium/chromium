// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/autofill_payment_app.h"

#include <algorithm>
#include <memory>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/payments/core/autofill_card_validation.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/features.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_request_base_delegate.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payments_experimental_features.h"

namespace payments {

AutofillPaymentApp::AutofillPaymentApp(
    const std::string& method_name,
    const autofill::CreditCard& card,
    const std::vector<autofill::AutofillProfile*>& billing_profiles,
    const std::string& app_locale,
    base::WeakPtr<PaymentRequestBaseDelegate> payment_request_delegate)
    : PaymentApp(autofill::data_util::GetPaymentRequestData(card.network())
                     .icon_resource_id,
                 PaymentApp::Type::AUTOFILL),
      method_name_(method_name),
      credit_card_(card),
      billing_profiles_(billing_profiles),
      app_locale_(app_locale),
      delegate_(nullptr),
      payment_request_delegate_(payment_request_delegate) {
  app_method_names_.insert(methods::kBasicCard);
}

AutofillPaymentApp::~AutofillPaymentApp() {}

void AutofillPaymentApp::InvokePaymentApp(base::WeakPtr<Delegate> delegate) {
  DCHECK(delegate);
  // There can be only one FullCardRequest going on at a time. If |delegate_| is
  // not null, there's already an active request, which shouldn't happen.
  // |delegate_| is reset to nullptr when the request succeeds or fails.
  DCHECK(!delegate_);
  delegate_ = delegate;

  // Get the billing address.
  if (!credit_card_.billing_address_id().empty()) {
    autofill::AutofillProfile* billing_address =
        autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
            credit_card_.billing_address_id(), billing_profiles_);
    if (billing_address)
      billing_address_ = *billing_address;
  }

  is_waiting_for_billing_address_normalization_ = true;
  is_waiting_for_card_unmask_ = true;

  if (payment_request_delegate_) {
    // Start the normalization of the billing address.
    payment_request_delegate_->GetAddressNormalizer()->NormalizeAddressAsync(
        billing_address_, /*timeout_seconds=*/5,
        base::BindOnce(&AutofillPaymentApp::OnAddressNormalized,
                       weak_ptr_factory_.GetWeakPtr()));

    payment_request_delegate_->DoFullCardRequest(
        credit_card_, weak_ptr_factory_.GetWeakPtr());
  }
}

bool AutofillPaymentApp::IsCompleteForPayment() const {
  // COMPLETE or EXPIRED cards are considered valid for payment. The user will
  // be prompted to enter the new expiration at the CVC step.
  return GetCompletionStatusForCard(credit_card_, app_locale_,
                                    billing_profiles_) <= CREDIT_CARD_EXPIRED;
}

uint32_t AutofillPaymentApp::GetCompletenessScore() const {
  return ::payments::GetCompletenessScore(credit_card_, app_locale_,
                                          billing_profiles_);
}

bool AutofillPaymentApp::CanPreselect() const {
  return IsCompleteForPayment();
}

std::u16string AutofillPaymentApp::GetMissingInfoLabel() const {
  return GetCompletionMessageForCard(
      GetCompletionStatusForCard(credit_card_, app_locale_, billing_profiles_));
}

bool AutofillPaymentApp::HasEnrolledInstrument() const {
  CreditCardCompletionStatus status =
      GetCompletionStatusForCard(credit_card_, app_locale_, billing_profiles_);
  // Card has to have a cardholder name and number for the purposes of
  // CanMakePayment. An expired card is still valid at this stage.
  return !(status & CREDIT_CARD_NO_CARDHOLDER ||
           status & CREDIT_CARD_NO_NUMBER);
}

void AutofillPaymentApp::RecordUse() {
  if (payment_request_delegate_) {
    // Record the use of the credit card.
    payment_request_delegate_->GetPersonalDataManager()->RecordUseOf(
        &credit_card_);
  }
}

bool AutofillPaymentApp::NeedsInstallation() const {
  // Autofill payment app is built-in, so it doesn't need installation.
  return false;
}

std::string AutofillPaymentApp::GetId() const {
  return credit_card_.guid();
}

std::u16string AutofillPaymentApp::GetLabel() const {
  return credit_card_.NetworkAndLastFourDigits();
}

std::u16string AutofillPaymentApp::GetSublabel() const {
  return credit_card_.GetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL), app_locale_);
}

bool AutofillPaymentApp::IsValidForModifier(
    const std::string& method,
    bool supported_networks_specified,
    const std::set<std::string>& supported_networks) const {
  bool is_valid = false;
  IsValidForPaymentMethodIdentifier(method, &is_valid);
  if (!is_valid)
    return false;

  if (supported_networks_specified) {
    std::string basic_card_network =
        autofill::data_util::GetPaymentRequestData(credit_card_.network())
            .basic_card_issuer_network;
    if (supported_networks.find(basic_card_network) == supported_networks.end())
      return false;
  }

  return true;
}

base::WeakPtr<PaymentApp> AutofillPaymentApp::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

bool AutofillPaymentApp::HandlesShippingAddress() const {
  return false;
}

bool AutofillPaymentApp::HandlesPayerName() const {
  return false;
}

bool AutofillPaymentApp::HandlesPayerEmail() const {
  return false;
}

bool AutofillPaymentApp::HandlesPayerPhone() const {
  return false;
}

void AutofillPaymentApp::OnFullCardRequestSucceeded(
    const autofill::payments::FullCardRequest& /* full_card_request */,
    const autofill::CreditCard& card,
    const std::u16string& cvc) {
  credit_card_ = card;
  cvc_ = cvc;
  is_waiting_for_card_unmask_ = false;

  if (!is_waiting_for_billing_address_normalization_)
    GenerateBasicCardResponse();
}

void AutofillPaymentApp::OnFullCardRequestFailed(
    autofill::payments::FullCardRequest::FailureType failure_type) {
  // The user may have cancelled the unmask or something has gone wrong (e.g.,
  // the network request failed). In all cases, reset the |delegate_| so another
  // request can start.
  delegate_ = nullptr;
}

void AutofillPaymentApp::RecordMissingFieldsForApp() const {
  CreditCardCompletionStatus completion_status =
      GetCompletionStatusForCard(credit_card_, app_locale_, billing_profiles_);
  if (completion_status == CREDIT_CARD_COMPLETE)
    return;

  // Record the missing fields from card completion status.
  base::UmaHistogramSparse("PaymentRequest.MissingPaymentFields",
                           completion_status);
}

void AutofillPaymentApp::GenerateBasicCardResponse() {
  DCHECK(!is_waiting_for_billing_address_normalization_);
  DCHECK(!is_waiting_for_card_unmask_);

  if (delegate_) {
    base::Value response_value =
        payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
            credit_card_, cvc_, billing_address_, app_locale_)
            ->ToValue();
    std::string stringified_details;
    base::JSONWriter::Write(response_value, &stringified_details);
    delegate_->OnInstrumentDetailsReady(method_name_, stringified_details,
                                        PayerData());
    delegate_ = nullptr;
  }

  cvc_ = u"";
}

void AutofillPaymentApp::OnAddressNormalized(
    bool success,
    const autofill::AutofillProfile& normalized_profile) {
  DCHECK(is_waiting_for_billing_address_normalization_);

  billing_address_ = normalized_profile;
  is_waiting_for_billing_address_normalization_ = false;

  if (!is_waiting_for_card_unmask_)
    GenerateBasicCardResponse();
}

}  // namespace payments
