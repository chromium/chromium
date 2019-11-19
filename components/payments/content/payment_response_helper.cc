// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_response_helper.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/geo/phone_number_i18n.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/core/method_strings.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_request_delegate.h"

namespace payments {

PaymentResponseHelper::PaymentResponseHelper(
    const std::string& app_locale,
    PaymentRequestSpec* spec,
    PaymentApp* selected_app,
    PaymentRequestDelegate* payment_request_delegate,
    autofill::AutofillProfile* selected_shipping_profile,
    autofill::AutofillProfile* selected_contact_profile,
    Delegate* delegate)
    : app_locale_(app_locale),
      is_waiting_for_shipping_address_normalization_(false),
      is_waiting_for_instrument_details_(false),
      spec_(spec),
      delegate_(delegate),
      selected_app_(selected_app),
      payment_request_delegate_(payment_request_delegate),
      selected_contact_profile_(selected_contact_profile) {
  DCHECK(spec_);
  DCHECK(selected_app_);
  DCHECK(delegate_);

  is_waiting_for_instrument_details_ = true;

  // Start to normalize the shipping address, if necessary.
  if (spec_->request_shipping() && !selected_app_->HandlesShippingAddress()) {
    DCHECK(selected_shipping_profile);
    DCHECK(spec_->selected_shipping_option());

    is_waiting_for_shipping_address_normalization_ = true;

    payment_request_delegate_->GetAddressNormalizer()->NormalizeAddressAsync(
        *selected_shipping_profile,
        /*timeout_seconds=*/5,
        base::BindOnce(&PaymentResponseHelper::OnAddressNormalized,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Start to get the instrument details. Will call back into
  // OnInstrumentDetailsReady.
  selected_app_->InvokePaymentApp(this);
}

PaymentResponseHelper::~PaymentResponseHelper() {}

void PaymentResponseHelper::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details,
    const PayerData& payer_data) {
  if (!is_waiting_for_instrument_details_)
    return;

  method_name_ = method_name;
  stringified_details_ = stringified_details;
  payer_data_from_app_.payer_name = payer_data.payer_name;
  payer_data_from_app_.payer_email = payer_data.payer_email;
  payer_data_from_app_.payer_phone = payer_data.payer_phone;
  payer_data_from_app_.shipping_address = payer_data.shipping_address.Clone();
  payer_data_from_app_.selected_shipping_option_id =
      payer_data.selected_shipping_option_id;
  is_waiting_for_instrument_details_ = false;

  if (!is_waiting_for_shipping_address_normalization_)
    GeneratePaymentResponse();
}

void PaymentResponseHelper::OnInstrumentDetailsError(
    const std::string& error_message) {
  if (!is_waiting_for_instrument_details_)
    return;

  is_waiting_for_instrument_details_ = false;
  is_waiting_for_shipping_address_normalization_ = false;
  delegate_->OnPaymentResponseError(error_message);
}

void PaymentResponseHelper::OnAddressNormalized(
    bool success,
    const autofill::AutofillProfile& normalized_profile) {
  if (!is_waiting_for_shipping_address_normalization_)
    return;

  shipping_address_ = normalized_profile;
  is_waiting_for_shipping_address_normalization_ = false;

  if (!is_waiting_for_instrument_details_)
    GeneratePaymentResponse();
}

mojom::PayerDetailPtr PaymentResponseHelper::GeneratePayerDetail(
    const autofill::AutofillProfile* selected_contact_profile) const {
  mojom::PayerDetailPtr payer = mojom::PayerDetail::New();

  if (spec_->request_payer_name()) {
    if (selected_app_->HandlesPayerName()) {
      payer->name = payer_data_from_app_.payer_name;
    } else {
      DCHECK(selected_contact_profile);
      payer->name = base::UTF16ToUTF8(
          selected_contact_profile->GetInfo(autofill::NAME_FULL, app_locale_));
    }
  }
  if (spec_->request_payer_email()) {
    if (selected_app_->HandlesPayerEmail()) {
      payer->email = payer_data_from_app_.payer_email;
    } else {
      DCHECK(selected_contact_profile);
      payer->email = base::UTF16ToUTF8(
          selected_contact_profile->GetRawInfo(autofill::EMAIL_ADDRESS));
    }
  }
  if (spec_->request_payer_phone()) {
    if (selected_app_->HandlesPayerPhone()) {
      payer->phone = payer_data_from_app_.payer_phone;
    } else {
      DCHECK(selected_contact_profile);

      // Try to format the phone number to the E.164 format to send in the
      // Payment Response, as defined in the Payment Request spec. If it's not
      // possible, send the original. More info at:
      // https://w3c.github.io/payment-request/#paymentrequest-updated-algorithm
      const std::string original_number =
          base::UTF16ToUTF8(selected_contact_profile->GetInfo(
              autofill::PHONE_HOME_WHOLE_NUMBER, app_locale_));

      const std::string default_region_code =
          autofill::AutofillCountry::CountryCodeForLocale(app_locale_);
      payer->phone = autofill::i18n::FormatPhoneForResponse(
          original_number, default_region_code);
    }
  }

  return payer;
}

void PaymentResponseHelper::GeneratePaymentResponse() {
  DCHECK(!is_waiting_for_instrument_details_);
  DCHECK(!is_waiting_for_shipping_address_normalization_);

  mojom::PaymentResponsePtr payment_response = mojom::PaymentResponse::New();

  // Make sure that we return the method name that the merchant specified for
  // this app: cards can be either specified through their name (e.g., "visa")
  // or through basic-card's supportedNetworks.
  payment_response->method_name =
      spec_->IsMethodSupportedThroughBasicCard(method_name_)
          ? methods::kBasicCard
          : method_name_;
  payment_response->stringified_details = stringified_details_;

  // Shipping Address section
  if (spec_->request_shipping()) {
    if (selected_app_->HandlesShippingAddress()) {
      payment_response->shipping_address =
          std::move(payer_data_from_app_.shipping_address);
      payment_response->shipping_option =
          payer_data_from_app_.selected_shipping_option_id;
    } else {
      payment_response->shipping_address =
          data_util::GetPaymentAddressFromAutofillProfile(shipping_address_,
                                                          app_locale_);
      payment_response->shipping_option = spec_->selected_shipping_option()->id;
    }
  }

  // Contact Details section.
  payment_response->payer = GeneratePayerDetail(selected_contact_profile_);

  delegate_->OnPaymentResponseReady(std::move(payment_response));
}

}  // namespace payments
