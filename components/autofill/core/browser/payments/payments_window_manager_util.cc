// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/payments_window_manager_util.h"

#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_util.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill::payments {

base::expected<PaymentsWindowManager::RedirectCompletionProof,
               PaymentsWindowManager::Vcn3dsAuthenticationPopupNonSuccessResult>
ParseUrlForVcn3ds(const GURL& url) {
  std::optional<bool> should_proceed;
  std::string redirect_completion_proof;
  std::string_view query_piece = url.query_piece();
  url::Component query(0, query_piece.length());
  url::Component key;
  url::Component value;
  while (url::ExtractQueryKeyValue(query_piece, &query, &key, &value)) {
    std::string_view key_view = query_piece.substr(key.begin, key.len);
    std::string_view value_view = query_piece.substr(value.begin, value.len);
    if (key_view == "shouldProceed") {
      should_proceed = value_view == "true";
    } else if (key_view == "token") {
      redirect_completion_proof = std::string(value_view);
    }
  }

  // `should_proceed` being present, having a value of true, and there being a
  // `redirect_completion_proof` present indicates the user completed the
  // authentication and a request to the Payments servers is required to
  // retrieve the authentication result.
  if (should_proceed.value_or(false) && !redirect_completion_proof.empty()) {
    return base::ok(PaymentsWindowManager::RedirectCompletionProof(
        redirect_completion_proof));
  }

  // `should_proceed` being present and having a value of false is the Google
  // Payments server's way of telling Chrome that the authentication failed.
  if (!should_proceed.value_or(true)) {
    return base::unexpected(
        PaymentsWindowManager::Vcn3dsAuthenticationPopupNonSuccessResult::
            kAuthenticationFailed);
  }

  return base::unexpected(
      PaymentsWindowManager::Vcn3dsAuthenticationPopupNonSuccessResult::
          kAuthenticationNotCompleted);
}

PaymentsNetworkInterface::UnmaskRequestDetails
CreateUnmaskRequestDetailsForVcn3ds(
    AutofillClient& client,
    const PaymentsWindowManager::Vcn3dsContext& context,
    PaymentsWindowManager::RedirectCompletionProof redirect_completion_proof) {
  payments::PaymentsNetworkInterface::UnmaskRequestDetails request_details;
  request_details.card = context.card;
  request_details.billing_customer_number =
      GetBillingCustomerId(client.GetPersonalDataManager());
  request_details.context_token = context.context_token;

  if (const url::Origin& origin =
          client.GetLastCommittedPrimaryMainFrameOrigin();
      !origin.opaque()) {
    request_details.last_committed_primary_main_frame_origin = origin.GetURL();
  }

  if (!client.IsOffTheRecord()) {
    request_details.merchant_domain_for_footprints =
        client.GetLastCommittedPrimaryMainFrameOrigin();
  }

  request_details.selected_challenge_option = context.challenge_option;
  request_details.redirect_completion_proof =
      std::move(redirect_completion_proof);
  return request_details;
}

PaymentsWindowManager::Vcn3dsAuthenticationResponse
CreateVcn3dsAuthenticationResponse(
    AutofillClient::PaymentsRpcResult result,
    const PaymentsNetworkInterface::UnmaskResponseDetails& response_details,
    CreditCard card) {
  PaymentsWindowManager::Vcn3dsAuthenticationResponse response;
  if (result == AutofillClient::PaymentsRpcResult::kSuccess) {
    card.SetNumber(base::UTF8ToUTF16(response_details.real_pan));
    card.SetExpirationMonthFromString(
        base::UTF8ToUTF16(response_details.expiration_month),
        /*app_locale=*/std::string());
    card.SetExpirationYearFromString(
        base::UTF8ToUTF16(response_details.expiration_year));
    card.set_cvc(base::UTF8ToUTF16(response_details.dcvv));
    response.card = std::move(card);
  }
  return response;
}

}  // namespace autofill::payments
