// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

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
constexpr size_t kDefaultCvcLength = 3U;

// Parses the `defined_challenge_option` as an  OTP challenge option, and sets
// the appropriate fields in `parsed_challenge_option`.
void ParseAsOtpChallengeOption(
    const base::Value::Dict* defined_challenge_option,
    CardUnmaskChallengeOption* parsed_challenge_option,
    CardUnmaskChallengeOptionType otp_challenge_option_type) {
  parsed_challenge_option->type = otp_challenge_option_type;
  const auto* challenge_id =
      defined_challenge_option->FindString("challenge_id");
  DCHECK(challenge_id);
  parsed_challenge_option->id =
      CardUnmaskChallengeOption::ChallengeOptionId(*challenge_id);

  const std::string* challenge_info;
  if (otp_challenge_option_type == CardUnmaskChallengeOptionType::kSmsOtp) {
    // For SMS OTP challenge, masked phone number is the `challenge_info` for
    // display.
    challenge_info =
        defined_challenge_option->FindString("masked_phone_number");
  } else {
    CHECK_EQ(otp_challenge_option_type,
             CardUnmaskChallengeOptionType::kEmailOtp);
    challenge_info =
        defined_challenge_option->FindString("masked_email_address");
  }
  DCHECK(challenge_info);
  parsed_challenge_option->challenge_info = base::UTF8ToUTF16(*challenge_info);

  // Get the OTP length for this challenge. This will be displayed to the user
  // in the OTP input dialog so that the user knows how many digits the OTP
  // should be.
  absl::optional<int> otp_length =
      defined_challenge_option->FindInt("otp_length");
  parsed_challenge_option->challenge_input_length =
      otp_length ? *otp_length : kDefaultOtpLength;
}

// Parses the `defined_challenge_option` as a CVC challenge option, and sets the
// appropriate fields in `parsed_challenge_option`.
void ParseAsCvcChallengeOption(
    const base::Value::Dict* defined_challenge_option,
    CardUnmaskChallengeOption* parsed_challenge_option) {
  parsed_challenge_option->type = CardUnmaskChallengeOptionType::kCvc;

  // Get the challenge id, which is the unique identifier of this challenge
  // option. The payments server will need this challenge id to know which
  // challenge option was selected.
  const auto* challenge_id =
      defined_challenge_option->FindString("challenge_id");
  DCHECK(challenge_id);
  parsed_challenge_option->id =
      CardUnmaskChallengeOption::ChallengeOptionId(*challenge_id);

  // Get the length of the CVC on the card. In most cases this is 3 digits,
  // but it is possible for this to be 4 digits, for example in the case of
  // the Card Identification Number on the front of an American Express card.
  absl::optional<int> cvc_length =
      defined_challenge_option->FindInt("cvc_length");
  parsed_challenge_option->challenge_input_length =
      cvc_length ? *cvc_length : kDefaultCvcLength;

  // Get the position of the CVC on the card. In most cases it will be on the
  // back of the card, but it is possible for it to be on the front, for
  // example in the case of the Card Identification Number on the front of an
  // American Express card. We will also build `challenge_info_position_string`,
  // which will be used to build the challenge info that will be rendered if we
  // end up displaying the authentication selection dialog.
  std::u16string challenge_info_position_string;
  const auto* cvc_position =
      defined_challenge_option->FindString("cvc_position");
  if (cvc_position) {
    if (*cvc_position == "CVC_POSITION_FRONT") {
      parsed_challenge_option->cvc_position = CvcPosition::kFrontOfCard;
      challenge_info_position_string = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_SECURITY_CODE_POSITION_FRONT_OF_CARD);
    } else if (*cvc_position == "CVC_POSITION_BACK") {
      parsed_challenge_option->cvc_position = CvcPosition::kBackOfCard;
      challenge_info_position_string = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_CARD_UNMASK_PROMPT_SECURITY_CODE_POSITION_BACK_OF_CARD);
    } else {
      NOTREACHED();
      parsed_challenge_option->cvc_position = CvcPosition::kUnknown;
    }
  }

  // Build the challenge info for this CVC challenge option. The challenge info
  // will be displayed under the authentication label for the challenge option
  // in the authentication selection dialog if we have multiple challenge
  // options present.
  if (!challenge_info_position_string.empty()) {
    parsed_challenge_option->challenge_info = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_CARD_UNMASK_AUTHENTICATION_SELECTION_DIALOG_CVC_CHALLENGE_INFO,
        base::NumberToString16(parsed_challenge_option->challenge_input_length),
        challenge_info_position_string);
  }
}

CardUnmaskChallengeOption ParseCardUnmaskChallengeOption(
    const base::Value::Dict& challenge_option) {
  const base::Value::Dict* defined_challenge_option;
  CardUnmaskChallengeOption parsed_challenge_option;

  // Check if it's an SMS OTP challenge option, and if it is, set
  // `defined_challenge_option` to the defined challenge option found, parse the
  // challenge option, and return it.
  if ((defined_challenge_option =
           challenge_option.FindDict("sms_otp_challenge_option"))) {
    ParseAsOtpChallengeOption(defined_challenge_option,
                              &parsed_challenge_option,
                              CardUnmaskChallengeOptionType::kSmsOtp);
  }
  // Check if it's an email OTP challenge option, and if it is, set
  // `defined_challenge_option` to the defined challenge option found, parse the
  // challenge option, and return it.
  else if (base::FeatureList::IsEnabled(
               features::kAutofillEnableEmailOtpForVcnYellowPath) &&
           (defined_challenge_option =
                challenge_option.FindDict("email_otp_challenge_option"))) {
    ParseAsOtpChallengeOption(defined_challenge_option,
                              &parsed_challenge_option,
                              CardUnmaskChallengeOptionType::kEmailOtp);
  }
  // Check if it's a CVC challenge option, and if it is, set
  // `defined_challenge_option` to the defined challenge option found, parse the
  // challenge option, and return it.
  else if ((defined_challenge_option =
                challenge_option.FindDict("cvc_challenge_option"))) {
    ParseAsCvcChallengeOption(defined_challenge_option,
                              &parsed_challenge_option);
  }

  // If it is not a challenge option type that we can parse, return an empty
  // challenge option.
  return parsed_challenge_option;
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
  base::Value::Dict request_dict;
  if (!request_details_.card.server_id().empty()) {
    request_dict.Set("credit_card_id", request_details_.card.server_id());
  }
  if (request_details_.card.instrument_id() != 0) {
    request_dict.Set(
        "instrument_id",
        base::NumberToString(request_details_.card.instrument_id()));
  }
  if (base::FeatureList::IsEnabled(
          features::kAutofillAlwaysReturnCloudTokenizedCard)) {
    // See b/140727361.
    request_dict.Set("instrument_token", "INSTRUMENT_TOKEN_FOR_TEST");
  }
  request_dict.Set("risk_data_encoded",
                   BuildRiskDictionary(request_details_.risk_data));
  base::Value::Dict context;
  context.Set("billable_service", kUnmaskCardBillableServiceNumber);
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

  if (!request_details_.context_token.empty())
    request_dict.Set("context_token", request_details_.context_token);

  int value = 0;
  if (base::StringToInt(request_details_.user_response.exp_month, &value))
    request_dict.Set("expiration_month", value);
  if (base::StringToInt(request_details_.user_response.exp_year, &value))
    request_dict.Set("expiration_year", value);

  request_dict.Set("opt_in_fido_auth",
                   request_details_.user_response.enable_fido_auth);

  if (request_details_.selected_challenge_option) {
    base::Value::Dict challenge_option;
    if (request_details_.selected_challenge_option->type ==
        CardUnmaskChallengeOptionType::kCvc) {
      challenge_option.Set(
          "challenge_id",
          request_details_.selected_challenge_option->id.value());
      challenge_option.Set(
          "cvc_length",
          base::NumberToString(request_details_.selected_challenge_option
                                   ->challenge_input_length));

      base::StringPiece cvc_position = "CVC_POSITION_UNKNOWN";
      switch (request_details_.selected_challenge_option->cvc_position) {
        case autofill::CvcPosition::kFrontOfCard:
          cvc_position = "CVC_POSITION_FRONT";
          break;
        case autofill::CvcPosition::kBackOfCard:
          cvc_position = "CVC_POSITION_BACK";
          break;
        case autofill::CvcPosition::kUnknown:
          NOTREACHED();
          break;
      }
      challenge_option.Set("cvc_position", cvc_position);

      request_dict.Set("cvc_challenge_option", std::move(challenge_option));
    }
  }

  bool is_cvc_auth = !request_details_.user_response.cvc.empty();
  bool is_otp_auth = !request_details_.otp.empty();
  bool is_fido_auth = request_details_.fido_assertion_info.has_value();

  // At most, only one of these auth methods can be provided.
  DCHECK_LE(is_cvc_auth + is_fido_auth + is_otp_auth, 1);
  if (is_cvc_auth) {
    request_dict.Set("encrypted_cvc", "__param:s7e_13_cvc");
  } else if (is_otp_auth) {
    request_dict.Set("otp", "__param:s7e_263_otp");
  } else if (is_fido_auth) {
    request_dict.Set("fido_assertion_info",
                     std::move(request_details_.fido_assertion_info.value()));
  }

  if (request_details_.last_committed_primary_main_frame_origin.has_value()) {
    base::Value::Dict virtual_card_request_info;
    virtual_card_request_info.Set(
        "merchant_domain",
        request_details_.last_committed_primary_main_frame_origin.value()
            .spec());
    request_dict.Set("virtual_card_request_info",
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

void UnmaskCardRequest::ParseResponse(const base::Value::Dict& response) {
  const std::string* pan = response.FindString("pan");
  response_details_.real_pan = pan ? *pan : std::string();

  const std::string* dcvv = response.FindString("dcvv");
  response_details_.dcvv = dcvv ? *dcvv : std::string();

  const base::Value::Dict* expiration = response.FindDict("expiration");
  if (expiration) {
    if (absl::optional<int> month = expiration->FindInt("month")) {
      response_details_.expiration_month = base::NumberToString(month.value());
    }

    if (absl::optional<int> year = expiration->FindInt("year")) {
      response_details_.expiration_year = base::NumberToString(year.value());
    }
  }

  const base::Value::Dict* request_options =
      response.FindDict("fido_request_options");
  if (request_options)
    response_details_.fido_request_options = request_options->Clone();

  const base::Value::List* challenge_option_list =
      response.FindList("idv_challenge_options");
  if (challenge_option_list) {
    std::vector<CardUnmaskChallengeOption> card_unmask_challenge_options;
    for (const base::Value& challenge_option : *challenge_option_list) {
      CardUnmaskChallengeOption parsed_challenge_option =
          ParseCardUnmaskChallengeOption(challenge_option.GetDict());
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
      response.FindString("card_authorization_token");
  response_details_.card_authorization_token =
      card_authorization_token ? *card_authorization_token : std::string();

  const std::string* context_token = response.FindString("context_token");
  response_details_.context_token =
      context_token ? *context_token : std::string();

  const std::string* flow_status = response.FindString("flow_status");
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

  const base::Value::Dict* decline_details =
      response.FindDict("decline_details");
  if (decline_details) {
    AutofillErrorDialogContext autofill_error_dialog_context;

    const std::string* user_message_title =
        decline_details->FindString("user_message_title");
    if (user_message_title && !user_message_title->empty()) {
      autofill_error_dialog_context.server_returned_title = *user_message_title;
    }

    const std::string* user_message_description =
        decline_details->FindString("user_message_description");
    if (user_message_description && !user_message_description->empty()) {
      autofill_error_dialog_context.server_returned_description =
          *user_message_description;
    }

    // Only set the |autofill_error_dialog_context| in |response_details_| if
    // both the title and description were returned from the server.
    if (autofill_error_dialog_context.server_returned_title &&
        autofill_error_dialog_context.server_returned_description) {
      response_details_.autofill_error_dialog_context =
          autofill_error_dialog_context;
    }
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
      return IsAllCardInformationValidIncludingDcvv() ||
             CanPerformVirtualCardAuth();
  }
}

void UnmaskCardRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, response_details_);
}

bool UnmaskCardRequest::IsRetryableFailure(const std::string& error_code) {
  // If the response error code indicates we are in the retryable failure case,
  // return true.
  if (PaymentsRequest::IsRetryableFailure(error_code))
    return true;

  // The additional case where this can be a retryable failure is only for
  // virtual cards, so if we are not in the virtual card unmasking case at this
  // point, return false.
  if (request_details_.card.record_type() != CreditCard::VIRTUAL_CARD)
    return false;

  // If a challenge option was not selected, we are not in the virtual card
  // unmasking case, so return false.
  if (!request_details_.selected_challenge_option)
    return false;

  // The additional retryable failure functionality currently only applies to
  // virtual card CVC auth, so if we did not select a CVC challenge option,
  // return false.
  if (request_details_.selected_challenge_option->type !=
      CardUnmaskChallengeOptionType::kCvc) {
    return false;
  }

  // If we are in the VCN CVC auth case and there is a flow status present
  // return true, otherwise return false.
  return !response_details_.flow_status.empty();
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
