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
  using RedirectCompletionProof =
      base::StrongAlias<class RedirectCompletionProofTag, std::string>;

  // The response fields for a VCN 3DS authentication, created once a response
  // to the second UnmaskCardRequest has been received.
  struct Vcn3dsAuthenticationResponse {
    Vcn3dsAuthenticationResponse();
    Vcn3dsAuthenticationResponse(const Vcn3dsAuthenticationResponse&);
    Vcn3dsAuthenticationResponse(Vcn3dsAuthenticationResponse&&);
    Vcn3dsAuthenticationResponse& operator=(
        const Vcn3dsAuthenticationResponse&);
    Vcn3dsAuthenticationResponse& operator=(Vcn3dsAuthenticationResponse&&);
    ~Vcn3dsAuthenticationResponse();

    // CreditCard representation of the data returned in the response of the
    // UnmaskCardRequest after a VCN 3DS authentication has completed. The
    // response is a success if `card` is present, it is a failure otherwise.
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

  // The result of the 3DS authentication inside of the pop-up if it was not a
  // success.
  enum class Vcn3dsAuthenticationPopupNonSuccessResult {
    // The authentication inside of the 3DS pop-up was a failure. The reason for
    // the failure is unknown to Chrome, and can be due to any of several
    // possible reasons. Some reasons can be that the user failed to
    // authenticate, or there is a server error.
    kAuthenticationFailed = 0,
    // The authentication inside of the 3DS pop-up did not complete. This occurs
    // if the user closes the pop-up before finishing the authentication, and
    // there are no query params.
    kAuthenticationNotCompleted = 1,
  };

  virtual ~PaymentsWindowManager() = default;

  // Initiates the VCN 3DS auth flow. All fields in `context` must be valid and
  // non-empty.
  virtual void InitVcn3dsAuthentication(Vcn3dsContext context) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
