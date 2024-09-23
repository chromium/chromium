// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/upload_card_request.h"

#include <string>
#include <string_view>

#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill::payments {

namespace {
const char kUploadCardRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/savecard"
    "?s7e_suffix=chromewallet";
const char kUploadCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_21_pan=%s&s7e_13_cvc=%s";
const char kUploadCardRequestFormatWithoutCvc[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_21_pan=%s";
}  // namespace

UploadCardRequest::UploadCardRequest(
    const PaymentsNetworkInterface::UploadCardRequestDetails& request_details,
    const bool full_sync_enabled,
    base::OnceCallback<void(
        PaymentsAutofillClient::PaymentsRpcResult,
        const PaymentsNetworkInterface::UploadCardResponseDetails&)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

UploadCardRequest::~UploadCardRequest() = default;

std::string UploadCardRequest::GetRequestUrlPath() {
  return kUploadCardRequestPath;
}

std::string UploadCardRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UploadCardRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  request_dict.Set("pan", "__param:s7e_21_pan");
  if (!request_details_.cvc.empty())
    request_dict.Set("encrypted_cvc", "__param:s7e_13_cvc");
  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));

  const std::string& app_locale = request_details_.app_locale;
  base::Value::Dict context;
  context.Set("language_code", app_locale);
  context.Set("billable_service", kUploadPaymentMethodBillableServiceNumber);
  if (request_details_.billing_customer_number != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(
                    request_details_.billing_customer_number));
  }
  request_dict.Set("context", std::move(context));

  request_dict.Set(
      "chrome_user_context",
      BuildChromeUserContext(request_details_.client_behavior_signals,
                             full_sync_enabled_));
  SetStringIfNotEmpty(request_details_.card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", request_dict);

  base::Value::List addresses;
  for (const AutofillProfile& profile : request_details_.profiles) {
    addresses.Append(BuildAddressDictionary(profile, app_locale, true));
  }
  request_dict.Set("address", std::move(addresses));

  request_dict.Set("context_token", request_details_.context_token);

  int value = 0;
  const std::u16string exp_month =
      request_details_.card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale);
  const std::u16string exp_year =
      request_details_.card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, app_locale);
  if (base::StringToInt(exp_month, &value))
    request_dict.Set("expiration_month", value);
  if (base::StringToInt(exp_year, &value))
    request_dict.Set("expiration_year", value);

  if (request_details_.card.HasNonEmptyValidNickname()) {
    request_dict.Set("nickname", request_details_.card.nickname());
  }

  const std::u16string pan =
      request_details_.card.GetInfo(CREDIT_CARD_NUMBER, app_locale);
  std::string json_request;
  base::JSONWriter::Write(request_dict, &json_request);
  std::string request_content;
  if (request_details_.cvc.empty()) {
    request_content = base::StringPrintf(
        kUploadCardRequestFormatWithoutCvc,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str());
  } else {
    request_content = base::StringPrintf(
        kUploadCardRequestFormat,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.cvc),
                                   true)
            .c_str());
  }
  VLOG(3) << "savecard request body: " << request_content;
  return request_content;
}

void UploadCardRequest::ParseResponse(const base::Value::Dict& response) {
  const std::string* response_instrument_id =
      response.FindString("instrument_id");
  if (response_instrument_id) {
    int64_t instrument_id;
    if (base::StringToInt64(std::string_view(*response_instrument_id),
                            &instrument_id)) {
      upload_card_response_details_.instrument_id = instrument_id;
    }
  }

  const std::string* card_art_url = response.FindString("card_art_url");
  upload_card_response_details_.card_art_url =
      card_art_url ? GURL(*card_art_url) : GURL();

  const auto* virtual_card_metadata =
      response.FindDict("virtual_card_metadata");
  if (virtual_card_metadata) {
    const std::string* virtual_card_enrollment_status =
        virtual_card_metadata->FindString("status");
    if (virtual_card_enrollment_status) {
      if (*virtual_card_enrollment_status == "ENROLLED") {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::kEnrolled;
      } else if (*virtual_card_enrollment_status == "ENROLLMENT_ELIGIBLE") {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible;
      } else {
        upload_card_response_details_.virtual_card_enrollment_state =
            CreditCard::VirtualCardEnrollmentState::kUnenrolledAndNotEligible;
      }
    }

    if (upload_card_response_details_.virtual_card_enrollment_state ==
        CreditCard::VirtualCardEnrollmentState::kUnenrolledAndEligible) {
      const auto* virtual_card_enrollment_data =
          virtual_card_metadata->FindDict("virtual_card_enrollment_data");
      if (virtual_card_enrollment_data) {
        PaymentsNetworkInterface::GetDetailsForEnrollmentResponseDetails
            get_details_for_enrollment_response_details;
        const base::Value::Dict* google_legal_message =
            virtual_card_enrollment_data->FindDict("google_legal_message");
        if (google_legal_message) {
          LegalMessageLine::Parse(
              *google_legal_message,
              &get_details_for_enrollment_response_details.google_legal_message,
              /*escape_apostrophes=*/true);
        }

        const base::Value::Dict* external_legal_message =
            virtual_card_enrollment_data->FindDict("external_legal_message");
        if (external_legal_message) {
          LegalMessageLine::Parse(
              *external_legal_message,
              &get_details_for_enrollment_response_details.issuer_legal_message,
              /*escape_apostrophes=*/true);
        }

        const auto* context_token =
            virtual_card_enrollment_data->FindString("context_token");
        get_details_for_enrollment_response_details.vcn_context_token =
            context_token ? *context_token : std::string();

        upload_card_response_details_
            .get_details_for_enrollment_response_details =
            get_details_for_enrollment_response_details;
      }
    }
  }
}

bool UploadCardRequest::IsResponseComplete() {
  return true;
}

void UploadCardRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, upload_card_response_details_);
}

std::string UploadCardRequest::GetHistogramName() const {
  return "UploadCardRequest";
}

std::optional<base::TimeDelta> UploadCardRequest::GetTimeout() const {
  if (!base::FeatureList::IsEnabled(
          features::kAutofillUploadCardRequestTimeout)) {
    return std::nullopt;
  }
  return base::Milliseconds(
      features::kAutofillUploadCardRequestTimeoutMilliseconds.Get());
}

}  // namespace autofill::payments
