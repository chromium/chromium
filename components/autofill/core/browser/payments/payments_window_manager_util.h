// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"

namespace autofill {

class AutofillClient;

namespace payments {

// Parses the URL for VCN 3DS, which is set in `url`. `metadata` contains the
// required query parameter information to search for in `url`.  If the parsed
// URL denotes the authentication completed successfully, this function will
// return a PaymentsWindowManager::RedirectCompletionResult as the expected
// response. Otherwise this function will return the non-success result.
base::expected<PaymentsWindowManager::RedirectCompletionResult,
               PaymentsWindowManager::Vcn3dsAuthenticationResult>
ParseUrlForVcn3ds(const GURL& url,
                  const Vcn3dsChallengeOptionMetadata& metadata);

// Creates UnmaskRequestDetails specific to VCN 3DS. `client` is the
// AutofillClient associated with the original browser window. `context` is the
// context that was set when the flow was initialized, and
// `redirect_completion_result` is the token that was parsed from the query
// parameters in the final redirect of the pop-up. Refer to
// ParseFinalUrlForVcn3ds() for when `redirect_completion_result` is set.
PaymentsNetworkInterface::UnmaskRequestDetails
CreateUnmaskRequestDetailsForVcn3ds(
    AutofillClient& client,
    const PaymentsWindowManager::Vcn3dsContext& context,
    PaymentsWindowManager::RedirectCompletionResult redirect_completion_result);

// Creates the Vcn3dsAuthenticationResponse for the response from the
// UnmaskCardRequest that was sent during the VCN 3DS authentication.
PaymentsWindowManager::Vcn3dsAuthenticationResponse
CreateVcn3dsAuthenticationResponseFromServerResult(
    PaymentsAutofillClient::PaymentsRpcResult result,
    const PaymentsNetworkInterface::UnmaskResponseDetails& response_details,
    CreditCard card);

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_
