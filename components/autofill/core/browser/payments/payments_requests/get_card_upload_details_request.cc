// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/get_card_upload_details_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"

namespace autofill::payments {

namespace {
const char kGetCardUploadDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforsavecard";
}  // namespace

GetCardUploadDetailsRequest::GetCardUploadDetailsRequest(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    const bool full_sync_enabled,
    const std::string& app_locale,
    base::OnceCallback<void(PaymentsAutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value::Dict>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    PaymentsNetworkInterface::UploadCardSource upload_card_source)
    : addresses_(addresses),
      detected_values_(detected_values),
      client_behavior_signals_(client_behavior_signals),
      full_sync_enabled_(full_sync_enabled),
      app_locale_(app_locale),
      callback_(std::move(callback)),
      billable_service_number_(billable_service_number),
      upload_card_source_(upload_card_source),
      billing_customer_number_(billing_customer_number) {}

GetCardUploadDetailsRequest::~GetCardUploadDetailsRequest() = default;

std::string GetCardUploadDetailsRequest::GetRequestUrlPath() {
  return kGetCardUploadDetailsRequestPath;
}

std::string GetCardUploadDetailsRequest::GetRequestContentType() {
  return "application/json";
}

std::string GetCardUploadDetailsRequest::GetRequestContent() {
  base::Value::Dict request_dict;
  base::Value::Dict context;
  context.Set("language_code", app_locale_);
  context.Set("billable_service", billable_service_number_);
  if (billing_customer_number_ != 0) {
    context.Set("customer_context",
                BuildCustomerContextDictionary(billing_customer_number_));
  }
  request_dict.Set("context", std::move(context));
  request_dict.Set(
      "chrome_user_context",
      BuildChromeUserContext(client_behavior_signals_, full_sync_enabled_));

  base::Value::List addresses;
  for (const AutofillProfile& profile : addresses_) {
    // These addresses are used by Payments to (1) accurately determine the
    // user's country in order to show the correct legal documents and (2) to
    // verify that the addresses are valid for their purposes so that we don't
    // offer save in a case where it would definitely fail (e.g. P.O. boxes if
    // min address is not possible). The final parameter directs
    // BuildAddressDictionary to omit names and phone numbers, which aren't
    // useful for these purposes.
    addresses.Append(BuildAddressDictionary(profile, app_locale_, false));
  }
  request_dict.Set("address", std::move(addresses));

  // It's possible we may not have found name/address/CVC in the checkout
  // flow. The detected_values_ bitmask tells Payments what *was* found, and
  // Payments will decide if the provided data is enough to offer upload save.
  request_dict.Set("detected_values", detected_values_);

  switch (upload_card_source_) {
    case PaymentsNetworkInterface::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE:
      request_dict.Set("upload_card_source", "UNKNOWN_UPLOAD_CARD_SOURCE");
      break;
    case PaymentsNetworkInterface::UploadCardSource::UPSTREAM_CHECKOUT_FLOW:
      request_dict.Set("upload_card_source", "UPSTREAM_CHECKOUT_FLOW");
      break;
    case PaymentsNetworkInterface::UploadCardSource::UPSTREAM_SETTINGS_PAGE:
      request_dict.Set("upload_card_source", "UPSTREAM_SETTINGS_PAGE");
      break;
    case PaymentsNetworkInterface::UploadCardSource::UPSTREAM_CARD_OCR:
      request_dict.Set("upload_card_source", "UPSTREAM_CARD_OCR");
      break;
    case PaymentsNetworkInterface::UploadCardSource::
        LOCAL_CARD_MIGRATION_CHECKOUT_FLOW:
      request_dict.Set("upload_card_source",
                       "LOCAL_CARD_MIGRATION_CHECKOUT_FLOW");
      break;
    case PaymentsNetworkInterface::UploadCardSource::
        LOCAL_CARD_MIGRATION_SETTINGS_PAGE:
      request_dict.Set("upload_card_source",
                       "LOCAL_CARD_MIGRATION_SETTINGS_PAGE");
      break;
    default:
      NOTREACHED_IN_MIGRATION();
  }

  std::string request_content;
  base::JSONWriter::Write(request_dict, &request_content);
  VLOG(3) << "getdetailsforsavecard request body: " << request_content;
  return request_content;
}

void GetCardUploadDetailsRequest::ParseResponse(
  const base::Value::Dict& response) {
  const auto* context_token = response.FindString("context_token");
  context_token_ =
      context_token ? base::UTF8ToUTF16(*context_token) : std::u16string();

  const base::Value::Dict* dictionary_value =
      response.FindDict("legal_message");
  if (dictionary_value)
    legal_message_ =
        std::make_unique<base::Value::Dict>(dictionary_value->Clone());

  const auto* supported_card_bin_ranges_string =
      response.FindString("supported_card_bin_ranges_string");
  supported_card_bin_ranges_ = ParseSupportedCardBinRangesString(
      supported_card_bin_ranges_string ? *supported_card_bin_ranges_string
                                       : std::string());
}

bool GetCardUploadDetailsRequest::IsResponseComplete() {
  return !context_token_.empty() && legal_message_;
}

void GetCardUploadDetailsRequest::RespondToDelegate(
    PaymentsAutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, context_token_, std::move(legal_message_),
                           supported_card_bin_ranges_);
}

std::vector<std::pair<int, int>>
GetCardUploadDetailsRequest::ParseSupportedCardBinRangesString(
    const std::string& supported_card_bin_ranges_string) {
  std::vector<std::pair<int, int>> supported_card_bin_ranges;
  std::vector<std::string> range_strings =
      base::SplitString(supported_card_bin_ranges_string, ",",
                        base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  for (std::string& range_string : range_strings) {
    std::vector<std::string> range = base::SplitString(
        range_string, "-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    DCHECK(range.size() <= 2);
    int start;
    base::StringToInt(range[0], &start);
    if (range.size() == 1) {
      supported_card_bin_ranges.emplace_back(start, start);
    } else {
      int end;
      base::StringToInt(range[1], &end);
      DCHECK_LE(start, end);
      supported_card_bin_ranges.emplace_back(start, end);
    }
  }
  return supported_card_bin_ranges;
}

}  // namespace autofill::payments
