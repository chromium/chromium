// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"

namespace autofill {

// TODO(crbug.com/1220990): Extract common functions to a parent class after
// full card request is removed from the flow.
// Authenticates credit card unmasking through OTP (One-Time Password)
// verification.
class CreditCardOtpAuthenticator {
 public:
  struct OtpAuthenticationResponse {
    OtpAuthenticationResponse();
    ~OtpAuthenticationResponse();

    OtpAuthenticationResponse& with_did_succeed(bool b) {
      did_succeed = b;
      return *this;
    }

    // The credit card object must outlive |this|.
    OtpAuthenticationResponse& with_card(const CreditCard* c) {
      card = c;
      return *this;
    }
    OtpAuthenticationResponse& with_cvc(const std::u16string s) {
      cvc = std::u16string(s);
      return *this;
    }
    bool did_succeed = false;
    const CreditCard* card;
    std::u16string cvc;
  };

  // The requester which delegates the authentication task to |this|.
  class Requester {
   public:
    virtual ~Requester() = default;
    // Invoked when OTP authentication is completed, regardless of whether it
    // succeeded.
    virtual void OnOtpAuthenticationComplete(
        const OtpAuthenticationResponse& response) = 0;
  };

  explicit CreditCardOtpAuthenticator(AutofillClient* client);
  virtual ~CreditCardOtpAuthenticator();
  CreditCardOtpAuthenticator(const CreditCardOtpAuthenticator&) = delete;
  CreditCardOtpAuthenticator& operator=(const CreditCardOtpAuthenticator&) =
      delete;

  // Start the OTP authentication for the |card| with
  // |selected_challenge_option|. Will invoke
  // |SendSelectChallengeOptionRequest()| to send the selected challenge option
  // to server.
  virtual void OnChallengeOptionSelected(
      const CreditCard* card,
      const CardUnmaskChallengeOption& selected_challenge_option,
      base::WeakPtr<Requester> requester,
      const std::string& context_token,
      int64_t billing_customer_number);

  // Have PaymentsClient send a SelectChallengeOptionRequest. This will also be
  // invoked when user requests to get a new OTP code. The response's callback
  // function is |OnDidSelectChallengeOption()| when server response returns.
  void SendSelectChallengeOptionRequest();

  // Callback function invoked when the client receives the select challenge
  // option response from the server. Updates locally-cached |context_token_| to
  // the latest version. On a success, this will trigger the otp dialog by
  // calling |ShowOtpDialog()|. If server returns error, show the error dialog
  // and end session.
  void OnDidSelectChallengeOption(AutofillClient::PaymentsRpcResult result,
                                  const std::string& context_token);

  // Called when the user has attempted an OTP verification. This will prepare
  // unmask request and invoke |SendUnmaskCardRequest()|. Note that this can
  // also be invoked when user retries with a new OTP.
  void OnUnmaskPromptAccepted(const std::u16string& otp);

  // Callback function invoked when the client receives a response from the
  // server. Updates locally-cached |context_token_| to the latest version. If
  // the request was successful, dismiss the UI and pass the full card
  // information to the CreditCardAccessManager, otherwise update the UI to show
  // the correct error message and end the session.
  void OnDidGetRealPan(
      AutofillClient::PaymentsRpcResult result,
      payments::PaymentsClient::UnmaskResponseDetails& response_details);

 private:
  friend class CreditCardOtpAuthenticatorTest;

  // Display the OTP dialog UI.
  // Once user confirms the OTP, we wil invoke |OnUnmaskPromptAccepted(otp)|.
  // If user asks for a new OTP code, we will invoke
  // |SendSelectChallengeOptionRequest()| again.
  void ShowOtpDialog();

  // Invoked when risk data is fetched.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Have PaymentsClient send a UnmaskCardRequest for this card. The response's
  // callback function is |OnDidGetRealPan()|.
  void SendUnmaskCardRequest();

  // Reset the authenticator to initial states.
  void Reset();

  // Card being unmasked.
  const CreditCard* card_;

  // User-entered OTP value.
  std::u16string otp_;

  // User-selected challenge option.
  CardUnmaskChallengeOption selected_challenge_option_;

  // Stores the latest version of the context token, passed between Payments
  // calls and unmodified by Chrome.
  std::string context_token_;

  std::string risk_data_;

  int64_t billing_customer_number_;

  // The associated autofill client.
  AutofillClient* autofill_client_;

  // The associated payments client.
  payments::PaymentsClient* payments_client_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // This contains the details of the SelectChallengeOption request to be sent
  // to the server.
  std::unique_ptr<payments::PaymentsClient::SelectChallengeOptionRequestDetails>
      select_challenge_option_request_;

  // This contains the details of the Unmask request to be sent to the server.
  std::unique_ptr<payments::PaymentsClient::UnmaskRequestDetails>
      unmask_request_;

  base::WeakPtrFactory<CreditCardOtpAuthenticator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_
