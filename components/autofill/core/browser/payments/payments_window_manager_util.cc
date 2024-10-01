// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_window_manager_util.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill::payments {

base::expected<PaymentsWindowManager::RedirectCompletionResult,
               PaymentsWindowManager::Vcn3dsAuthenticationResult>
ParseUrlForVcn3ds(const GURL& url,
                  const Vcn3dsChallengeOptionMetadata& metadata) {
  std::string redirect_completion_result;
  bool is_failure = false;
  std::string_view query_piece = url.query_piece();
  url::Component query(0, query_piece.length());
  url::Component key;
  url::Component value;
  while (url::ExtractQueryKeyValue(query_piece, &query, &key, &value)) {
    std::string_view key_view = query_piece.substr(key.begin, key.len);
    std::string_view value_view = query_piece.substr(value.begin, value.len);
    if (key_view == metadata.success_query_param_name) {
      redirect_completion_result = std::string(value_view);
    } else if (key_view == metadata.failure_query_param_name) {
      is_failure = true;
    }
  }

  // `redirect_completion_result` being present indicates the user completed the
  // authentication and a request to the Payments servers is required to
  // retrieve the authentication result.
  if (!redirect_completion_result.empty()) {
    return base::ok(PaymentsWindowManager::RedirectCompletionResult(
        redirect_completion_result));
  }

  // `is_failure` being true indicates the authentication has failed.
  if (is_failure) {
    return base::unexpected(PaymentsWindowManager::Vcn3dsAuthenticationResult::
                                kAuthenticationFailed);
  }

  return base::unexpected(PaymentsWindowManager::Vcn3dsAuthenticationResult::
                              kAuthenticationNotCompleted);
}

PaymentsNetworkInterface::UnmaskRequestDetails
CreateUnmaskRequestDetailsForVcn3ds(
    AutofillClient& client,
    const PaymentsWindowManager::Vcn3dsContext& context,
    PaymentsWindowManager::RedirectCompletionResult
        redirect_completion_result) {
  payments::PaymentsNetworkInterface::UnmaskRequestDetails request_details;
  request_details.card = context.card;
  request_details.billing_customer_number = GetBillingCustomerId(
      &client.GetPersonalDataManager()->payments_data_manager());
  request_details.risk_data = context.risk_data;
  request_details.context_token = context.context_token;

  if (const url::Origin& origin =
          client.GetLastCommittedPrimaryMainFrameOrigin();
      !origin.opaque()) {
    request_details.last_committed_primary_main_frame_origin = origin.GetURL();
  }

  request_details.selected_challenge_option = context.challenge_option;
  request_details.redirect_completion_result =
      std::move(redirect_completion_result);
  return request_details;
}

PaymentsWindowManager::Vcn3dsAuthenticationResponse
CreateVcn3dsAuthenticationResponseFromServerResult(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const PaymentsNetworkInterface::UnmaskResponseDetails& response_details,
    CreditCard card) {
  PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  if (result == PaymentsAutofillClient::PaymentsRpcResult::kSuccess) {
    response.result =
        PaymentsWindowManager::Vcn3dsAuthenticationResult::kSuccess;
    card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
    card.SetExpirationMonthFromString(
        base::UTF8ToUTF16(response_details.expiration_month),
        /*app_locale=*/std::string());
    card.SetExpirationYearFromString(
        base::UTF8ToUTF16(response_details.expiration_year));
    card.set_cvc(base::UTF8ToUTF16(response_details.dcvv));
    response.card = std::move(card);
  } else {
    response.result = PaymentsWindowManager::Vcn3dsAuthenticationResult::
        kAuthenticationFailed;
  }
  return response;
}

}  // namespace autofill::payments
