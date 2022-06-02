// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/migrate_cards_request.h"

#include <string>

#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"

namespace autofill::payments {

namespace {
const char kMigrateCardsRequestPath[] =
    "payments/apis-secure/chromepaymentsservice/migratecards"
    "?s7e_suffix=chromewallet";
const char kMigrateCardsRequestFormat[] =
    "requestContentType=application/json; charset=utf-8&request=%s";
}  // namespace

MigrateCardsRequest::MigrateCardsRequest(
    const PaymentsClient::MigrationRequestDetails& request_details,
    const std::vector<MigratableCreditCard>& migratable_credit_cards,
    const bool full_sync_enabled,
    MigrateCardsCallback callback)
    : request_details_(request_details),
      migratable_credit_cards_(migratable_credit_cards),
      full_sync_enabled_(full_sync_enabled),
      callback_(std::move(callback)) {}

MigrateCardsRequest::~MigrateCardsRequest() = default;

std::string MigrateCardsRequest::GetRequestUrlPath() {
  return kMigrateCardsRequestPath;
}

std::string MigrateCardsRequest::GetRequestContentType() {
  return "application/x-www-form-urlencoded";
}

std::string MigrateCardsRequest::GetRequestContent() {
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
    migrate_cards.Append(
        BuildCreditCardDictionary(migratable_credit_cards_[index].credit_card(),
                                  app_locale, pan_field_name));
    // Append pan data to the |all_pans_data|.
    all_pans_data += GetAppendPan(migratable_credit_cards_[index].credit_card(),
                                  app_locale, pan_field_name);
  }
  request_dict.SetKey("local_card", std::move(migrate_cards));

  std::string json_request;
  base::JSONWriter::Write(request_dict, &json_request);
  std::string request_content = base::StringPrintf(
      kMigrateCardsRequestFormat,
      base::EscapeUrlEncodedData(json_request, true).c_str());
  request_content += all_pans_data;
  return request_content;
}

void MigrateCardsRequest::ParseResponse(const base::Value& response) {
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

bool MigrateCardsRequest::IsResponseComplete() {
  return !display_text_.empty() && save_result_;
}

void MigrateCardsRequest::RespondToDelegate(
    AutofillClient::PaymentsRpcResult result) {
  std::move(callback_).Run(result, std::move(save_result_), display_text_);
}

std::string MigrateCardsRequest::GetPanFieldName(const size_t& index) {
  return "s7e_1_pan" + base::NumberToString(index);
}

std::string MigrateCardsRequest::GetAppendPan(
    const CreditCard& credit_card,
    const std::string& app_locale,
    const std::string& pan_field_name) {
  const std::u16string pan =
      credit_card.GetInfo(AutofillType(CREDIT_CARD_NUMBER), app_locale);
  std::string pan_str =
      base::EscapeUrlEncodedData(base::UTF16ToASCII(pan), true).c_str();
  std::string append_pan = "&" + pan_field_name + "=" + pan_str;
  return append_pan;
}

}  // namespace autofill::payments
