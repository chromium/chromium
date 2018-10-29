// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_client.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/account_info_getter.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/payments_request.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "components/variations/net/variations_http_headers.h"
#include "net/base/escape.h"
#include "net/base/load_flags.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/identity/public/cpp/identity_manager.h"
#include "services/identity/public/cpp/primary_account_access_token_fetcher.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace autofill {
namespace payments {

namespace {

const char kUnmaskCardRequestPath[] =
    "payments/apis-secure/creditcardservice/getrealpan?s7e_suffix=chromewallet";
const char kUnmaskCardRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s"
    "&s7e_13_cvc=%s";

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

base::DictionaryValue BuildCustomerContextDictionary(
    int64_t external_customer_id) {
  base::DictionaryValue customer_context;
  customer_context.SetString("external_customer_id",
                             std::to_string(external_customer_id));
  return customer_context;
}

base::DictionaryValue BuildRiskDictionary(
    const std::string& encoded_risk_data) {
  base::DictionaryValue risk_data;
#if defined(OS_IOS)
  // Browser fingerprinting is not available on iOS. Instead, we generate
  // RiskAdvisoryData.
  risk_data.SetString("message_type", "RISK_ADVISORY_DATA");
  risk_data.SetString("encoding_type", "BASE_64_URL");
#else
  risk_data.SetString("message_type", "BROWSER_NATIVE_FINGERPRINTING");
  risk_data.SetString("encoding_type", "BASE_64");
#endif

  risk_data.SetString("value", encoded_risk_data);

  return risk_data;
}

void SetStringIfNotEmpty(const AutofillDataModel& profile,
                         const ServerFieldType& type,
                         const std::string& app_locale,
                         const std::string& path,
                         base::DictionaryValue* dictionary) {
  const base::string16 value = profile.GetInfo(AutofillType(type), app_locale);
  if (!value.empty())
    dictionary->SetString(path, value);
}

void AppendStringIfNotEmpty(const AutofillProfile& profile,
                            const ServerFieldType& type,
                            const std::string& app_locale,
                            base::ListValue* list) {
  const base::string16 value = profile.GetInfo(type, app_locale);
  if (!value.empty())
    list->AppendString(value);
}

// Returns a dictionary with the structure expected by Payments RPCs, containing
// each of the fields in |profile|, formatted according to |app_locale|. If
// |include_non_location_data| is false, the name and phone number in |profile|
// are not included.
std::unique_ptr<base::DictionaryValue> BuildAddressDictionary(
    const AutofillProfile& profile,
    const std::string& app_locale,
    bool include_non_location_data) {
  std::unique_ptr<base::DictionaryValue> postal_address(
      new base::DictionaryValue());

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, NAME_FULL, app_locale,
                        PaymentsClient::kRecipientName, postal_address.get());
  }

  std::unique_ptr<base::ListValue> address_lines(new base::ListValue());
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE1, app_locale,
                         address_lines.get());
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE2, app_locale,
                         address_lines.get());
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE3, app_locale,
                         address_lines.get());
  if (!address_lines->empty())
    postal_address->Set("address_line", std::move(address_lines));

  SetStringIfNotEmpty(profile, ADDRESS_HOME_CITY, app_locale, "locality_name",
                      postal_address.get());
  SetStringIfNotEmpty(profile, ADDRESS_HOME_STATE, app_locale,
                      "administrative_area_name", postal_address.get());
  SetStringIfNotEmpty(profile, ADDRESS_HOME_ZIP, app_locale,
                      "postal_code_number", postal_address.get());

  // Use GetRawInfo to get a country code instead of the country name:
  const base::string16 country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    postal_address->SetString("country_name_code", country_code);

  std::unique_ptr<base::DictionaryValue> address(new base::DictionaryValue());
  address->Set("postal_address", std::move(postal_address));

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, PHONE_HOME_WHOLE_NUMBER, app_locale,
                        PaymentsClient::kPhoneNumber, address.get());
  }

  return address;
}

// Returns a dictionary of the credit card with the structure expected by
// Payments RPCs, containing expiration month, expiration year and cardholder
// name (if any) fields in |credit_card|, formatted according to |app_locale|.
// |pan_field_name| is the field name for the encrypted pan. We use each credit
// card's guid as the unique id.
std::unique_ptr<base::DictionaryValue> BuildCreditCardDictionary(
    const CreditCard& credit_card,
    const std::string& app_locale,
    const std::string& pan_field_name) {
  std::unique_ptr<base::DictionaryValue> card(new base::DictionaryValue());
  card->SetString("unique_id", credit_card.guid());

  const base::string16 exp_month =
      credit_card.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
  const base::string16 exp_year = credit_card.GetInfo(
      AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
  int value = 0;
  if (base::StringToInt(exp_month, &value))
    card->SetInteger("expiration_month", value);
  if (base::StringToInt(exp_year, &value))
    card->SetInteger("expiration_year", value);
  SetStringIfNotEmpty(credit_card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", card.get());

  card->SetString("encrypted_pan", "__param:" + pan_field_name);
  return card;
}

// Populates the list of active experiments that affect either the data sent in
// payments RPCs or whether the RPCs are sent or not.
void SetActiveExperiments(const std::vector<const char*>& active_experiments,
                          base::DictionaryValue* request_dict) {
  if (active_experiments.empty())
    return;

  std::unique_ptr<base::ListValue> active_chrome_experiments(
      std::make_unique<base::ListValue>());
  for (const char* it : active_experiments)
    active_chrome_experiments->AppendString(it);

  request_dict->Set("active_chrome_experiments",
                    std::move(active_chrome_experiments));
}

class UnmaskCardRequest : public PaymentsRequest {
 public:
  UnmaskCardRequest(const PaymentsClient::UnmaskRequestDetails& request_details,
                    const bool full_sync_enabled,
                    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                                            const std::string&)> callback)
      : request_details_(request_details),
        full_sync_enabled_(full_sync_enabled),
        callback_(std::move(callback)) {
    DCHECK(
        CreditCard::MASKED_SERVER_CARD == request_details.card.record_type() ||
        CreditCard::FULL_SERVER_CARD == request_details.card.record_type());
  }
  ~UnmaskCardRequest() override {}

  std::string GetRequestUrlPath() override { return kUnmaskCardRequestPath; }

  std::string GetRequestContentType() override {
    return "application/x-www-form-urlencoded";
  }

  std::string GetRequestContent() override {
    base::DictionaryValue request_dict;
    request_dict.SetString("encrypted_cvc", "__param:s7e_13_cvc");
    request_dict.SetString("credit_card_id", request_details_.card.server_id());
    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));
    std::unique_ptr<base::DictionaryValue> context(new base::DictionaryValue());
    context->SetInteger("billable_service", kUnmaskCardBillableServiceNumber);
    if (request_details_.billing_customer_number != 0) {
      context->SetKey("customer_context",
                      BuildCustomerContextDictionary(
                          request_details_.billing_customer_number));
    }
    request_dict.Set("context", std::move(context));

    if (ShouldUseActiveSignedInAccount()) {
      std::unique_ptr<base::DictionaryValue> chrome_user_context(
          new base::DictionaryValue());
      chrome_user_context->SetBoolean("full_sync_enabled", full_sync_enabled_);
      request_dict.Set("chrome_user_context", std::move(chrome_user_context));
    }

    int value = 0;
    if (base::StringToInt(request_details_.user_response.exp_month, &value))
      request_dict.SetInteger("expiration_month", value);
    if (base::StringToInt(request_details_.user_response.exp_year, &value))
      request_dict.SetInteger("expiration_year", value);

    std::string json_request;
    base::JSONWriter::Write(request_dict, &json_request);
    std::string request_content = base::StringPrintf(
        kUnmaskCardRequestFormat,
        net::EscapeUrlEncodedData(json_request, true).c_str(),
        net::EscapeUrlEncodedData(
            base::UTF16ToASCII(request_details_.user_response.cvc), true)
            .c_str());
    VLOG(3) << "getrealpan request body: " << request_content;
    return request_content;
  }

  void ParseResponse(std::unique_ptr<base::DictionaryValue> response) override {
    response->GetString("pan", &real_pan_);
  }

  bool IsResponseComplete() override { return !real_pan_.empty(); }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, real_pan_);
  }

 private:
  PaymentsClient::UnmaskRequestDetails request_details_;
  const bool full_sync_enabled_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const std::string&)>
      callback_;
  std::string real_pan_;
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
                              const base::string16&,
                              std::unique_ptr<base::DictionaryValue>)> callback,
      const int billable_service_number)
      : addresses_(addresses),
        detected_values_(detected_values),
        active_experiments_(active_experiments),
        full_sync_enabled_(full_sync_enabled),
        app_locale_(app_locale),
        callback_(std::move(callback)),
        billable_service_number_(billable_service_number) {}
  ~GetUploadDetailsRequest() override {}

  std::string GetRequestUrlPath() override {
    return kGetUploadDetailsRequestPath;
  }

  std::string GetRequestContentType() override { return "application/json"; }

  std::string GetRequestContent() override {
    base::DictionaryValue request_dict;
    std::unique_ptr<base::DictionaryValue> context(new base::DictionaryValue());
    context->SetString("language_code", app_locale_);
    context->SetInteger("billable_service", billable_service_number_);
    request_dict.Set("context", std::move(context));

    if (ShouldUseActiveSignedInAccount()) {
      std::unique_ptr<base::DictionaryValue> chrome_user_context(
          new base::DictionaryValue());
      chrome_user_context->SetBoolean("full_sync_enabled", full_sync_enabled_);
      request_dict.Set("chrome_user_context", std::move(chrome_user_context));
    }

    std::unique_ptr<base::ListValue> addresses(new base::ListValue());
    for (const AutofillProfile& profile : addresses_) {
      // These addresses are used by Payments to (1) accurately determine the
      // user's country in order to show the correct legal documents and (2) to
      // verify that the addresses are valid for their purposes so that we don't
      // offer save in a case where it would definitely fail (e.g. P.O. boxes if
      // min address is not possible). The final parameter directs
      // BuildAddressDictionary to omit names and phone numbers, which aren't
      // useful for these purposes.
      addresses->Append(BuildAddressDictionary(profile, app_locale_, false));
    }
    request_dict.Set("address", std::move(addresses));

    // It's possible we may not have found name/address/CVC in the checkout
    // flow. The detected_values_ bitmask tells Payments what *was* found, and
    // Payments will decide if the provided data is enough to offer upload save.
    request_dict.SetInteger("detected_values", detected_values_);

    SetActiveExperiments(active_experiments_, &request_dict);

    std::string request_content;
    base::JSONWriter::Write(request_dict, &request_content);
    VLOG(3) << "getdetailsforsavecard request body: " << request_content;
    return request_content;
  }

  void ParseResponse(std::unique_ptr<base::DictionaryValue> response) override {
    response->GetString("context_token", &context_token_);
    base::DictionaryValue* unowned_legal_message;
    if (response->GetDictionary("legal_message", &unowned_legal_message))
      legal_message_ = unowned_legal_message->CreateDeepCopy();
  }

  bool IsResponseComplete() override {
    return !context_token_.empty() && legal_message_;
  }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, context_token_, std::move(legal_message_));
  }

 private:
  const std::vector<AutofillProfile> addresses_;
  const int detected_values_;
  const std::vector<const char*> active_experiments_;
  const bool full_sync_enabled_;
  std::string app_locale_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const base::string16&,
                          std::unique_ptr<base::DictionaryValue>)>
      callback_;
  base::string16 context_token_;
  std::unique_ptr<base::DictionaryValue> legal_message_;
  const int billable_service_number_;
};

class UploadCardRequest : public PaymentsRequest {
 public:
  UploadCardRequest(const PaymentsClient::UploadRequestDetails& request_details,
                    const bool full_sync_enabled,
                    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                                            const std::string&)> callback)
      : request_details_(request_details),
        full_sync_enabled_(full_sync_enabled),
        callback_(std::move(callback)) {}
  ~UploadCardRequest() override {}

  std::string GetRequestUrlPath() override { return kUploadCardRequestPath; }

  std::string GetRequestContentType() override {
    return "application/x-www-form-urlencoded";
  }

  std::string GetRequestContent() override {
    base::DictionaryValue request_dict;
    request_dict.SetString("encrypted_pan", "__param:s7e_1_pan");
    if (!request_details_.cvc.empty())
      request_dict.SetString("encrypted_cvc", "__param:s7e_13_cvc");
    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));

    const std::string& app_locale = request_details_.app_locale;
    std::unique_ptr<base::DictionaryValue> context(new base::DictionaryValue());
    context->SetString("language_code", app_locale);
    context->SetInteger("billable_service", kUploadCardBillableServiceNumber);
    if (request_details_.billing_customer_number != 0) {
      context->SetKey("customer_context",
                      BuildCustomerContextDictionary(
                          request_details_.billing_customer_number));
    }
    request_dict.Set("context", std::move(context));

    if (ShouldUseActiveSignedInAccount()) {
      std::unique_ptr<base::DictionaryValue> chrome_user_context(
          new base::DictionaryValue());
      chrome_user_context->SetBoolean("full_sync_enabled", full_sync_enabled_);
      request_dict.Set("chrome_user_context", std::move(chrome_user_context));
    }

    SetStringIfNotEmpty(request_details_.card, CREDIT_CARD_NAME_FULL,
                        app_locale, "cardholder_name", &request_dict);

    std::unique_ptr<base::ListValue> addresses(new base::ListValue());
    for (const AutofillProfile& profile : request_details_.profiles) {
      addresses->Append(BuildAddressDictionary(profile, app_locale, true));
    }
    request_dict.Set("address", std::move(addresses));

    request_dict.SetString("context_token", request_details_.context_token);

    int value = 0;
    const base::string16 exp_month = request_details_.card.GetInfo(
        AutofillType(CREDIT_CARD_EXP_MONTH), app_locale);
    const base::string16 exp_year = request_details_.card.GetInfo(
        AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), app_locale);
    if (base::StringToInt(exp_month, &value))
      request_dict.SetInteger("expiration_month", value);
    if (base::StringToInt(exp_year, &value))
      request_dict.SetInteger("expiration_year", value);

    SetActiveExperiments(request_details_.active_experiments, &request_dict);

    const base::string16 pan = request_details_.card.GetInfo(
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

  void ParseResponse(std::unique_ptr<base::DictionaryValue> response) override {
    response->GetString("credit_card_id", &server_id_);
  }

  bool IsResponseComplete() override { return true; }

  void RespondToDelegate(AutofillClient::PaymentsRpcResult result) override {
    std::move(callback_).Run(result, server_id_);
  }

 private:
  const PaymentsClient::UploadRequestDetails request_details_;
  const bool full_sync_enabled_;
  base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                          const std::string&)>
      callback_;
  std::string server_id_;
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
  ~MigrateCardsRequest() override {}

  std::string GetRequestUrlPath() override { return kMigrateCardsRequestPath; }

  std::string GetRequestContentType() override {
    return "application/x-www-form-urlencoded";
  }

  // TODO(crbug.com/877281):Refactor DictionaryValue to base::Value
  std::string GetRequestContent() override {
    base::DictionaryValue request_dict;

    request_dict.SetKey("risk_data_encoded",
                        BuildRiskDictionary(request_details_.risk_data));

    const std::string& app_locale = request_details_.app_locale;
    std::unique_ptr<base::DictionaryValue> context(new base::DictionaryValue());
    context->SetString("language_code", app_locale);
    context->SetInteger("billable_service", kMigrateCardsBillableServiceNumber);
    if (request_details_.billing_customer_number != 0) {
      context->SetKey("customer_context",
                      BuildCustomerContextDictionary(
                          request_details_.billing_customer_number));
    }
    request_dict.Set("context", std::move(context));

    if (ShouldUseActiveSignedInAccount()) {
      std::unique_ptr<base::DictionaryValue> chrome_user_context(
          new base::DictionaryValue());
      chrome_user_context->SetBoolean("full_sync_enabled", full_sync_enabled_);
      request_dict.Set("chrome_user_context", std::move(chrome_user_context));
    }

    request_dict.SetString("context_token", request_details_.context_token);

    std::string all_pans_data = std::string();
    std::unique_ptr<base::ListValue> migrate_cards(new base::ListValue());
    for (size_t index = 0; index < migratable_credit_cards_.size(); ++index) {
      std::string pan_field_name = GetPanFieldName(index);
      // Generate credit card dictionary.
      migrate_cards->Append(BuildCreditCardDictionary(
          migratable_credit_cards_[index].credit_card(), app_locale,
          pan_field_name));
      // Append pan data to the |all_pans_data|.
      all_pans_data +=
          GetAppendPan(migratable_credit_cards_[index].credit_card(),
                       app_locale, pan_field_name);
    }
    request_dict.Set("local_card", std::move(migrate_cards));

    std::string json_request;
    base::JSONWriter::Write(request_dict, &json_request);
    std::string request_content = base::StringPrintf(
        kMigrateCardsRequestFormat,
        net::EscapeUrlEncodedData(json_request, true).c_str());
    request_content += all_pans_data;
    return request_content;
  }

  void ParseResponse(std::unique_ptr<base::DictionaryValue> response) override {
    const base::ListValue* save_result_list = nullptr;
    if (!response->GetList("save_result", &save_result_list))
      return;
    save_result_ =
        std::make_unique<std::unordered_map<std::string, std::string>>();
    for (size_t i = 0; i < save_result_list->GetSize(); ++i) {
      const base::DictionaryValue* single_card_save_result;
      if (save_result_list->GetDictionary(i, &single_card_save_result)) {
        std::string unique_id;
        single_card_save_result->GetString("unique_id", &unique_id);
        std::string save_result;
        single_card_save_result->GetString("status", &save_result);
        save_result_->insert(std::make_pair(unique_id, save_result));
      }
    }
    response->GetString("value_prop_display_text", &display_text_);
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
    const base::string16 pan =
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

PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails() {}
PaymentsClient::UnmaskRequestDetails::UnmaskRequestDetails(
    const UnmaskRequestDetails& other) = default;
PaymentsClient::UnmaskRequestDetails::~UnmaskRequestDetails() {}

PaymentsClient::UploadRequestDetails::UploadRequestDetails() {}
PaymentsClient::UploadRequestDetails::UploadRequestDetails(
    const UploadRequestDetails& other) = default;
PaymentsClient::UploadRequestDetails::~UploadRequestDetails() {}

PaymentsClient::MigrationRequestDetails::MigrationRequestDetails() {}
PaymentsClient::MigrationRequestDetails::MigrationRequestDetails(
    const MigrationRequestDetails& other) = default;
PaymentsClient::MigrationRequestDetails::~MigrationRequestDetails() {}

PaymentsClient::PaymentsClient(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    PrefService* pref_service,
    identity::IdentityManager* identity_manager,
    AccountInfoGetter* account_info_getter,
    bool is_off_the_record)
    : url_loader_factory_(url_loader_factory),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      account_info_getter_(account_info_getter),
      is_off_the_record_(is_off_the_record),
      has_retried_authorization_(false),
      weak_ptr_factory_(this) {}

PaymentsClient::~PaymentsClient() {}

void PaymentsClient::Prepare() {
  if (access_token_.empty())
    StartTokenFetch(false);
}

PrefService* PaymentsClient::GetPrefService() const {
  return pref_service_;
}

void PaymentsClient::UnmaskCard(
    const PaymentsClient::UnmaskRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  IssueRequest(
      std::make_unique<UnmaskCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      true);
}

void PaymentsClient::GetUploadDetails(
    const std::vector<AutofillProfile>& addresses,
    const int detected_values,
    const std::vector<const char*>& active_experiments,
    const std::string& app_locale,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const base::string16&,
                            std::unique_ptr<base::DictionaryValue>)> callback,
    const int billable_service_number) {
  IssueRequest(std::make_unique<GetUploadDetailsRequest>(
                   addresses, detected_values, active_experiments,
                   account_info_getter_->IsSyncFeatureEnabled(), app_locale,
                   std::move(callback), billable_service_number),
               false);
}

void PaymentsClient::UploadCard(
    const PaymentsClient::UploadRequestDetails& request_details,
    base::OnceCallback<void(AutofillClient::PaymentsRpcResult,
                            const std::string&)> callback) {
  IssueRequest(
      std::make_unique<UploadCardRequest>(
          request_details, account_info_getter_->IsSyncFeatureEnabled(),
          std::move(callback)),
      true);
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
  resource_request_->load_flags = net::LOAD_DO_NOT_SAVE_COOKIES |
                                  net::LOAD_DO_NOT_SEND_COOKIES |
                                  net::LOAD_DISABLE_CACHE;
  resource_request_->method = "POST";
  if (base::FeatureList::IsEnabled(
          features::kAutofillSendExperimentIdsInPaymentsRPCs)) {
    // Add Chrome experiment state to the request headers.
    net::HttpRequestHeaders headers;
    // User is always signed-in to be able to upload card to Google Payments.
    variations::AppendVariationHeaders(
        resource_request_->url,
        is_off_the_record_ ? variations::InIncognito::kYes
                           : variations::InIncognito::kNo,
        variations::SignedIn::kYes, &resource_request_->headers);
  }
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
  std::unique_ptr<base::DictionaryValue> response_dict;
  VLOG(2) << "Got data: " << data;

  AutofillClient::PaymentsRpcResult result = AutofillClient::SUCCESS;

  switch (response_code) {
    // Valid response.
    case net::HTTP_OK: {
      std::string error_code;
      std::unique_ptr<base::Value> message_value = base::JSONReader::Read(data);
      if (message_value.get() && message_value->is_dict()) {
        response_dict.reset(
            static_cast<base::DictionaryValue*>(message_value.release()));
        response_dict->GetString("error.code", &error_code);
        request_->ParseResponse(std::move(response_dict));
      }

      if (base::LowerCaseEqualsASCII(error_code, "internal"))
        result = AutofillClient::TRY_AGAIN_FAILURE;
      else if (!error_code.empty() || !request_->IsResponseComplete())
        result = AutofillClient::PERMANENT_FAILURE;

      break;
    }

    case net::HTTP_UNAUTHORIZED: {
      if (has_retried_authorization_) {
        result = AutofillClient::PERMANENT_FAILURE;
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
      result = AutofillClient::NETWORK_ERROR;
      break;
    }

    // Handle anything else as a generic (permanent) failure.
    default: {
      result = AutofillClient::PERMANENT_FAILURE;
      break;
    }
  }

  if (result != AutofillClient::SUCCESS) {
    VLOG(1) << "Payments returned error: " << response_code
            << " with data: " << data;
  }

  request_->RespondToDelegate(result);
}

void PaymentsClient::AccessTokenFetchFinished(
    GoogleServiceAuthError error,
    identity::AccessTokenInfo access_token_info) {
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
    request_->RespondToDelegate(AutofillClient::PERMANENT_FAILURE);
}

void PaymentsClient::StartTokenFetch(bool invalidate_old) {
  // We're still waiting for the last request to come back.
  if (!invalidate_old && token_fetcher_)
    return;

  DCHECK(account_info_getter_);

  identity::ScopeSet payments_scopes;
  payments_scopes.insert(kPaymentsOAuth2Scope);
  std::string account_id =
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
      identity::AccessTokenFetcher::Mode::kImmediate);
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
  // TODO(https://crbug.com/808498): Re-add data use measurement once
  // SimpleURLLoader supports it.
  // ID=data_use_measurement::DataUseUserData::AUTOFILL
  simple_url_loader_ = network::SimpleURLLoader::Create(
      std::move(resource_request_), traffic_annotation);
  simple_url_loader_->AttachStringForUpload(request_->GetRequestContent(),
                                            request_->GetRequestContentType());

  simple_url_loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
      url_loader_factory_.get(),
      base::BindOnce(&PaymentsClient::OnSimpleLoaderComplete,
                     base::Unretained(this)));
}

}  // namespace payments
}  // namespace autofill
