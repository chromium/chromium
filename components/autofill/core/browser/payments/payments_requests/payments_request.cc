// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_requests/payments_request.h"

#include <utility>

#include "base/strings/string_number_conversions.h"
#include "base/types/cxx23_to_underlying.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/payments/client_behavior_constants.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"

namespace autofill::payments {

PaymentsRequest::PaymentsRequest() {
  // Enforce the invariant: if you have a client-side timeout set, you must
  // provide a name for the associated histogram.
  if (GetTimeout().has_value()) {
    CHECK(!GetHistogramName().empty())
        << "If a PaymentsRequest subclass sets a client-side timeout, it must "
           "also provide a GetHistogram implementation.";
  }
}

PaymentsRequest::~PaymentsRequest() = default;

bool PaymentsRequest::IsRetryableFailure(const std::string& error_code) {
  // Returns true if the `error_code` denotes this is a retryable failure. This
  // should be overridden in subclasses that have additional cases where the
  // PaymentsRpcResult should be kTryAgainFailure if certain conditions are
  // true. If this function is overridden in subclasses, the super class'
  // implementation should still be called in addition to the subclass'
  // implementation. An example of this is in the virtual card CVC
  // authentication flow, we want to set result to kTryAgainFailure if a flow
  // status is present in the response.
  return base::EqualsCaseInsensitiveASCII(error_code, "internal");
}

std::string PaymentsRequest::GetHistogramName() const {
  return "";
}

std::optional<base::TimeDelta> PaymentsRequest::GetTimeout() const {
  return std::nullopt;
}

base::Value::Dict PaymentsRequest::BuildRiskDictionary(
    const std::string& encoded_risk_data) {
  base::Value::Dict risk_data;
#if BUILDFLAG(IS_IOS)
  // Browser fingerprinting is not available on iOS. Instead, we generate
  // RiskAdvisoryData.
  risk_data.Set("message_type", "RISK_ADVISORY_DATA");
  risk_data.Set("encoding_type", "BASE_64_URL");
#else
  risk_data.Set("message_type", "BROWSER_NATIVE_FINGERPRINTING");
  risk_data.Set("encoding_type", "BASE_64");
#endif

  risk_data.Set("value", encoded_risk_data);

  return risk_data;
}

base::Value::Dict PaymentsRequest::BuildCustomerContextDictionary(
    int64_t external_customer_id) {
  base::Value::Dict customer_context;
  customer_context.Set("external_customer_id",
                       base::NumberToString(external_customer_id));
  return customer_context;
}

base::Value::Dict PaymentsRequest::BuildChromeUserContext(
    const std::vector<ClientBehaviorConstants>& client_behavior_signals,
    bool full_sync_enabled) {
  base::Value::Dict chrome_user_context;
  chrome_user_context.Set("full_sync_enabled", full_sync_enabled);
  if (!client_behavior_signals.empty()) {
    base::Value::List active_client_signals;
    for (ClientBehaviorConstants signal : client_behavior_signals) {
      active_client_signals.Append(base::to_underlying(signal));
    }
    base::ranges::sort(active_client_signals);
    chrome_user_context.Set("client_behavior_signals",
                            std::move(active_client_signals));
  }
  return chrome_user_context;
}

base::Value::Dict PaymentsRequest::BuildAddressDictionary(
    const AutofillProfile& profile,
    const std::string& app_locale,
    bool include_non_location_data) {
  base::Value::Dict postal_address;

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, NAME_FULL, app_locale,
                        PaymentsNetworkInterface::kRecipientName,
                        postal_address);
  }

  base::Value::List address_lines;
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE1, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE2, app_locale,
                         address_lines);
  AppendStringIfNotEmpty(profile, ADDRESS_HOME_LINE3, app_locale,
                         address_lines);
  if (!address_lines.empty())
    postal_address.Set("address_line", std::move(address_lines));

  SetStringIfNotEmpty(profile, ADDRESS_HOME_CITY, app_locale, "locality_name",
                      postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_STATE, app_locale,
                      "administrative_area_name", postal_address);
  SetStringIfNotEmpty(profile, ADDRESS_HOME_ZIP, app_locale,
                      "postal_code_number", postal_address);

  // Use GetRawInfo to get a country code instead of the country name:
  const std::u16string country_code = profile.GetRawInfo(ADDRESS_HOME_COUNTRY);
  if (!country_code.empty())
    postal_address.Set("country_name_code", country_code);

  base::Value::Dict address;
  address.Set("postal_address", std::move(postal_address));

  if (include_non_location_data) {
    SetStringIfNotEmpty(profile, PHONE_HOME_WHOLE_NUMBER, app_locale,
                        PaymentsNetworkInterface::kPhoneNumber, address);
  }

  return address;
}

base::Value::Dict PaymentsRequest::BuildCreditCardDictionary(
    const CreditCard& credit_card,
    const std::string& app_locale,
    const std::string& pan_field_name) {
  base::Value::Dict card;
  card.Set("unique_id", credit_card.guid());

  const std::u16string exp_month =
      credit_card.GetInfo(CREDIT_CARD_EXP_MONTH, app_locale);
  const std::u16string exp_year =
      credit_card.GetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, app_locale);
  int value = 0;
  if (base::StringToInt(exp_month, &value))
    card.Set("expiration_month", value);
  if (base::StringToInt(exp_year, &value))
    card.Set("expiration_year", value);
  SetStringIfNotEmpty(credit_card, CREDIT_CARD_NAME_FULL, app_locale,
                      "cardholder_name", card);

  if (credit_card.HasNonEmptyValidNickname())
    card.Set("nickname", credit_card.nickname());

  card.Set("encrypted_pan", "__param:" + pan_field_name);
  return card;
}

// static
void PaymentsRequest::AppendStringIfNotEmpty(const AutofillProfile& profile,
                                             const FieldType& type,
                                             const std::string& app_locale,
                                             base::Value::List& list) {
  std::u16string value = profile.GetInfo(type, app_locale);
  if (!value.empty())
    list.Append(value);
}

// static
void PaymentsRequest::SetStringIfNotEmpty(const AutofillDataModel& profile,
                                          const FieldType& type,
                                          const std::string& app_locale,
                                          const std::string& path,
                                          base::Value::Dict& dictionary) {
  std::u16string value = profile.GetInfo(type, app_locale);
  if (!value.empty())
    dictionary.Set(path, std::move(value));
}

}  // namespace autofill::payments
