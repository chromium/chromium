// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
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

  // The current status inside the BNPL pop-up.
  enum class BnplPopupStatus {
    // The user successfully finished the flow inside of the pop-up.
    kSuccess = 0,
    // The user failed the flow inside of the pop-up.
    kFailure = 1,
    // The user has not yet finished the flow inside of the pop-up.
    kNotFinished = 2,
  };

  // The result of the BNPL flow, which will be sent to the caller that
  // initiated the flow.
  //
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange(BnplFlowResult)
  enum class BnplFlowResult {
    // The BNPL flow was successful.
    kSuccess = 0,
    // The BNPL flow failed.
    kFailure = 1,
    // The user closed the pop-up which ended the BNPL flow.
    kUserClosed = 2,

    kMaxValue = kUserClosed,
  };
  // LINT.ThenChange(/tools/metrics/histograms/metadata/autofill/enums.xml:BnplPopupWindowResult)

  using OnVcn3dsAuthenticationCompleteCallback =
      base::OnceCallback<void(Vcn3dsAuthenticationResponse)>;

  using OnBnplPopupClosedCallback =
      base::OnceCallback<void(BnplFlowResult, GURL)>;

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

  // The contextual data required for the BNPL flow.
  struct BnplContext {
    BnplContext();
    BnplContext(BnplContext&&);
    BnplContext& operator=(BnplContext&&);
    ~BnplContext();

    // The ID of the issuer for the BNPL flow.
    autofill::BnplIssuer::IssuerId issuer_id;
    // The starting location of the BNPL flow, which is an initial URL to
    // open inside of the pop-up.
    GURL initial_url;
    // The URL prefix that denotes the user successfully finished the flow
    // inside of the pop-up. This parameter will be used to match against each
    // URL navigation inside of the pop-up, and if the window manager observes
    // `success_url_prefix` inside of the pop-up, it will close the pop-up
    // automatically.
    GURL success_url_prefix;
    // The URL prefix that denotes the user failed the flow inside of the
    // pop-up. This parameter will be used to match against each URL navigation
    // inside of the pop-up, and if the window manager observes the
    // `failure_url_prefix` inside of the pop-up, it will close the pop-up
    // automatically.
    GURL failure_url_prefix;
    // The callback to run to notify the caller that the flow inside of the
    // pop-up was finished, with the result.
    OnBnplPopupClosedCallback completion_callback;
  };

  // Contains the possible flows that this class can support.
  enum class FlowType {
    kNoFlow = 0,
    kVcn3ds = 1,
    kBnpl = 2,
    kMaxValue = kBnpl,
  };

  // Keeps track of the state for the ongoing flow.
  struct FlowState {
    FlowState();
    FlowState(FlowState&&);
    FlowState& operator=(FlowState&&);
    ~FlowState();

    // Only present if `flow_type` is `kVcn3ds`.
    std::optional<Vcn3dsContext> vcn_3ds_context;

    // Only present if `flow_type` is `kBnpl`.
    std::optional<BnplContext> bnpl_context;

    // The timestamp for when the VCN 3DS pop-up was shown to the user. Used for
    // logging purposes. Present if `flow_type` is `kVcn3ds` and a
    // popup was created for the flow.
    std::optional<base::TimeTicks> vcn_3ds_popup_shown_timestamp;

    // The timestamp for when the BNPL payments window popup was shown to the
    // user. Used for logging purposes. Present if `flow_type` is `kBnpl` and a
    // tab popup was created for the flow.
    std::optional<base::TimeTicks> bnpl_popup_shown_timestamp;

    // Set on every navigation inside of the observed tab. Used on tab
    // destruction to understand the reason for destruction, and to notify the
    // caller. This member is required because at the point where the most
    // recent URL navigation needs to be known, accessing the observed web
    // contents is unsafe. Thus it is preferred to cache this earlier and read
    // from it when needed.
    GURL most_recent_url_navigation;

    // The type of flow that is currently ongoing. Set when a flow is initiated.
    FlowType flow_type = FlowType::kNoFlow;
  };

  virtual ~PaymentsWindowManager() = default;

  // Initiates the VCN 3DS auth flow. All fields in `context` must be valid and
  // non-empty.
  virtual void InitVcn3dsAuthentication(Vcn3dsContext context) = 0;

  // Initiates the BNPL flow. All fields in `context` must be valid and
  // non-empty.
  virtual void InitBnplFlow(BnplContext context) = 0;
};

}  // namespace autofill::payments

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_PAYMENTS_WINDOW_MANAGER_H_
