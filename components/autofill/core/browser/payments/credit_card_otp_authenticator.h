// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/otp_unmask_delegate.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"

namespace autofill {

class AutofillClient;

// TODO(crbug.com/40186650): Extract common functions to a parent class after
// full card request is removed from the flow.
// Authenticates credit card unmasking through OTP (One-Time Password)
// verification.
class CreditCardOtpAuthenticator : public OtpUnmaskDelegate {
 public:
  struct OtpAuthenticationResponse {
    OtpAuthenticationResponse();
    ~OtpAuthenticationResponse();

    enum Result {
      kUnknown = 0,
      // The OTP authentication succeeded.
      kSuccess = 1,
      // The OTP authentication was cancelled.
      kFlowCancelled = 2,
      // The OTP authentication failed due to unexpected generic errors.
      kGenericError = 3,
      // The OTP authentication failed due to auth errors.
      kAuthenticationError = 4,
      // The OTP authentication failed due to virtual card retrieval errors.
      // Only applicable for virtual cards.
      kVirtualCardRetrievalError = 5,
    };

    OtpAuthenticationResponse& with_result(const Result& r) {
      result = r;
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
    Result result = kUnknown;
    raw_ptr<const CreditCard> card;
    // TODO(crbug.com/40927733): Remove CVC.
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

  // OtpUnmaskDelegate:
  void OnUnmaskPromptAccepted(const std::u16string& otp) override;
  void OnUnmaskPromptClosed(bool user_closed_dialog) override;
  void OnNewOtpRequested() override;

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

  // Have PaymentsNetworkInterface send a SelectChallengeOptionRequest. This
  // will also be invoked when user requests to get a new OTP code. The
  // response's callback function is |OnDidSelectChallengeOption()| when server
  // response returns.
  void SendSelectChallengeOptionRequest();

  // Callback function invoked when the client receives the select challenge
  // option response from the server. Updates locally-cached |context_token_| to
  // the latest version. On a success, this will trigger the otp dialog by
  // calling |ShowOtpDialog()|. If server returns error, show the error dialog
  // and end session.
  void OnDidSelectChallengeOption(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const std::string& context_token);

  // Callback function invoked when the client receives a response from the
  // server. Updates locally-cached |context_token_| to the latest version. If
  // the request was successful, dismiss the UI and pass the full card
  // information to the CreditCardAccessManager, otherwise update the UI to show
  // the correct error message and end the session.
  void OnDidGetRealPan(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      const payments::PaymentsNetworkInterface::UnmaskResponseDetails&
          response_details);

  // Reset the authenticator to initial states.
  virtual void Reset();

  std::string ContextTokenForTesting() const { return context_token_; }

 private:
  // Display the OTP dialog UI.
  // Once user confirms the OTP, we wil invoke |OnUnmaskPromptAccepted(otp)|.
  // If user asks for a new OTP code, we will invoke
  // |SendSelectChallengeOptionRequest()| again.
  void ShowOtpDialog();

  // Invoked when risk data is fetched.
  void OnDidGetUnmaskRiskData(const std::string& risk_data);

  // Have PaymentsNetworkInterface send a UnmaskCardRequest for this card. The
  // response's callback function is |OnDidGetRealPan()|.
  void SendUnmaskCardRequest();

  // Card being unmasked.
  raw_ptr<const CreditCard> card_;

  // User-entered OTP value.
  std::u16string otp_;

  // User-selected challenge option.
  CardUnmaskChallengeOption selected_challenge_option_;

  // Stores the latest version of the context token, passed between Payments
  // calls and unmodified by Chrome.
  std::string context_token_;

  std::string risk_data_;

  int64_t billing_customer_number_;

  // Whether there is a SelectChallengeOption request ongoing.
  bool selected_challenge_option_request_ongoing_ = false;

  // Whether user clicked the link to request a new OTP code.
  bool new_otp_requested_ = false;

  // AutofillClient that owns `this`.
  const raw_ref<AutofillClient> autofill_client_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // This contains the details of the SelectChallengeOption request to be sent
  // to the server.
  std::unique_ptr<
      payments::PaymentsNetworkInterface::SelectChallengeOptionRequestDetails>
      select_challenge_option_request_;

  // This contains the details of the Unmask request to be sent to the server.
  std::unique_ptr<payments::PaymentsNetworkInterface::UnmaskRequestDetails>
      unmask_request_;

  // The timestamps when the requests are sent. Used for logging.
  std::optional<base::TimeTicks> select_challenge_option_request_timestamp_;
  std::optional<base::TimeTicks> unmask_card_request_timestamp_;

  base::WeakPtrFactory<CreditCardOtpAuthenticator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_OTP_AUTHENTICATOR_H_
