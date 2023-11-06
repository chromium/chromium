// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {

class CreditCard;

// Authenticates credit card unmasking through risk-based authentication. This
// authenticator is owned by AutofillClient, and will exist per tab on Chrome.
class CreditCardRiskBasedAuthenticator {
 public:
  struct RiskBasedAuthenticationResponse {
    RiskBasedAuthenticationResponse();
    ~RiskBasedAuthenticationResponse();

    RiskBasedAuthenticationResponse& with_did_succeed(bool b) {
      did_succeed = b;
      return *this;
    }
    RiskBasedAuthenticationResponse& with_card(CreditCard c) {
      card = c;
      return *this;
    }

    // Whether the RPC call was successful.
    bool did_succeed = false;
    // The `error_dialog_context` will be set if the RPC call fails, and is used
    // to render the error dialog in CreditCardAccessManager.
    AutofillErrorDialogContext error_dialog_context;
    // The card will be set when the server response was successful and the
    // card's real pan was returned from the server side.
    absl::optional<CreditCard> card;
    // The items below will be set when the server response was successful and
    // the card's real pan was not returned from the server side.
    // FIDO request options will be present only when FIDO is available.
    absl::optional<base::Value::Dict> fido_request_options;
    // Challenge options returned by the server side for further authentication.
    std::vector<CardUnmaskChallengeOption> card_unmask_challenge_options;
    // Stores the latest version of the context token, passed between Payments
    // calls and unmodified by Chrome.
    std::string context_token;
  };

  class Requester {
   public:
    virtual ~Requester() = default;
    virtual void OnRiskBasedAuthenticationResponseReceived(
        const RiskBasedAuthenticationResponse& response) = 0;
    // Callback function invoked when an unmask response for a virtual card has
    // been received.
    // TODO(crbug.com/1487282): Merge virtual card authentication response
    // handling logic with OnRiskBasedAuthenticationResponseReceived().
    virtual void OnVirtualCardRiskBasedAuthenticationResponseReceived(
        AutofillClient::PaymentsRpcResult result,
        payments::PaymentsClient::UnmaskResponseDetails& response_details) = 0;
  };

  explicit CreditCardRiskBasedAuthenticator(AutofillClient* client);
  CreditCardRiskBasedAuthenticator(const CreditCardRiskBasedAuthenticator&) =
      delete;
  CreditCardRiskBasedAuthenticator& operator=(
      const CreditCardRiskBasedAuthenticator&) = delete;
  virtual ~CreditCardRiskBasedAuthenticator();

  // Invokes authentication flow. Responds to `requester` with full pan or
  // necessary field for further authentication.
  //
  // Does not support concurrent calls. Once called, Authenticate must not be
  // called again until Requestor::OnRiskBasedAuthenticationResponseReceived has
  // been triggered for this `requester`.
  virtual void Authenticate(CreditCard card,
                            base::WeakPtr<Requester> requester);

  void OnUnmaskResponseReceivedForTesting(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskResponseDetails& response_details) {
    OnUnmaskResponseReceived(result, response_details);
  }

 private:
  // Callback function invoked when risk data is fetched.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Callback function invoked when an unmask response has been received.
  void OnUnmaskResponseReceived(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskResponseDetails& response_details);

  // Reset the authenticator to its initial state.
  virtual void Reset();

  // The associated autofill client.
  const raw_ref<AutofillClient> autofill_client_;

  // Card being unmasked.
  CreditCard card_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // This contains the details of the card unmask request to be sent to the
  // server.
  std::unique_ptr<payments::PaymentsClient::UnmaskRequestDetails>
      unmask_request_details_;

  base::WeakPtrFactory<CreditCardRiskBasedAuthenticator> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
