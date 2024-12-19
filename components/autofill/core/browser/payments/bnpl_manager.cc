// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/bnpl_manager.h"

#include <cstdint>
#include <optional>
#include <string>

#include "base/check_deref.h"
#include "base/numerics/checked_math.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_request_details.h"
#include "third_party/re2/src/re2/re2.h"

namespace autofill::payments {

BnplManager::BnplManager(PaymentsAutofillClient* payments_autofill_client)
    : payments_autofill_client_(CHECK_DEREF(payments_autofill_client)) {}

BnplManager::~BnplManager() = default;

std::optional<uint64_t> BnplManager::MaybeParseAmountToMonetaryMicroUnits(
    const std::string& amount) {
  const RE2 re(
      R"([^0-9,eE\-]*(0|[0-9]{1,3}(,?[0-9]{3})*)(\.([0-9]{2}))[^0-9eE\-]*)");
  std::string dollar;
  std::string cent;
  // The first regex capture group gives dollar and the fourth gives the cent.
  if (!RE2::FullMatch(amount, re, &dollar, nullptr, nullptr, &cent)) {
    return std::nullopt;
  }
  dollar.erase(std::remove(dollar.begin(), dollar.end(), ','), dollar.end());

  uint64_t dollar_value = 0;
  uint64_t cent_value = 0;
  base::StringToUint64(dollar, &dollar_value);
  base::StringToUint64(cent, &cent_value);

  // Safely multiply to convert amount to micro.
  constexpr int kMicrosPerDollar = 1'000'000;
  uint64_t micro_amount = 0;
  base::CheckedNumeric<uint64_t> checked_dollar_value =
      base::CheckedNumeric<uint64_t>(dollar_value) * kMicrosPerDollar;
  base::CheckedNumeric<uint64_t> checked_cent_value =
      base::CheckedNumeric<uint64_t>(cent_value) * (kMicrosPerDollar / 100);
  base::CheckedNumeric<uint64_t> checked_result =
      checked_dollar_value + checked_cent_value;
  if (!checked_result.AssignIfValid(&micro_amount)) {
    return std::nullopt;
  }
  return micro_amount;
}

void BnplManager::FetchVcnDetails() {
  GetBnplPaymentInstrumentForFetchingVcnRequestDetails request_details;
  request_details.billing_customer_number = billing_customer_number_;
  request_details.risk_data = risk_data_;
  request_details.instrument_id = instrument_id_;
  request_details.context_token = context_token_;
  request_details.redirect_url = redirect_url_;

  payments_autofill_client_->GetPaymentsNetworkInterface()
      ->GetBnplPaymentInstrumentForFetchingVcn(
          std::move(request_details),
          base::BindOnce(&BnplManager::OnVcnDetailsFetched,
                         weak_factory_.GetWeakPtr()));
}

void BnplManager::OnVcnDetailsFetched(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const BnplFetchVcnResponseDetails& response_details) {
  // TODO(crbug.com/378518604): Implement OnVcnDetailsFetched() to fill the form
  // from the VCN details that were fetched.
}

}  // namespace autofill::payments
