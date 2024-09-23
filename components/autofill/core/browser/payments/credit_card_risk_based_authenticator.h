// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_

#include <memory>
#include <string>

#include "components/autofill/core/browser/payments/autofill_error_dialog_context.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"

namespace autofill {

class AutofillClient;
class CreditCard;

// Authenticates credit card unmasking through risk-based authentication. This
// authenticator is owned by AutofillClient, and will exist per tab on Chrome.
class CreditCardRiskBasedAuthenticator {
 public:
  struct RiskBasedAuthenticationResponse {
    RiskBasedAuthenticationResponse();
    RiskBasedAuthenticationResponse& operator=(
        const RiskBasedAuthenticationResponse& other);
    ~RiskBasedAuthenticationResponse();

    // The outcome of the risk-based authentication.
    enum class Result {
      // Default value, should never be used.
      kUnknown = 0,
      // No further authentication is required. Also known as green path.
      kNoAuthenticationRequired = 1,
      // The user needs to complete further authentication to retrieve the card.
      // Also known as yellow path.
      kAuthenticationRequired = 2,
      // The authentication has been cancelled.
      kAuthenticationCancelled = 3,
      // The authentication failed. Also known as red path.
      kError = 4,
      kMaxValue = kError,
    };

    RiskBasedAuthenticationResponse& with_result(Result r) {
      result = r;
      return *this;
    }
    RiskBasedAuthenticationResponse& with_card(CreditCard c) {
      card = std::move(c);
      return *this;
    }
    RiskBasedAuthenticationResponse& with_fido_request_options(
        base::Value::Dict v) {
      fido_request_options = std::move(v);
      return *this;
    }
    RiskBasedAuthenticationResponse& with_context_token(std::string s) {
      context_token = std::move(s);
      return *this;
    }

    // The `result` will be used to notify requesters of the outcome of the
    // risk-based authentication.
    Result result = Result::kUnknown;
    // The `error_dialog_context` will be set if the RPC call fails, and is used
    // to render the error dialog in CreditCardAccessManager.
    AutofillErrorDialogContext error_dialog_context;
    // The card will be set when the server response was successful and the
    // card's real pan was returned from the server side.
    std::optional<CreditCard> card;
    // The items below will be set when the server response was successful and
    // the card's real pan was not returned from the server side.
    // FIDO request options will be present only when FIDO is available.
    base::Value::Dict fido_request_options;
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
    // TODO(crbug.com/40934051): Merge virtual card authentication response
    // handling logic with OnRiskBasedAuthenticationResponseReceived().
    virtual void OnVirtualCardRiskBasedAuthenticationResponseReceived(
        payments::PaymentsAutofillClient::PaymentsRpcResult result,
        const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
            response_details) = 0;
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

  // Callback function invoked when an unmask response has been cancelled.
  void OnUnmaskCancelled();

  base::WeakPtr<CreditCardRiskBasedAuthenticator> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void OnUnmaskResponseReceivedForTesting(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
          response_details) {
    OnUnmaskResponseReceived(result, response_details);
  }

 private:
  // Callback function invoked when risk data is fetched.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Callback function invoked when an unmask response has been received.
  void OnUnmaskResponseReceived(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
          response_details);

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
  std::unique_ptr<payments::PaymentsNetworkInterface::UnmaskRequestDetails>
      unmask_request_details_;

  // The timestamp when the unmask request is sent. Used for logging.
  std::optional<base::TimeTicks> unmask_card_request_timestamp_;

  base::WeakPtrFactory<CreditCardRiskBasedAuthenticator> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_RISK_BASED_AUTHENTICATOR_H_
