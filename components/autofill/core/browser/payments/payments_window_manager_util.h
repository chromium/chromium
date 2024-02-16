// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_

#include "base/functional/callback_forward.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/payments/payments_window_manager.h"

namespace autofill {

class AutofillClient;

namespace payments {

// Parses the final URL for VCN 3DS, which is set in `url`. If the parsed URL
// denotes the authentication completed successfully, this function will return
// a PaymentsWindowManager::RedirectCompletionProof as the expected response.
// Otherwise this function will return the error type.
base::expected<PaymentsWindowManager::RedirectCompletionProof,
               PaymentsWindowManager::Vcn3dsAuthenticationPopupErrorType>
ParseFinalUrlForVcn3ds(const GURL& url);

// Creates UnmaskRequestDetails specific to VCN 3DS. `client` is the
// AutofillClient associated with the original browser window. `context` is the
// context that was set when the flow was initialized, and
// `redirect_completion_proof` is the token that was parsed from the query
// parameters in the final redirect of the pop-up. Refer to
// ParseFinalUrlForVcn3ds() for when `redirect_completion_proof` is set.
PaymentsNetworkInterface::UnmaskRequestDetails
CreateUnmaskRequestDetailsForVcn3ds(
    AutofillClient& client,
    const PaymentsWindowManager::Vcn3dsContext& context,
    PaymentsWindowManager::RedirectCompletionProof redirect_completion_proof);

}  // namespace payments

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_UTIL_H_
