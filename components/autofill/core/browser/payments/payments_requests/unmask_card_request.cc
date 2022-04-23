// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {
namespace payments {

namespace {
const char kUnmaskCardRequestPath[] =
    "payments/apis-secure/creditcardservice/getrealpan?s7e_suffix=chromewallet";

const char kUnmaskCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";

const char kUnmaskCardRequestFormatWithCvc[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_13_cvc=%s";

const char kUnmaskCardRequestFormatWithOtp[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_263_otp=%s";

constexpr size_t kDefaultOtpLength = 6U;

CardUnmaskChallengeOption ParseCardUnmaskChallengeOption(
    const base::Value& challenge_option) {
  CardUnmaskChallengeOption card_unmask_challenge_option;

  // Check if it's SMS OTP challenge option.
  const base::Value* sms_challenge_option = challenge_option.FindKeyOfType(
      "sms_otp_challenge_option", base::Value::Type::DICTIONARY);
  if (sms_challenge_option) {
    card_unmask_challenge_option.type = CardUnmaskChallengeOptionType::kSmsOtp;
    const auto* challenge_id =
        sms_challenge_option->FindStringKey("challenge_id");
    DCHECK(challenge_id);
    card_unmask_challenge_option.id = *challenge_id;
    // For SMS OTP challenge, masked phone number is the challenge_info for
    // display.
    const auto* masked_phone_number =
        sms_challenge_option->FindStringKey("masked_phone_number");
    DCHECK(masked_phone_number);
    card_unmask_challenge_option.challenge_info =
        base::UTF8ToUTF16(*masked_phone_number);
    absl::optional<int> otp_length =
        sms_challenge_option->FindIntKey("otp_length");
    if (otp_length.has_value())
      card_unmask_challenge_option.otp_length = *otp_length;
    else
      card_unmask_challenge_option.otp_length = kDefaultOtpLength;
  }

  return card_unmask_challenge_option;
}
}  // namespace

UnmaskCardRequest::UnmaskCardRequest(
    const PaymentsClient::UnmaskRequestDetails& request_details,
    const bool full_sync_enabled,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskResponseDetails&)> callback)
    : request_details_(request_details),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {
  DCHECK_NE(CreditCard::LOCAL_CARD, request_details.card.record_type());
}

UnmaskCardRequest::~UnmaskCardRequest() = default;

std::string UnmaskCardRequest::GetRequestUrlPath() {
  return kUnmaskCardRequestPath;
}

std::string UnmaskCardRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string UnmaskCardRequest::GetRequestContent() {
  // Either non-legacy instrument id or legacy server id must be provided.
  DCHECK(!request_details_.card.server_id().empty() ||
         request_details_.card.instrument_id() != 0);
  base::Value request_dict(base::Value::Type::DICTIONARY);
  if (!request_details_.card.server_id().empty()) {
    request_dict.SetKey("credit_card_id",
                        base::Value(request_details_.card.server_id()));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillEnableUnmaskCardRequestSetInstrumentId) &&
      request_details_.card.instrument_id() != 0) {
    request_dict.SetKey("instrument_id",
                        base::Value(base::NumberToString(
                            request_details_.card.instrument_id())));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillAlwaysReturnCloudTokenizedCard)) {
    // See b/140727361.
    request_dict.SetKey("instrument_token",
                        base::Value("INSTRUMENT_TOKEN_FOR_TEST"));
  }
  request_dict.SetKey("risk_data_encoded",
                      BuildRiskDictionary(request_details_.risk_data));
  base::Value context(base::Value::Type::DICTIONARY);
  context.SetKey("billable_service",
                 base::Value(kUnmaskCardBillableServiceNumber));
  if (request_details_.billing_customer_number != 0) {
    context.SetKey("customer_context",
                   BuildCustomerContextDictionary(
                       request_details_.billing_customer_number));
  }
  request_dict.SetKey("context", std::move(context));

  base::Value chrome_user_context(base::Value::Type::DICTIONARY);
  chrome_user_context.SetKey("full_sync_enabled",
                             base::Value(full_sync_enabled_));
  request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

  if (!request_details_.context_token.empty()) {
    request_dict.SetKey("context_token",
                        base::Value(request_details_.context_token));
  }

  int value = 0;
  if (base::StringToInt(request_details_.user_response.exp_month, &value))
    request_dict.SetKey("expiration_month", base::Value(value));
  if (base::StringToInt(request_details_.user_response.exp_year, &value))
    request_dict.SetKey("expiration_year", base::Value(value));

  request_dict.SetKey(
      "opt_in_fido_auth",
      base::Value(request_details_.user_response.enable_fido_auth));

  bool is_cvc_auth = !request_details_.user_response.cvc.empty();
  bool is_otp_auth = !request_details_.otp.empty();
  bool is_fido_auth = request_details_.fido_assertion_info.has_value();

  // At most, only one of these auth methods can be provided.
  DCHECK_LE(is_cvc_auth + is_fido_auth + is_otp_auth, 1);
  if (is_cvc_auth) {
    request_dict.SetKey("encrypted_cvc", base::Value("__param:s7e_13_cvc"));
  } else if (is_otp_auth) {
    request_dict.SetKey("otp", base::Value("__param:s7e_263_otp"));
  } else if (is_fido_auth) {
    request_dict.SetKey(
        "fido_assertion_info",
        std::move(request_details_.fido_assertion_info.value()));
  }

  if (request_details_.last_committed_url_origin.has_value()) {
    base::Value virtual_card_request_info(base::Value::Type::DICTIONARY);
    virtual_card_request_info.SetKey(
        "merchant_domain",
        base::Value(request_details_.last_committed_url_origin.value().spec()));
    request_dict.SetKey("virtual_card_request_info",
                        std::move(virtual_card_request_info));
  }

  std::string json_request;
  base::JSONWriter::Write(request_dict, &json_request);
  std::string request_content;
  if (is_cvc_auth) {
    request_content = base::StringPrintf(
        kUnmaskCardRequestFormatWithCvc,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(
            base::UTF16ToASCII(request_details_.user_response.cvc), true)
            .c_str());
  } else if (is_otp_auth) {
    request_content = base::StringPrintf(
        kUnmaskCardRequestFormatWithOtp,
        base::EscapeUrlEncodedData(json_request, true).c_str(),
        base::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.otp),
                                   true)
            .c_str());
  } else {
    // If neither cvc nor otp request, use the normal request format.
    request_content = base::StringPrintf(
        kUnmaskCardRequestFormat,
        base::EscapeUrlEncodedData(json_request, true).c_str());
  }

  VLOG(3) << "getrealpan request body: " << request_content;
  return request_content;
}

void UnmaskCardRequest::ParseResponse(const base::Value& response) {
  const std::string* pan = response.FindStringKey("pan");
  response_details_.real_pan = pan ? *pan : std::string();

  const std::string* dcvv = response.FindStringKey("dcvv");
  response_details_.dcvv = dcvv ? *dcvv : std::string();

  const base::Value* expiration =
      response.FindKeyOfType("expiration", base::Value::Type::DICTIONARY);
  if (expiration) {
    if (absl::optional<int> month = expiration->FindIntKey("month")) {
      response_details_.expiration_month = base::NumberToString(month.value());
    }

    if (absl::optional<int> year = expiration->FindIntKey("year"))
      response_details_.expiration_year = base::NumberToString(year.value());
  }

  // TODO(crbug.com/1248268): Clean up unused fido_creation_options.
  const base::Value* creation_options = response.FindKeyOfType(
      "fido_creation_options", base::Value::Type::DICTIONARY);
  if (creation_options)
    response_details_.fido_creation_options = creation_options->Clone();

  const base::Value* request_options = response.FindKeyOfType(
      "fido_request_options", base::Value::Type::DICTIONARY);
  if (request_options)
    response_details_.fido_request_options = request_options->Clone();

  const base::Value* challenge_option_list =
      response.FindKeyOfType("idv_challenge_options", base::Value::Type::LIST);
  if (challenge_option_list) {
    std::vector<CardUnmaskChallengeOption> card_unmask_challenge_options;
    for (const base::Value& challenge_option :
         challenge_option_list->GetListDeprecated()) {
      CardUnmaskChallengeOption parsed_challenge_option =
          ParseCardUnmaskChallengeOption(challenge_option);
      // Only return successfully parsed challenge option.
      if (parsed_challenge_option.type !=
          CardUnmaskChallengeOptionType::kUnknownType) {
        card_unmask_challenge_options.push_back(parsed_challenge_option);
      }
    }
    response_details_.card_unmask_challenge_options =
        card_unmask_challenge_options;
  }

  const std::string* card_authorization_token =
      response.FindStringKey("card_authorization_token");
  response_details_.card_authorization_token =
      card_authorization_token ? *card_authorization_token : std::string();

  const std::string* context_token = response.FindStringKey("context_token");
  response_details_.context_token =
      context_token ? *context_token : std::string();

  const std::string* flow_status = response.FindStringKey("flow_status");
  response_details_.flow_status = flow_status ? *flow_status : std::string();

  if (request_details_.card.record_type() == CreditCard::VIRTUAL_CARD) {
    response_details_.card_type =
        AutofillClient::PaymentsRpcCardType::kVirtualCard;
  } else if (request_details_.card.record_type() ==
             CreditCard::MASKED_SERVER_CARD) {
    response_details_.card_type =
        AutofillClient::PaymentsRpcCardType::kServerCard;
  } else {
    NOTREACHED();
  }
}

bool UnmaskCardRequest::IsResponseComplete() {
  switch (response_details_.card_type) {
    case AutofillClient::PaymentsRpcCardType::kUnknown:
      return false;
    case AutofillClient::PaymentsRpcCardType::kServerCard:
      return !response_details_.real_pan.empty();
    case AutofillClient::PaymentsRpcCardType::kVirtualCard:
      // When pan is returned, it has to contain pan + expiry + cvv.
      // When pan is not returned, it has to contain context token to indicate
      // success.
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableVirtualCardsRiskBasedAuthentication)) {
        return IsAllCardInformationValidIncludingDcvv() ||
               CanPerformVirtualCardAuth();
      }
      return IsAllCardInformationValidIncludingDcvv();
  }
}

void UnmaskCardRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

bool UnmaskCardRequest::IsAllCardInformationValidIncludingDcvv() {
  return !response_details_.real_pan.empty() &&
         !response_details_.expiration_month.empty() &&
         !response_details_.expiration_year.empty() &&
         !response_details_.dcvv.empty();
}

bool UnmaskCardRequest::CanPerformVirtualCardAuth() {
  return !response_details_.context_token.empty() &&
         (response_details_.fido_request_options.has_value() ||
          !response_details_.card_unmask_challenge_options.empty() ||
          !response_details_.flow_status.empty());
}

}  // namespace payments
}  // namespace autofill
