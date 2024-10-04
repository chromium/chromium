// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_

#include <memory>
#include <optional>
#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/waitable_event.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_driver.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/payments_network_interface.h"
#include "components/autofill/core/browser/strike_databases/payments/fido_authentication_strike_database.h"
#include "components/webauthn/core/browser/internal_authenticator.h"
#include "device/fido/fido_constants.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom-forward.h"

namespace autofill {

class AutofillClient;

// Enum denotes user's intention to opt in/out.
enum class UserOptInIntention {
  // Unspecified intention. No pref mismatch.
  kUnspecified = 0,
  // Only used for Android settings page. Local pref is opted in but Payments
  // considers the user not opted-in.
  kIntentToOptIn = 1,
  // User intends to opt out, happens when user opted out from settings page on
  // Android, or opt-out call failed on Desktop.
  kIntentToOptOut = 2,
};

// Authenticates credit card unmasking through FIDO authentication, using the
// WebAuthn specification, standardized by the FIDO alliance. The Webauthn
// specification defines an API to cryptographically bind a server and client,
// and verify that binding. More information can be found here:
// - https://www.w3.org/TR/webauthn-1/
// - https://fidoalliance.org/fido2/
class CreditCardFidoAuthenticator
    : public payments::FullCardRequest::ResultDelegate {
 public:
  // Useful for splitting metrics to correct sub-histograms and knowing which
  // Payments RPC's to send.
  enum Flow {
    // No flow is in progress.
    NONE_FLOW,
    // Authentication flow.
    AUTHENTICATION_FLOW,
    // Registration flow, including a challenge to sign.
    OPT_IN_WITH_CHALLENGE_FLOW,
    // Opt-in attempt flow, no challenge to sign.
    OPT_IN_FETCH_CHALLENGE_FLOW,
    // Opt-out flow.
    OPT_OUT_FLOW,
    // Authorization of a new card.
    FOLLOWUP_AFTER_CVC_AUTH_FLOW,
  };
  // The response of FIDO authentication, including necessary information needed
  // by the subclasses.
  struct FidoAuthenticationResponse {
    // Whether the authentication was successful.
    bool did_succeed = false;
    // The fetched credit card if the authentication was successful. Can be
    // nullptr if authentication failed.
    raw_ptr<const CreditCard> card = nullptr;
    // The CVC of the fetched credit card. Can be empty string.
    std::u16string cvc = std::u16string();
    // The type of the failure of the full card request.
    payments::FullCardRequest::FailureType failure_type =
        payments::FullCardRequest::UNKNOWN;
  };
  class Requester {
   public:
    virtual ~Requester() = default;
    virtual void OnFIDOAuthenticationComplete(
        const FidoAuthenticationResponse& response) = 0;
    virtual void OnFidoAuthorizationComplete(bool did_succeed) = 0;
  };
  CreditCardFidoAuthenticator(AutofillDriver* driver, AutofillClient* client);

  CreditCardFidoAuthenticator(const CreditCardFidoAuthenticator&) = delete;
  CreditCardFidoAuthenticator& operator=(const CreditCardFidoAuthenticator&) =
      delete;

  ~CreditCardFidoAuthenticator() override;

  // Invokes Authentication flow. Responds to |accessor_| with full pan.
  // |context_token| is used to share context between different requests. It
  // will be populated only for virtual card unmasking.
  virtual void Authenticate(
      CreditCard card,
      base::WeakPtr<Requester> requester,
      base::Value::Dict request_options,
      std::optional<std::string> context_token = std::nullopt);

  // Invokes Registration flow. Sends credentials created from
  // |creation_options| along with the |card_authorization_token| to Payments in
  // order to enroll the user and authorize the corresponding card.
  void Register(std::string card_authorization_token = std::string(),
                base::Value::Dict creation_options = base::Value::Dict());

  // Invokes an Authorization flow. Sends signature created from
  // |request_options| along with the |card_authorization_token| to Payments in
  // order to authorize the corresponding card. Notifies |requester| once
  // Authorization is complete.
  void Authorize(base::WeakPtr<Requester> requester,
                 std::string card_authorization_token,
                 base::Value::Dict request_options);

  // Opts the user out.
  virtual void OptOut();

  // Invokes callback with true if user has a verifying platform authenticator.
  // e.g. Touch/Face ID, Windows Hello, Android fingerprint, etc., is available
  // and enabled. Otherwise invokes callback with false.
  virtual void IsUserVerifiable(base::OnceCallback<void(bool)> callback);

  // Returns true only if the user has opted-in to use WebAuthn for autofill.
  virtual bool IsUserOptedIn();

  // Return user's opt in/out intention based on unmask detail response and
  // local pref.
  UserOptInIntention GetUserOptInIntention(
      payments::PaymentsNetworkInterface::UnmaskDetails& unmask_details);

  // Cancel the ongoing verification process. Used to reset states in this class
  // and in the FullCardRequest if any.
  void CancelVerification();

#if !BUILDFLAG(IS_ANDROID)
  // Invoked when a Webauthn offer dialog is about to be shown.
  void OnWebauthnOfferDialogRequested(std::string card_authorization_token);

  // Invoked when the WebAuthn offer dialog is accepted or declined/cancelled.
  void OnWebauthnOfferDialogUserResponse(bool did_accept);
#endif

  // Retrieves the strike database for offering FIDO authentication. This can
  // return nullptr so check before using.
  FidoAuthenticationStrikeDatabase*
  GetOrCreateFidoAuthenticationStrikeDatabase();

  // Returns the current flow.
  Flow current_flow() { return current_flow_; }

  // Returns true if `request_options` contains a challenge and has a non-empty
  // list of keys that each have a Credential ID.
  bool IsValidRequestOptions(const base::Value::Dict& request_options);

 private:
  friend class BrowserAutofillManagerTest;
  friend class CreditCardAccessManagerTestBase;
  friend class CreditCardFidoAuthenticatorTest;
  friend class TestCreditCardFidoAuthenticator;
  FRIEND_TEST_ALL_PREFIXES(CreditCardFidoAuthenticatorTest,
                           ParseRequestOptions);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFidoAuthenticatorTest,
                           ParseAssertionResponse);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFidoAuthenticatorTest,
                           ParseCreationOptions);
  FRIEND_TEST_ALL_PREFIXES(CreditCardFidoAuthenticatorTest,
                           ParseAttestationResponse);

  // Invokes the WebAuthn prompt to request user verification to sign the
  // challenge in |request_options|.
  virtual void GetAssertion(
      blink::mojom::PublicKeyCredentialRequestOptionsPtr request_options);

  // Invokes the WebAuthn prompt to request user verification to sign the
  // challenge in |creation_options| and create a key-pair.
  virtual void MakeCredential(
      blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options);

  // Makes a request to payments to either opt-in or opt-out the user.
  // TODO(crbug.com/345006413): Remove logic related to the FIDO opt-out flow.
  void OptChange(
      base::Value::Dict authenticator_response = base::Value::Dict());

  // The callback invoked from the WebAuthn prompt including the
  // |assertion_response|, which will be sent to Google Payments to retrieve
  // card details.
  void OnDidGetAssertion(
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details);

  // The callback invoked from the WebAuthn prompt including the
  // |attestation_response|, which will be sent to Google Payments to enroll the
  // credential for this user.
  void OnDidMakeCredential(
      blink::mojom::AuthenticatorStatus status,
      blink::mojom::MakeCredentialAuthenticatorResponsePtr attestation_response,
      blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details);

  // Sets prefstore to enable credit card authentication if rpc was successful.
  void OnDidGetOptChangeResult(
      payments::PaymentsAutofillClient::PaymentsRpcResult result,
      payments::PaymentsNetworkInterface::OptChangeResponseDetails& response);

  // payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const payments::FullCardRequest& full_card_request,
      const CreditCard& card,
      const std::u16string& cvc) override;
  void OnFullCardRequestFailed(
      CreditCard::RecordType card_type,
      payments::FullCardRequest::FailureType failure_type) override;

  // Converts |request_options| from JSON to mojom pointer.
  blink::mojom::PublicKeyCredentialRequestOptionsPtr ParseRequestOptions(
      const base::Value::Dict& request_options);

  // Converts |creation_options| from JSON to mojom pointer.
  blink::mojom::PublicKeyCredentialCreationOptionsPtr ParseCreationOptions(
      const base::Value::Dict& creation_options);

  // Helper function to parse |key_info| sub-dictionary found in
  // |request_options| and |creation_options|.
  device::PublicKeyCredentialDescriptor ParseCredentialDescriptor(
      const base::Value& key_info);

  // Converts |assertion_response| from mojom pointer to JSON.
  base::Value::Dict ParseAssertionResponse(
      blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response);

  // Converts |attestation_response| from mojom pointer to JSON.
  base::Value::Dict ParseAttestationResponse(
      blink::mojom::MakeCredentialAuthenticatorResponsePtr
          attestation_response);

  // Returns true if |request_options| contains a challenge.
  bool IsValidCreationOptions(const base::Value::Dict& creation_options);

  // Logs the result of a WebAuthn prompt.
  void LogWebauthnResult(blink::mojom::AuthenticatorStatus status);

  // Updates the user preference to the value of |user_is_opted_in_|.
  void UpdateUserPref();

  // Helper functions to handle the GetAssertion result.
  void HandleGetAssertionSuccess(
      blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response);
  void HandleGetAssertionFailure();

  // Gets or creates Authenticator pointer to facilitate WebAuthn.
  webauthn::InternalAuthenticator* authenticator();

  // Card being unmasked.
  std::optional<CreditCard> card_;

  // The current flow in progress.
  Flow current_flow_ = NONE_FLOW;

  // Token used for authorizing new cards. Helps tie CVC auth and FIDO calls
  // together in order to support FIDO-only unmasking on future attempts.
  std::string card_authorization_token_;

  // The associated autofill driver. Weak reference.
  const raw_ptr<AutofillDriver> autofill_driver_;

  // The associated autofill client. Weak reference.
  const raw_ptr<AutofillClient> autofill_client_;

  // Interface to make HTTP-based requests to Google Payments.
  const raw_ptr<payments::PaymentsNetworkInterface> payments_network_interface_;

  // Authenticator pointer to facilitate WebAuthn.
  std::unique_ptr<webauthn::InternalAuthenticator> authenticator_;

  // Responsible for getting the full card details, including the PAN and the
  // CVC.
  std::unique_ptr<payments::FullCardRequest> full_card_request_;

  // Weak pointer to object that is requesting authentication.
  base::WeakPtr<Requester> requester_;

  // Is set to true when user is opted-in, else false. This value will always
  // override the value in the pref store in the case of any discrepancies.
  bool user_is_opted_in_;

  // Strike database to ensure we limit the number of times we offer fido
  // authentication.
  std::unique_ptr<FidoAuthenticationStrikeDatabase>
      fido_authentication_strike_database_;

  // Signaled when callback for IsUserVerifiable() is invoked.
  base::WaitableEvent user_is_verifiable_callback_received_;

  // The context token used for sharing context between different server
  // requests. Will be populated only for virtual card unmasking.
  std::optional<std::string> context_token_;

  base::WeakPtrFactory<CreditCardFidoAuthenticator> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_PAYMENTS_CREDIT_CARD_FIDO_AUTHENTICATOR_H_
