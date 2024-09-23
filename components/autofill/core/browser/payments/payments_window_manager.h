// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "url/gurl.h"

namespace autofill::payments {

// Interface for objects that manage popup-related redirect flows for payments
// autofill, with different implementations meant to handle different operating
// systems.
class PaymentsWindowManager {
 public:
  using RedirectCompletionResult =
      base::StrongAlias<class RedirectCompletionResultTag, std::string>;

  // The result of the VCN 3DS authentication.
  enum class Vcn3dsAuthenticationResult {
    // The authentication was a success.
    kSuccess = 0,
    // The authentication was a failure. If the authentication failed inside of
    // the pop-up, the reason for the failure is unknown to Chrome, and can be
    // due to any of several possible reasons. Some reasons can be that the user
    // failed to authenticate, or there is a server error. This can also mean
    // the authentication failed during the Payments server call to retrieve the
    // card after the pop-up has closed.
    kAuthenticationFailed = 1,
    // The authentication did not complete. This occurs if the user closes the
    // pop-up before finishing the authentication, and there are no query
    // params. This can also occur if the user cancels any of the dialogs during
    // the flow.
    kAuthenticationNotCompleted = 2,
  };

  // The response fields for a VCN 3DS authentication, created once the flow is
  // complete and a response to the caller is required.
  struct Vcn3dsAuthenticationResponse {
    Vcn3dsAuthenticationResponse();
    Vcn3dsAuthenticationResponse(const Vcn3dsAuthenticationResponse&);
    Vcn3dsAuthenticationResponse(Vcn3dsAuthenticationResponse&&);
    Vcn3dsAuthenticationResponse& operator=(
        const Vcn3dsAuthenticationResponse&);
    Vcn3dsAuthenticationResponse& operator=(Vcn3dsAuthenticationResponse&&);
    ~Vcn3dsAuthenticationResponse();

    // The result of the VCN 3DS authentication.
    Vcn3dsAuthenticationResult result;

    // CreditCard representation of the data returned in the response of the
    // UnmaskCardRequest after a VCN 3DS authentication has completed. Only
    // present if `result` is a success.
    std::optional<CreditCard> card;
  };

  using OnVcn3dsAuthenticationCompleteCallback =
      base::OnceCallback<void(Vcn3dsAuthenticationResponse)>;

  // The contextual data required for the VCN 3DS flow.
  struct Vcn3dsContext {
    Vcn3dsContext();
    Vcn3dsContext(Vcn3dsContext&&);
    Vcn3dsContext& operator=(Vcn3dsContext&&);
    ~Vcn3dsContext();

    // The virtual card that is currently being authenticated with a VCN 3DS
    // authentication flow.
    CreditCard card;
    // The context token that was returned from the Payments Server for the
    // ongoing VCN authentication flow.
    std::string context_token;
    // The risk data that must be sent to the Payments Server during a VCN 3DS
    // card unmask request.
    std::string risk_data;
    // The challenge option that was returned from the server which contains
    // details required for the VCN 3DS authentication flow.
    CardUnmaskChallengeOption challenge_option;
    // Callback that will be run when the VCN 3DS authentication completed.
    OnVcn3dsAuthenticationCompleteCallback completion_callback;
    // Boolean that denotes whether the user already provided consent for the
    // VCN 3DS authentication pop-up. If false, user consent must be achieved
    // before triggering a VCN 3DS authentication pop-up.
    bool user_consent_already_given = false;
  };

  virtual ~PaymentsWindowManager() = default;

  // Initiates the VCN 3DS auth flow. All fields in `context` must be valid and
  // non-empty.
  virtual void InitVcn3dsAuthentication(Vcn3dsContext context) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
