// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_client.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_data_model.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/account_info_getter.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_requests/get_details_for_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"
#include "components/autofill/core/browser/payments/payments_requests/select_challenge_option_request.h"
#include "components/autofill/core/browser/payments/payments_requests/unmask_card_request.h"
#include "components/autofill/core/browser/payments/payments_requests/update_virtual_card_enrollment_request.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/primary_account_access_token_fetcher.h"
#include "components/signin/public/identity_manager/scope_set.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace autofill::payments {

namespace {

const char kGetUnmaskDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforgetrealpan";

const char kOptChangeRequestPath[] =
    "payments/apis/chromepaymentsservice/updateautofilluserpreference";

const char kGetUploadDetailsRequestPath[] =
    "payments/apis/chromepaymentsservice/getdetailsforsavecard";

const char kUploadCardRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/savecard"
    "?s7e_suffix=chromewallet";
const char kUploadCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_1_pan=%s&s7e_13_cvc=%s";
const char kUploadCardRequestFormatWithoutCvc[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_1_pan=%s";

const char kMigrateCardsRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/migratecards"
    "?s7e_suffix=chromewallet";
const char kMigrateCardsRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";

const char kTokenFetchId[] = "wallet_client";
const char kPaymentsOAuth2Scope[] =
    "https://www.googleapis.com/auth/wallet.chrome";

GURL GetRequestUrl(const std::string& path) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch("sync-url")) {
    if (IsPaymentsProductionEnabled()) {
      LOG(ERROR) << "You are using production Payments but you specified a "
                    "--sync-url. You likely want to disable the sync sandbox "
                    "or switch to sandbox Payments. Both are controlled in "
                    "about:flags.";
    }
  } else if (!IsPaymentsProductionEnabled()) {
    LOG(ERROR) << "You are using sandbox Payments but you didn't specify a "
                  "--sync-url. You likely want to enable the sync sandbox "
                  "or switch to production Payments. Both are controlled in "
                  "about:flags.";
  }

  return GetBaseSecureUrl().Resolve(path);
}

void SetStringIfNotEmpty(const AutofillDataModel& profile,
                         const ServerFieldType& type,
                         const std::string& app_locale,
                         const std::string& path,
                         base::Value& dictionary) {
  const std::u16string value = profile.GetInfo(AutofillType(type), app_locale);
  if (!value.empty())
    dictionary.SetKey(path, base::Value(value));
}

void AppendStringIfNotEmpty(const AutofillProfile& profile,
                            const ServerFieldType& type,
                            const std::string& app_locale,
                            base::Value& list) {
  const std::u16string value = profile.GetInfo(type, app_locale);
  if (!value.empty())
    list.Append(value);
}

// Returns a dictionary with the structure expected by Payments RPCs, containing
// each of the fields in |profile|, formatted according to |app_locale|. If
// |include_non_location_data| is false, the name and phone number in |profile|
// are not included.
base::Value BuildAddressDictionary(const AutofillProfile& profile,
                                   const std::string& app_locale,
                                   bool include_non_location_data) {
  base::Value postal_address(base::Value::Type::DICTIONARY);

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, NAME_FULL, app_locale,
                        PaymentsClient::kRecipientName, postal_address);
  }

  base::Value address_lines(base::Value::Type::LIST);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE1, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE2, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE3, app_locale,
                         address_lines);
  if (!address_lines.GetListDeprecated().empty())
    postal_address.SetKey("address_line", std::move(address_lines));

  SetStringIfNotEmpty(profile, ADDRESS_HOME_CITY, app_locale, "locality_name",
                      postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_STATE, app_locale,
                      "administrative_area_name", postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_ZIP, app_locale,
                      "postal_code_number", postal_address);

  // Use GetRawInfo to get a country code instead of the country name:
  const std::u16string country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    postal_address.SetKey("country_name_code", base::Value(country_code));

  base::Value address(base::Value::Type::DICTIONARY);
  address.SetKey("postal_address", std::move(postal_address));

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, PHONE_HOME_WHOLE_NUMBER, app_locale,
                        PaymentsClient::kPhoneNumber, address);
  }

  return address;
}

// Returns a dictionary of the credit card with the structure expected by
// Payments RPCs, containing expiration month, expiration year and cardholder
// name (if any) fields in |credit_card|, formatted according to |app_locale|.
// |pan_field_name| is the field name for the encrypted pan. We use each credit
// card's guid as the unique id.
base::Value BuildCreditCardDictionary(const CreditCard& credit_card,
                                      const std::string& app_locale,
                                      const std::string& pan_field_name) {
  base::Value card(base::Value::Type::DICTIONARY);
  card.SetKey("unique_id", base::Value(credit_card.guid()));

  const std::u16string exp_month =
      credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
  const std::u16string exp_year = credit_card.GetInfo(
      AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
  int value = 0;
  if (base::StringToInt(exp_month, &value))
    card.SetKey("expiration_month", base::Value(value));
  if (base::StringToInt(exp_year, &value))
    card.SetKey("expiration_year", base::Value(value));
  SetStringIfNotEmpty(credit_card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", card);

  if (credit_card.HasNonEmptyValidNickname())
    card.SetKey("nickname", base::Value(credit_card.nickname()));

  card.SetKey("encrypted_pan", base::Value("__param:" + pan_field_name));
  return card;
}

// Populates the list of active experiments that affect either the data sent in
// payments RPCs or whether the RPCs are sent or not.
void SetActiveExperiments(const std::vector<const char*>& active_experiments,
                          base::Value& request_dict) {
  if (active_experiments.empty())
    return;

  base::Value active_chrome_experiments(base::Value::Type::LIST);
  for (const char* it : active_experiments)
    active_chrome_experiments.Append(it);

  request_dict.SetKey("active_chrome_experiments",
                      std::move(active_chrome_experiments));
}

// TODO(crbug.com/1249665): Move requests to separate files.
class GetUnmaskDetailsRequest : public PaymentsRequest {
 public:
  GetUnmaskDetailsRequest(
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::UnmaskDetails&)> callback,
      const std::string& app_locale,
      const bool full_sync_enabled)
      : callback_(std::move(callback)),
        app_locale_(app_locale),
        full_sync_enabled_(full_sync_enabled) {}

  GetUnmaskDetailsRequest(const GetUnmaskDetailsRequest&) = delete;
  GetUnmaskDetailsRequest& operator=(const GetUnmaskDetailsRequest&) = delete;

  ~GetUnmaskDetailsRequest() override = default;

  std::string GetRequestUrlPath() override {
    return kGetUnmaskDetailsRequestPath;
  }

  std::string GetRequestContentType() override { return "application/json"; }

  std::string GetRequestContent() override {
    base::Value request_dict(base::Value::Type::DICTIONARY);
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("language_code", base::Value(app_locale_));
    context.SetKey("billable_service",
                   base::Value(kUnmaskCardBillableServiceNumber));
    request_dict.SetKey("context", std::move(context));

    base::Value chrome_user_context(base::Value::Type::DICTIONARY);
    chrome_user_context.SetKey("full_sync_enabled",
                               base::Value(full_sync_enabled_));
    request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

    std::string request_content;
    base::JSONWriter::Write(request_dict, &request_content);
    VLOG(3) << "getdetailsforgetrealpan request body: " << request_content;
    return request_content;
  }

  void ParseResponse(const base::Value& response) override {
    const auto* method = response.FindStringKey("authentication_method");
    if (method) {
      if (*method == "CVC") {
        unmask_details_.unmask_auth_method =
            AutofillClient::UnmaskAuthMethod::kCvc;
      } else if (*method == "FIDO") {
        unmask_details_.unmask_auth_method =
            AutofillClient::UnmaskAuthMethod::kFido;
      }
    }

    const auto* offer_fido_opt_in =
        response.FindKeyOfType("offer_fido_opt_in", base::Value::Type::BOOLEAN);
    unmask_details_.offer_fido_opt_in =
        offer_fido_opt_in && offer_fido_opt_in->GetBool();

    const auto* dictionary_value = response.FindKeyOfType(
        "fido_request_options", base::Value::Type::DICTIONARY);
    if (dictionary_value)
      unmask_details_.fido_request_options = dictionary_value->Clone();

    const auto* fido_eligible_card_ids = response.FindKeyOfType(
        "fido_eligible_card_id", base::Value::Type::LIST);
    if (fido_eligible_card_ids) {
      for (const base::Value& result :
           fido_eligible_card_ids->GetListDeprecated()) {
        unmask_details_.fido_eligible_card_ids.insert(result.GetString());
      }
    }
  }

  bool IsResponseComplete() override {
    return unmask_details_.unmask_auth_method !=
           AutofillClient::UnmaskAuthMethod::kUnknown;
  }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, unmask_details_);
  }

 private:
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          PaymentsClient::UnmaskDetails&)>
      callback_;
  std::string app_locale_;
  const bool full_sync_enabled_;

  // Suggested authentication method and other information to facilitate card
  // unmasking.
  payments::PaymentsClient::UnmaskDetails unmask_details_;
};

class OptChangeRequest : public PaymentsRequest {
 public:
  OptChangeRequest(
      const PaymentsClient::OptChangeRequestDetails& request_details,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              PaymentsClient::OptChangeResponseDetails&)>
          callback,
      const bool full_sync_enabled)
      : request_details_(request_details),
        callback_(std::move(callback)),
        full_sync_enabled_(full_sync_enabled) {}

  OptChangeRequest(const OptChangeRequest&) = delete;
  OptChangeRequest& operator=(const OptChangeRequest&) = delete;

  ~OptChangeRequest() override = default;

  std::string GetRequestUrlPath() override { return kOptChangeRequestPath; }

  std::string GetRequestContentType() override { return "application/json"; }

  std::string GetRequestContent() override {
    base::Value request_dict(base::Value::Type::DICTIONARY);
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("language_code", base::Value(request_details_.app_locale));
    context.SetKey("billable_service",
                   base::Value(kUnmaskCardBillableServiceNumber));
    request_dict.SetKey("context", std::move(context));

    base::Value chrome_user_context(base::Value::Type::DICTIONARY);
    chrome_user_context.SetKey("full_sync_enabled",
                               base::Value(full_sync_enabled_));
    request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

    std::string reason;
    switch (request_details_.reason) {
      case PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH:
        reason = "ENABLE_FIDO_AUTH";
        break;
      case PaymentsClient::OptChangeRequestDetails::DISABLE_FIDO_AUTH:
        reason = "DISABLE_FIDO_AUTH";
        break;
      case PaymentsClient::OptChangeRequestDetails::ADD_CARD_FOR_FIDO_AUTH:
        reason = "ADD_CARD_FOR_FIDO_AUTH";
        break;
      default:
        NOTREACHED();
        break;
    }
    request_dict.SetKey("reason", base::Value(reason));

    if (request_details_.fido_authenticator_response.has_value()) {
      base::Value fido_authentication_info(base::Value::Type::DICTIONARY);

      fido_authentication_info.SetKey(
          "fido_authenticator_response",
          std::move(request_details_.fido_authenticator_response.value()));

      if (!request_details_.card_authorization_token.empty()) {
        fido_authentication_info.SetKey(
            "card_authorization_token",
            base::Value(request_details_.card_authorization_token));
      }

      request_dict.SetKey("fido_authentication_info",
                          std::move(fido_authentication_info));
    }

    std::string request_content;
    base::JSONWriter::Write(request_dict, &request_content);
    VLOG(3) << "updateautofilluserpreference request body: " << request_content;
    return request_content;
  }

  void ParseResponse(const base::Value& response) override {
    const auto* fido_authentication_info = response.FindKeyOfType(
        "fido_authentication_info", base::Value::Type::DICTIONARY);
    if (!fido_authentication_info)
      return;

    const auto* user_status =
        fido_authentication_info->FindStringKey("user_status");
    if (user_status && *user_status != "UNKNOWN_USER_STATUS")
      response_details_.user_is_opted_in =
          (*user_status == "FIDO_AUTH_ENABLED");

    const auto* fido_creation_options = fido_authentication_info->FindKeyOfType(
        "fido_creation_options", base::Value::Type::DICTIONARY);
    if (fido_creation_options)
      response_details_.fido_creation_options = fido_creation_options->Clone();

    const auto* fido_request_options = fido_authentication_info->FindKeyOfType(
        "fido_request_options", base::Value::Type::DICTIONARY);
    if (fido_request_options)
      response_details_.fido_request_options = fido_request_options->Clone();
  }

  bool IsResponseComplete() override {
    return response_details_.user_is_opted_in.has_value();
  }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, response_details_);
  }

 private:
  PaymentsClient::OptChangeRequestDetails request_details_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          PaymentsClient::OptChangeResponseDetails&)>
      callback_;
  const bool full_sync_enabled_;
  PaymentsClient::OptChangeResponseDetails response_details_;
};

class GetUploadDetailsRequest : public PaymentsRequest {
 public:
  GetUploadDetailsRequest(
      const std::vector<AutofillProfile>& addresses,
      const int detected_values,
      const std::vector<const char*>& active_experiments,
      const bool full_sync_enabled,
      const std::string& app_locale,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const std::u16string&,
                              std::unique_ptr<base::Value>,
                              std::vector<std::pair<int, int>>)> callback,
      const int billable_service_number,
      const int64_t billing_customer_number,
      PaymentsClient::UploadCardSource upload_card_source)
      : addresses_(addresses),
        detected_values_(detected_values),
        active_experiments_(active_experiments),
        full_sync_enabled_(full_sync_enabled),
        app_locale_(app_locale),
        callback_(std::move(callback)),
        billable_service_number_(billable_service_number),
        upload_card_source_(upload_card_source),
        billing_customer_number_(billing_customer_number) {}

  GetUploadDetailsRequest(const GetUploadDetailsRequest&) = delete;
  GetUploadDetailsRequest& operator=(const GetUploadDetailsRequest&) = delete;

  ~GetUploadDetailsRequest() override = default;

  std::string GetRequestUrlPath() override {
    return kGetUploadDetailsRequestPath;
  }

  std::string GetRequestContentType() override { return "application/json"; }

  std::string GetRequestContent() override {
    base::Value request_dict(base::Value::Type::DICTIONARY);
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("language_code", base::Value(app_locale_));
    context.SetKey("billable_service", base::Value(billable_service_number_));
    if (base::FeatureList::IsEnabled(
            features::kAutofillEnableSendingBcnInGetUploadDetails) &&
        billing_customer_number_ != 0) {
      context.SetKey("customer_context",
                     BuildCustomerContextDictionary(billing_customer_number_));
    }
    request_dict.SetKey("context", std::move(context));

    base::Value chrome_user_context(base::Value::Type::DICTIONARY);
    chrome_user_context.SetKey("full_sync_enabled",
                               base::Value(full_sync_enabled_));
    request_dict.SetKey("chrome_user_context", std::move(chrome_user_context));

    base::Value addresses(base::Value::Type::LIST);
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
    request_dict.SetKey("address", std::move(addresses));

    // It's possible we may not have found name/address/CVC in the checkout
    // flow. The detected_values_ bitmask tells Payments what *was* found, and
    // Payments will decide if the provided data is enough to offer upload save.
    request_dict.SetKey("detected_values", base::Value(detected_values_));

    SetActiveExperiments(active_experiments_, request_dict);

    switch (upload_card_source_) {
      case PaymentsClient::UploadCardSource::UNKNOWN_UPLOAD_CARD_SOURCE:
        request_dict.SetKey("upload_card_source",
                            base::Value("UNKNOWN_UPLOAD_CARD_SOURCE"));
        break;
      case PaymentsClient::UploadCardSource::UPSTREAM_CHECKOUT_FLOW:
        request_dict.SetKey("upload_card_source",
                            base::Value("UPSTREAM_CHECKOUT_FLOW"));
        break;
      case PaymentsClient::UploadCardSource::UPSTREAM_SETTINGS_PAGE:
        request_dict.SetKey("upload_card_source",
                            base::Value("UPSTREAM_SETTINGS_PAGE"));
        break;
      case PaymentsClient::UploadCardSource::UPSTREAM_CARD_OCR:
        request_dict.SetKey("upload_card_source",
                            base::Value("UPSTREAM_CARD_OCR"));
        break;
      case PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_CHECKOUT_FLOW:
        request_dict.SetKey("upload_card_source",
                            base::Value("LOCAL_CARD_MIGRATION_CHECKOUT_FLOW"));
        break;
      case PaymentsClient::UploadCardSource::LOCAL_CARD_MIGRATION_SETTINGS_PAGE:
        request_dict.SetKey("upload_card_source",
                            base::Value("LOCAL_CARD_MIGRATION_SETTINGS_PAGE"));
        break;
      default:
        NOTREACHED();
    }

    std::string request_content;
    base::JSONWriter::Write(request_dict, &request_content);
    VLOG(3) << "getdetailsforsavecard request body: " << request_content;
    return request_content;
  }

  void ParseResponse(const base::Value& response) override {
    const auto* context_token = response.FindStringKey("context_token");
    context_token_ =
        context_token ? base::UTF8ToUTF16(*context_token) : std::u16string();

    const base::Value* dictionary_value =
        response.FindKeyOfType("legal_message", base::Value::Type::DICTIONARY);
    if (dictionary_value)
      legal_message_ = std::make_unique<base::Value>(dictionary_value->Clone());

    const auto* supported_card_bin_ranges_string =
        response.FindStringKey("supported_card_bin_ranges_string");
    supported_card_bin_ranges_ = ParseSupportedCardBinRangesString(
        supported_card_bin_ranges_string ? *supported_card_bin_ranges_string
                                         : base::EmptyString());
  }

  bool IsResponseComplete() override {
    return !context_token_.empty() && legal_message_;
  }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, context_token_, std::move(legal_message_),
                             supported_card_bin_ranges_);
  }

 private:
  // Helper for ParseResponse(). Input format should be :"1234,30000-55555,765",
  // where ranges are separated by commas and items separated with a dash means
  // the start and ends of the range. Items without a dash have the same start
  // and end (ex. 1234-1234)
  std::vector<std::pair<int, int>> ParseSupportedCardBinRangesString(
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

  const std::vector<AutofillProfile> addresses_;
  const int detected_values_;
  const std::vector<const char*> active_experiments_;
  const bool full_sync_enabled_;
  std::string app_locale_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const std::u16string&,
                          std::unique_ptr<base::Value>,
                          std::vector<std::pair<int, int>>)>
      callback_;
  std::u16string context_token_;
  std::unique_ptr<base::Value> legal_message_;
  std::vector<std::pair<int, int>> supported_card_bin_ranges_;
  const int billable_service_number_;
  PaymentsClient::UploadCardSource upload_card_source_;
  const int64_t billing_customer_number_;
};

class UploadCardRequest : public PaymentsRequest {
 public:
  UploadCardRequest(
      const PaymentsClient::UploadRequestDetails& request_details,
      const bool full_sync_enabled,
      base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                              const PaymentsClient::UploadCardResponseDetails&)>
          callback)
      : request_details_(request_details),
        full_sync_enabled_(full_sync_enabled),
        callback_(std::move(callback)) {}

  UploadCardRequest(const UploadCardRequest&) = delete;
  UploadCardRequest& operator=(const UploadCardRequest&) = delete;

  ~UploadCardRequest() override = default;

  std::string GetRequestUrlPath() override { return kUploadCardRequestPath; }

  std::string GetRequestContentType() override {
    return "application/x-www-form-urlencoded";
  }

  std::string GetRequestContent() override {
    base::Value request_dict(base::Value::Type::DICTIONARY);
    request_dict.SetKey("encrypted_pan", base::Value("__param:s7e_1_pan"));
    if (!request_details_.cvc.empty())
      request_dict.SetKey("encrypted_cvc", base::Value("__param:s7e_13_cvc"));
    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));

    const std::string& app_locale = request_details_.app_locale;
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("language_code", base::Value(app_locale));
    context.SetKey("billable_service",
                   base::Value(kUploadCardBillableServiceNumber));
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

    SetStringIfNotEmpty(request_details_.card, CREDIT_CARD_NAME_FULL,
                        app_locale, "cardholder_name", request_dict);

    base::Value addresses(base::Value::Type::LIST);
    for (const AutofillProfile& profile : request_details_.profiles) {
      addresses.Append(BuildAddressDictionary(profile, app_locale, true));
    }
    request_dict.SetKey("address", std::move(addresses));

    request_dict.SetKey("context_token",
                        base::Value(request_details_.context_token));

    int value = 0;
    const std::u16string exp_month = request_details_.card.GetInfo(
        AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
    const std::u16string exp_year = request_details_.card.GetInfo(
        AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
    if (base::StringToInt(exp_month, &value))
      request_dict.SetKey("expiration_month", base::Value(value));
    if (base::StringToInt(exp_year, &value))
      request_dict.SetKey("expiration_year", base::Value(value));

    if (request_details_.card.HasNonEmptyValidNickname()) {
      request_dict.SetKey("nickname",
                          base::Value(request_details_.card.nickname()));
    }

    SetActiveExperiments(request_details_.active_experiments, request_dict);

    const std::u16string pan = request_details_.card.GetInfo(
        AutofillType(CREDIT_CARD_NUMBER), app_locale);
    std::string json_request;
    base::JSONWriter::Write(request_dict, &json_request);
    std::string request_content;
    if (request_details_.cvc.empty()) {
      request_content = base::StringPrintf(
          kUploadCardRequestFormatWithoutCvc,
          net::EscapeUrlEncodedData(json_request, true).c_str(),
          net::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str());
    } else {
      request_content = base::StringPrintf(
          kUploadCardRequestFormat,
          net::EscapeUrlEncodedData(json_request, true).c_str(),
          net::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str(),
          net::EscapeUrlEncodedData(base::UTF16ToASCII(request_details_.cvc),
                                    true)
              .c_str());
    }
    VLOG(3) << "savecard request body: " << request_content;
    return request_content;
  }

  void ParseResponse(const base::Value& response) override {
    const std::string* credit_card_id =
        response.FindStringKey("credit_card_id");
    upload_card_response_details_.server_id =
        credit_card_id ? *credit_card_id : std::string();

    const std::string* response_instrument_id =
        response.FindStringKey("instrument_id");
    if (response_instrument_id) {
      int64_t instrument_id;
      if (base::StringToInt64(base::StringPiece(*response_instrument_id),
                              &instrument_id)) {
        upload_card_response_details_.instrument_id = instrument_id;
      }
    }

    const auto* virtual_card_metadata = response.FindKeyOfType(
        "virtual_card_metadata", base::Value::Type::DICTIONARY);
    if (virtual_card_metadata) {
      const std::string* virtual_card_enrollment_status =
          virtual_card_metadata->FindStringKey("status");
      if (virtual_card_enrollment_status) {
        if (*virtual_card_enrollment_status == "ENROLLED") {
          upload_card_response_details_.virtual_card_enrollment_state =
              CreditCard::VirtualCardEnrollmentState::ENROLLED;
        } else if (*virtual_card_enrollment_status == "ENROLLMENT_ELIGIBLE") {
          upload_card_response_details_.virtual_card_enrollment_state =
              CreditCard::VirtualCardEnrollmentState::UNENROLLED_AND_ELIGIBLE;
        } else {
          upload_card_response_details_.virtual_card_enrollment_state =
              CreditCard::VirtualCardEnrollmentState::
                  UNENROLLED_AND_NOT_ELIGIBLE;
        }
      }
    }

    const std::string* card_art_url = response.FindStringKey("card_art_url");
    upload_card_response_details_.card_art_url =
        card_art_url ? GURL(*card_art_url) : GURL();
  }

  bool IsResponseComplete() override { return true; }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, upload_card_response_details_);
  }

 private:
  const PaymentsClient::UploadRequestDetails request_details_;
  const bool full_sync_enabled_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const PaymentsClient::UploadCardResponseDetails&)>
      callback_;
  PaymentsClient::UploadCardResponseDetails upload_card_response_details_;
};

class MigrateCardsRequest : public PaymentsRequest {
 public:
  MigrateCardsRequest(
      const PaymentsClient::MigrationRequestDetails& request_details,
      const std::vector<MigratableCreditCard>& migratable_credit_cards,
      const bool full_sync_enabled,
      MigrateCardsCallback callback)
      : request_details_(request_details),
        migratable_credit_cards_(migratable_credit_cards),
        full_sync_enabled_(full_sync_enabled),
        callback_(std::move(callback)) {}

  MigrateCardsRequest(const MigrateCardsRequest&) = delete;
  MigrateCardsRequest& operator=(const MigrateCardsRequest&) = delete;

  ~MigrateCardsRequest() override = default;

  std::string GetRequestUrlPath() override { return kMigrateCardsRequestPath; }

  std::string GetRequestContentType() override {
    return "application/x-www-form-urlencoded";
  }

  std::string GetRequestContent() override {
    base::Value request_dict(base::Value::Type::DICTIONARY);

    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));

    const std::string& app_locale = request_details_.app_locale;
    base::Value context(base::Value::Type::DICTIONARY);
    context.SetKey("language_code", base::Value(app_locale));
    context.SetKey("billable_service",
                   base::Value(kMigrateCardsBillableServiceNumber));
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

    request_dict.SetKey("context_token",
                        base::Value(request_details_.context_token));

    std::string all_pans_data = std::string();
    base::Value migrate_cards(base::Value::Type::LIST);
    for (size_t index = 0; index < migratable_credit_cards_.size(); ++index) {
      std::string pan_field_name = GetPanFieldName(index);
      // Generate credit card dictionary.
      migrate_cards.Append(BuildCreditCardDictionary(
          migratable_credit_cards_[index].credit_card(), app_locale,
          pan_field_name));
      // Append pan data to the |all_pans_data|.
      all_pans_data +=
          GetAppendPan(migratable_credit_cards_[index].credit_card(),
                       app_locale, pan_field_name);
    }
    request_dict.SetKey("local_card", std::move(migrate_cards));

    std::string json_request;
    base::JSONWriter::Write(request_dict, &json_request);
    std::string request_content = base::StringPrintf(
        kMigrateCardsRequestFormat,
        net::EscapeUrlEncodedData(json_request, true).c_str());
    request_content += all_pans_data;
    return request_content;
  }

  void ParseResponse(const base::Value& response) override {
    const auto* found_list =
        response.FindKeyOfType("save_result", base::Value::Type::LIST);
    if (!found_list)
      return;

    save_result_ =
        std::make_unique<std::unordered_map<std::string, std::string>>();
    for (const base::Value& result : found_list->GetListDeprecated()) {
      if (result.is_dict()) {
        const std::string* unique_id = result.FindStringKey("unique_id");
        const std::string* status = result.FindStringKey("status");
        save_result_->insert(
            std::make_pair(unique_id ? *unique_id : std::string(),
                           status ? *status : std::string()));
      }
    }

    const std::string* display_text =
        response.FindStringKey("value_prop_display_text");
    display_text_ = display_text ? *display_text : std::string();
  }

  bool IsResponseComplete() override {
    return !display_text_.empty() && save_result_;
  }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, std::move(save_result_), display_text_);
  }

 private:
  // Return the pan field name for the encrypted pan based on the |index|.
  std::string GetPanFieldName(const size_t& index) {
    return "s7e_1_pan" + std::to_string(index);
  }

  // Return the formatted pan to append to the end of the request.
  std::string GetAppendPan(const CreditCard& credit_card,
                           const std::string& app_locale,
                           const std::string& pan_field_name) {
    const std::u16string pan =
        credit_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale);
    std::string pan_str =
        net::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str();
    std::string append_pan = "&" + pan_field_name + "=" + pan_str;
    return append_pan;
  }

  const PaymentsClient::MigrationRequestDetails request_details_;
  const std::vector<MigratableCreditCard>& migratable_credit_cards_;
  const bool full_sync_enabled_;
  MigrateCardsCallback callback_;
  std::unique_ptr<std::unordered_map<std::string, std::string>> save_result_;
  std::string display_text_;
};

}  // namespace

const char PaymentsClient::kRecipientName[] = "recipient_name";
const char PaymentsClient::kPhoneNumber[] = "phone_number";

PaymentsClient::UnmaskDetails::UnmaskDetails() = default;
PaymentsClient::UnmaskDetails::~UnmaskDetails() = default;
PaymentsClient::UnmaskDetails& PaymentsClient::UnmaskDetails::operator=(
    const PaymentsClient::UnmaskDetails& other) {
  unmask_auth_method = other.unmask_auth_method;
  offer_fido_opt_in = other.offer_fido_opt_in;
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  fido_eligible_card_ids = other.fido_eligible_card_ids;
  return *this;
}

PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails() = default;
PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails(
    const UnmaskRequestDetails& other) {
  *this = other;
}
PaymentsClient::UnmaskRequestDetails&
PaymentsClient::UnmaskRequestDetails::operator=(
    const PaymentsClient::UnmaskRequestDetails& other) {
  billing_customer_number = other.billing_customer_number;
  card = other.card;
  risk_data = other.risk_data;
  user_response = other.user_response;
  if (other.fido_assertion_info.has_value()) {
    fido_assertion_info = other.fido_assertion_info->Clone();
  } else {
    fido_assertion_info.reset();
  }
  context_token = other.context_token;
  otp = other.otp;
  last_committed_url_origin = other.last_committed_url_origin;
  return *this;
}
PaymentsClient::UnmaskRequestDetails::~UnmaskRequestDetails() = default;

PaymentsClient::UnmaskResponseDetails::UnmaskResponseDetails() = default;
PaymentsClient::UnmaskResponseDetails::UnmaskResponseDetails(
    const UnmaskResponseDetails& other) {
  *this = other;
}
PaymentsClient::UnmaskResponseDetails::~UnmaskResponseDetails() = default;
PaymentsClient::UnmaskResponseDetails&
PaymentsClient::UnmaskResponseDetails::operator=(
    const PaymentsClient::UnmaskResponseDetails& other) {
  real_pan = other.real_pan;
  if (other.fido_creation_options.has_value()) {
    fido_creation_options = other.fido_creation_options->Clone();
  } else {
    fido_creation_options.reset();
  }
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
  card_unmask_challenge_options = other.card_unmask_challenge_options;
  card_authorization_token = other.card_authorization_token;
  flow_status = other.flow_status;
  context_token = other.context_token;
  return *this;
}

PaymentsClient::OptChangeRequestDetails::OptChangeRequestDetails() = default;
PaymentsClient::OptChangeRequestDetails::OptChangeRequestDetails(
    const OptChangeRequestDetails& other) {
  app_locale = other.app_locale;
  reason = other.reason;
  if (other.fido_authenticator_response.has_value()) {
    fido_authenticator_response = other.fido_authenticator_response->Clone();
  } else {
    fido_authenticator_response.reset();
  }
  card_authorization_token = other.card_authorization_token;
}
PaymentsClient::OptChangeRequestDetails::~OptChangeRequestDetails() = default;

PaymentsClient::OptChangeResponseDetails::OptChangeResponseDetails() = default;
PaymentsClient::OptChangeResponseDetails::OptChangeResponseDetails(
    const OptChangeResponseDetails& other) {
  user_is_opted_in = other.user_is_opted_in;

  if (other.fido_creation_options.has_value()) {
    fido_creation_options = other.fido_creation_options->Clone();
  } else {
    fido_creation_options.reset();
  }
  if (other.fido_request_options.has_value()) {
    fido_request_options = other.fido_request_options->Clone();
  } else {
    fido_request_options.reset();
  }
}
PaymentsClient::OptChangeResponseDetails::~OptChangeResponseDetails() = default;

PaymentsClient::UploadRequestDetails::UploadRequestDetails() = default;
PaymentsClient::UploadRequestDetails::UploadRequestDetails(
    const UploadRequestDetails& other) = default;
PaymentsClient::UploadRequestDetails::~UploadRequestDetails() = default;

PaymentsClient::MigrationRequestDetails::MigrationRequestDetails() = default;
PaymentsClient::MigrationRequestDetails::MigrationRequestDetails(
    const MigrationRequestDetails& other) = default;
PaymentsClient::MigrationRequestDetails::~MigrationRequestDetails() = default;

PaymentsClient::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails() = default;
PaymentsClient::SelectChallengeOptionRequestDetails::
    SelectChallengeOptionRequestDetails(
        const SelectChallengeOptionRequestDetails& other) = default;
PaymentsClient::SelectChallengeOptionRequestDetails::
    ~SelectChallengeOptionRequestDetails() = default;

PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails() = default;
PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    GetDetailsForEnrollmentRequestDetails(
        const GetDetailsForEnrollmentRequestDetails& other) = default;
PaymentsClient::GetDetailsForEnrollmentRequestDetails::
    ~GetDetailsForEnrollmentRequestDetails() = default;

PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails() = default;
PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    GetDetailsForEnrollmentResponseDetails(
        const GetDetailsForEnrollmentResponseDetails& other) = default;
PaymentsClient::GetDetailsForEnrollmentResponseDetails::
    ~GetDetailsForEnrollmentResponseDetails() = default;

PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails() = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    UpdateVirtualCardEnrollmentRequestDetails(
        const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails&
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::operator=(
    const UpdateVirtualCardEnrollmentRequestDetails&) = default;
PaymentsClient::UpdateVirtualCardEnrollmentRequestDetails::
    ~UpdateVirtualCardEnrollmentRequestDetails() = default;

PaymentsClient::PaymentsClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    signin::IdentityManager* identity_manager,
    AccountInfoGetter* account_info_getter,
    bool is_off_the_record)
    : url_loader_factory_(url_loader_factory),
      identity_manager_(identity_manager),
      account_info_getter_(account_info_getter),
      is_off_the_record_(is_off_the_record),
      has_retried_authorization_(false) {}

PaymentsClient::~PaymentsClient() = default;

void PaymentsClient::Prepare() {
  if (access_token_.empty())
    StartTokenFetch(false);
}

void PaymentsClient::GetUnmaskDetails(
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskDetails&)> callback,
    const std::string& app_locale) {
  IssueRequest(std::make_unique<GetUnmaskDetailsRequest>(
                   std::move(callback), app_locale,
                   account_info_getter_->IsSyncFeatureEnabled()),
               /*authenticate=*/true);
}

void PaymentsClient::UnmaskCard(
    const PaymentsClient::UnmaskRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::UnmaskResponseDetails&)> callback) {
  IssueRequest(
      std::make_unique<UnmaskCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::OptChange(
    const OptChangeRequestDetails request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            PaymentsClient::OptChangeResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<OptChangeRequest>(
                   request_details, std::move(callback),
                   account_info_getter_->IsSyncFeatureEnabled()),
               /*authenticate=*/true);
}

void PaymentsClient::GetUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<const char*>& active_experiments,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::u16string&,
                            std::unique_ptr<base::Value>,
                            std::vector<std::pair<int, int>>)> callback,
    const int billable_service_number,
    const int64_t billing_customer_number,
    UploadCardSource upload_card_source) {
  IssueRequest(std::make_unique<GetUploadDetailsRequest>(
                   addresses, detected_values, active_experiments,
                   account_info_getter_->IsSyncFeatureEnabled(), app_locale,
                   std::move(callback), billable_service_number,
                   billing_customer_number, upload_card_source),
               /*authenticate=*/false);
}

void PaymentsClient::UploadCard(
    const PaymentsClient::UploadRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const UploadCardResponseDetails&)> callback) {
  IssueRequest(
      std::make_unique<UploadCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::MigrateCards(
    const MigrationRequestDetails& request_details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    MigrateCardsCallback callback) {
  IssueRequest(
      std::make_unique<MigrateCardsRequest>(
          request_details, migratable_credit_cards,
          account_info_getter_->IsSyncFeatureEnabled(), std::move(callback)),
      /*authenticate=*/true);
}

void PaymentsClient::SelectChallengeOption(
    const SelectChallengeOptionRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  IssueRequest(std::make_unique<SelectChallengeOptionRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::GetVirtualCardEnrollmentDetails(
    const GetDetailsForEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const payments::PaymentsClient::
                                GetDetailsForEnrollmentResponseDetails&)>
        callback) {
  IssueRequest(std::make_unique<GetDetailsForEnrollmentRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::UpdateVirtualCardEnrollment(
    const UpdateVirtualCardEnrollmentRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult)> callback) {
  IssueRequest(std::make_unique<UpdateVirtualCardEnrollmentRequest>(
                   request_details, std::move(callback)),
               /*authenticate=*/true);
}

void PaymentsClient::CancelRequest() {
  request_.reset();
  resource_request_.reset();
  simple_url_loader_.reset();
  token_fetcher_.reset();
  access_token_.clear();
  has_retried_authorization_ = false;
}

void PaymentsClient::set_url_loader_factory_for_testing(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory) {
  url_loader_factory_ = std::move(url_loader_factory);
}

void PaymentsClient::IssueRequest(std::unique_ptr<PaymentsRequest> request,
                                  bool authenticate) {
  request_ = std::move(request);
  has_retried_authorization_ = false;

  InitializeResourceRequest();

  if (!authenticate) {
    StartRequest();
  } else if (access_token_.empty()) {
    StartTokenFetch(false);
  } else {
    SetOAuth2TokenAndStartRequest();
  }
}

void PaymentsClient::InitializeResourceRequest() {
  resource_request_ = std::make_unique<network::ResourceRequest>();
  resource_request_->url = GetRequestUrl(request_->GetRequestUrlPath());
  resource_request_->load_flags = net::LOAD_DISABLE_CACHE;
  resource_request_->credentials_mode = network::mojom::CredentialsMode::kOmit;
  resource_request_->method = "POST";

  // Add Chrome experiment state to the request headers.
  net::HttpRequestHeaders headers;
  // User is always signed-in to be able to upload card to Google Payments.
  variations::AppendVariationsHeader(
      resource_request_->url,
      is_off_the_record_ ? variations::InIncognito::kYes
                         : variations::InIncognito::kNo,
      variations::SignedIn::kYes, resource_request_.get());
}

void PaymentsClient::OnSimpleLoaderComplete(
    std::unique_ptr<std::string> response_body) {
  int response_code = -1;
  if (simple_url_loader_->ResponseInfo() &&
      simple_url_loader_->ResponseInfo()->headers) {
    response_code =
        simple_url_loader_->ResponseInfo()->headers->response_code();
  }
  std::string data;
  if (response_body)
    data = std::move(*response_body);
  OnSimpleLoaderCompleteInternal(response_code, data);
}

void PaymentsClient::OnSimpleLoaderCompleteInternal(int response_code,
                                                    const std::string& data) {
  VLOG(2) << "Got data: " << data;

  AutofillClient::PaymentsRpcResult result =
      AutofillClient::PaymentsRpcResult::kSuccess;

  if (!request_)
    return;

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      std::string error_code;
      std::string error_api_error_reason;
      absl::optional<base::Value> message_value = base::JSONReader::Read(data);
      if (message_value && message_value->is_dict()) {
        const auto* found_error_code = message_value->FindPathOfType(
            {"error", "code"}, base::Value::Type::STRING);
        if (found_error_code)
          error_code = found_error_code->GetString();

        const auto* found_error_reason = message_value->FindPathOfType(
            {"error", "api_error_reason"}, base::Value::Type::STRING);
        if (found_error_reason)
          error_api_error_reason = found_error_reason->GetString();

        request_->ParseResponse(*message_value);
      }

      if (base::LowerCaseEqualsASCII(error_api_error_reason,
                                     "virtual_card_temporary_error")) {
        result =
            AutofillClient::PaymentsRpcResult::kVcnRetrievalTryAgainFailure;
      } else if (base::LowerCaseEqualsASCII(error_api_error_reason,
                                            "virtual_card_permanent_error")) {
        result =
            AutofillClient::PaymentsRpcResult::kVcnRetrievalPermanentFailure;
      } else if (base::LowerCaseEqualsASCII(error_code, "internal")) {
        result = AutofillClient::PaymentsRpcResult::kTryAgainFailure;
      } else if (!error_code.empty() || !request_->IsResponseComplete()) {
        result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
      }

      break;
    }

    case net::HTTP_UNAUTHORIZED: {
      if (has_retried_authorization_) {
        result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
        break;
      }
      has_retried_authorization_ = true;

      InitializeResourceRequest();
      StartTokenFetch(true);
      return;
    }

    // TODO(estade): is this actually how network connectivity issues are
    // reported?
    case net::HTTP_REQUEST_TIMEOUT: {
      result = AutofillClient::PaymentsRpcResult::kNetworkError;
      break;
    }

    // Handle anything else as a generic (permanent) failure.
    default: {
      result = AutofillClient::PaymentsRpcResult::kPermanentFailure;
      break;
    }
  }

  if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
    VLOG(1) << "Payments returned error: " << response_code
            << " with data: " << data;
  }

  request_->RespondToDelegate(result);
}

void PaymentsClient::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    signin::AccessTokenInfo access_token_info) {
  DCHECK(token_fetcher_);
  token_fetcher_.reset();

  if (error.state() != GoogleServiceAuthError::NONE) {
    AccessTokenError(error);
    return;
  }

  access_token_ = access_token_info.token;
  if (resource_request_)
    SetOAuth2TokenAndStartRequest();
}

void PaymentsClient::AccessTokenError(const GoogleServiceAuthError& error) {
  VLOG(1) << "Unhandled OAuth2 error: " << error.ToString();
  if (simple_url_loader_)
    simple_url_loader_.reset();
  if (request_)
    request_->RespondToDelegate(
        AutofillClient::PaymentsRpcResult::kPermanentFailure);
}

void PaymentsClient::StartTokenFetch(bool invalidate_old) {
  // We're still waiting for the last request to come back.
  if (!invalidate_old && token_fetcher_)
    return;

  DCHECK(account_info_getter_);

  signin::ScopeSet payments_scopes;
  payments_scopes.insert(kPaymentsOAuth2Scope);
  CoreAccountId account_id =
      account_info_getter_->GetAccountInfoForPaymentsServer().account_id;
  if (invalidate_old) {
    DCHECK(!access_token_.empty());
    identity_manager_->RemoveAccessTokenFromCache(account_id, payments_scopes,
                                                  access_token_);
  }
  access_token_.clear();
  token_fetcher_ = identity_manager_->CreateAccessTokenFetcherForAccount(
      account_id, kTokenFetchId, payments_scopes,
      base::BindOnce(&PaymentsClient::AccessTokenFetchFinished,
                     base::Unretained(this)),
      signin::AccessTokenFetcher::Mode::kImmediate);
}

void PaymentsClient::SetOAuth2TokenAndStartRequest() {
  DCHECK(resource_request_);
  resource_request_->headers.SetHeader(net::HttpRequestHeaders::kAuthorization,
                                       std::string("Bearer ") + access_token_);
  StartRequest();
}

void PaymentsClient::StartRequest() {
  DCHECK(resource_request_);
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("payments_sync_cards", R"(
        semantics {
          sender: "Payments"
          description:
            "This service communicates with Google Payments servers to upload "
            "(save) or receive the user's credit card info."
          trigger:
            "Requests are triggered by a user action, such as selecting a "
            "masked server card from Chromium's credit card autofill dropdown, "
            "submitting a form which has credit card information, or accepting "
            "the prompt to save a credit card to Payments servers."
          data:
            "In case of save, a protocol buffer containing relevant address "
            "and credit card information which should be saved in Google "
            "Payments servers, along with user credentials. In case of load, a "
            "protocol buffer containing the id of the credit card to unmask, "
            "an encrypted cvc value, an optional updated card expiration date, "
            "and user credentials."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "Users can enable or disable this feature in Chromium settings by "
            "toggling 'Credit cards and addresses using Google Payments', "
            "under 'Advanced sync settings...'. This feature is enabled by "
            "default."
          chrome_policy {
            AutoFillEnabled {
              policy_options {mode: MANDATORY}
              AutoFillEnabled: false
            }
          }
        })");
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request_), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(request_->GetRequestContent(),
                                            request_->GetRequestContentType());

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PaymentsClient::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

}  // namespace autofill::payments
