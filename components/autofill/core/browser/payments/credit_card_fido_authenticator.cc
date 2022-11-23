// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif
#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
#include "base/synchronization/waitable_event.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_progress_dialog_type.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/payments/fido_authentication_strike_database.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/payments/payments_service_url.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/fido_types.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {

namespace {
// Default timeout for user to respond to WebAuthn prompt.
constexpr int kWebAuthnTimeoutMs = 3 * 60 * 1000;  // 3 minutes
constexpr char kGooglePaymentsRpid[] = "google.com";
constexpr char kGooglePaymentsRpName[] = "Google Payments";

std::vector<uint8_t> Base64ToBytes(std::string base64) {
  std::string bytes;
  bool did_succeed = base::Base64Decode(base::StringPiece(base64), &bytes);
  if (did_succeed) {
    return std::vector<uint8_t>(bytes.begin(), bytes.end());
  }
  return std::vector<uint8_t>{};
}

base::Value BytesToBase64(const std::vector<uint8_t> bytes) {
  std::string base64;
  base::Base64Encode(std::string(bytes.begin(), bytes.end()), &base64);
  return base::Value(std::move(base64));
}
}  // namespace

CreditCardFIDOAuthenticator::CreditCardFIDOAuthenticator(AutofillDriver* driver,
                                                         AutofillClient* client)
    : autofill_driver_(driver),
      autofill_client_(client),
      payments_client_(client->GetPaymentsClient()),
      user_is_verifiable_callback_received_(
          base::WaitableEvent::ResetPolicy::AUTOMATIC,
          base::WaitableEvent::InitialState::NOT_SIGNALED) {
  user_is_opted_in_ = IsUserOptedIn();
}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {
  UpdateUserPref();
}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::Value request_options,
    absl::optional<std::string> context_token) {
  card_ = card;
  requester_ = requester;
  context_token_ = context_token;

  // Cancel any previous pending WebAuthn requests.
  authenticator()->Cancel();

  if (card_ && IsValidRequestOptions(request_options.Clone())) {
    current_flow_ = AUTHENTICATION_FLOW;
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  } else if (requester_) {
    FidoAuthenticationResponse response{.did_succeed = false};
    requester_->OnFIDOAuthenticationComplete(response);
  }
}

void CreditCardFIDOAuthenticator::Register(std::string card_authorization_token,
                                           base::Value creation_options) {
  // Cancel any previous pending WebAuthn requests.
  authenticator()->Cancel();

  // If |creation_options| is set, then must enroll a new credential. Otherwise
  // directly send request to payments for opting in.
  card_authorization_token_ = card_authorization_token;
  if (creation_options.is_dict()) {
    if (IsValidCreationOptions(creation_options)) {
      current_flow_ = OPT_IN_WITH_CHALLENGE_FLOW;
      MakeCredential(ParseCreationOptions(creation_options));
    }
  } else {
    current_flow_ = OPT_IN_FETCH_CHALLENGE_FLOW;
    OptChange();
  }
}

void CreditCardFIDOAuthenticator::Authorize(
    base::WeakPtr<Requester> requester,
    std::string card_authorization_token,
    base::Value request_options) {
  requester_ = requester;
  card_authorization_token_ = card_authorization_token;

  // Cancel any previous pending WebAuthn requests.
  authenticator()->Cancel();

  if (IsValidRequestOptions(request_options)) {
    // If user is already opted-in, then a new card is trying to be
    // authorized. Otherwise, a user with a credential on file is trying to
    // opt-in.
    current_flow_ = user_is_opted_in_ ? FOLLOWUP_AFTER_CVC_AUTH_FLOW
                                      : OPT_IN_WITH_CHALLENGE_FLOW;
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  }
}

void CreditCardFIDOAuthenticator::OptOut() {
  // Cancel any previous pending WebAuthn requests.
  authenticator()->Cancel();

  current_flow_ = OPT_OUT_FLOW;
  card_authorization_token_ = std::string();
  OptChange();
}

void CreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  if (!IsCreditCardFidoAuthenticationEnabled() || !authenticator()) {
    std::move(callback).Run(false);
    return;
  }
#if BUILDFLAG(IS_ANDROID)
  // Because Payments servers only accept WebAuthn credentials for Android P
  // and above, this returns false if the build version is O or below.
  if (base::android::BuildInfo::GetInstance()->sdk_int() <
      base::android::SDK_VERSION_P) {
    std::move(callback).Run(false);
    return;
  }
#endif  // BUILDFLAG(IS_ANDROID)
  authenticator()->IsUserVerifyingPlatformAuthenticatorAvailable(
      std::move(callback));
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  return IsCreditCardFidoAuthenticationEnabled() &&
         prefs::IsCreditCardFIDOAuthEnabled(autofill_client_->GetPrefs());
}

UserOptInIntention CreditCardFIDOAuthenticator::GetUserOptInIntention(
    payments::PaymentsClient::UnmaskDetails& unmask_details) {
  // This local pref can be affected by the user toggling on the settings page.
  // And payments might not update in time. We derive user opt in/out intention
  // when we see the mismatch.
  user_is_opted_in_ = IsUserOptedIn();
  bool user_local_opt_in_status = IsUserOptedIn();

  // If payments is offering to opt-in, then that means user is not opted in
  // from Payments. Only take action if the local pref mismatches.
  if (unmask_details.offer_fido_opt_in && user_local_opt_in_status) {
#if BUILDFLAG(IS_ANDROID)
    // For Android, if local pref says user is opted in while payments not, it
    // denotes that user intended to opt in from settings page. We will opt user
    // in and hide the checkbox in the next checkout flow.
    // For intent to opt in, we also update |user_is_opted_in_| here so that
    // |current_flow_| can be correctly set to OPT_IN_WITH_CHALLENGE_FLOW when
    // calling Authorize() later.
    user_is_opted_in_ = false;
    return UserOptInIntention::kIntentToOptIn;
#else
    // For desktop, just update the local pref, since the desktop settings page
    // attempts opt-in at time of toggling the switch, unlike mobile.
    user_is_opted_in_ = false;
    UpdateUserPref();
#endif
  }

  // If payments is requesting a FIDO auth, then that means user is opted in
  // from payments. And if local pref says user is opted out, it denotes that
  // user intended to opt out.
  if (unmask_details.unmask_auth_method ==
          AutofillClient::UnmaskAuthMethod::kFido &&
      !user_local_opt_in_status) {
    return UserOptInIntention::kIntentToOptOut;
  }
  return UserOptInIntention::kUnspecified;
}

void CreditCardFIDOAuthenticator::CancelVerification() {
  authenticator()->Cancel();

  current_flow_ = NONE_FLOW;
  // Full card request may not exist when this function is called. The full card
  // request is created in OnDidGetAssertion() but the flow can be cancelled
  // before than.
  if (full_card_request_)
    full_card_request_->OnFIDOVerificationCancelled();
}

#if !BUILDFLAG(IS_ANDROID)
void CreditCardFIDOAuthenticator::OnWebauthnOfferDialogRequested(
    std::string card_authorization_token) {
  card_authorization_token_ = card_authorization_token;

  // Cancel any previous pending WebAuthn requests.
  authenticator()->Cancel();

  AutofillMetrics::LogWebauthnOptInPromoShown(
      /*is_checkout_flow=*/!card_authorization_token_.empty());

  // At this point, it must be the case that the user is opted-out, otherwise
  // there would be no need to register the user. However, if the user is
  // opting-in through the settings page, the user preference is set to opted-in
  // directly from the toggle switch being turned on. Storing the actual opt-in
  // state in |user_is_opted_in_| for now, and will update the pref store once
  // the UI flow is complete to avoid abrupt UI changes.
  user_is_opted_in_ = false;
}

void CreditCardFIDOAuthenticator::OnWebauthnOfferDialogUserResponse(
    bool did_accept) {
  if (did_accept) {
    // Wait until GetAssertion()/MakeCredential() to log user acceptance, since
    // user still has the option to cancel the dialog while the challenge is
    // being fetched.
    Register(card_authorization_token_);
  } else {
    // If user declined, log user decision. User may have initially accepted the
    // dialog, but then chose to cancel while the challenge was being fetched.
    AutofillMetrics::LogWebauthnOptInPromoUserDecision(
        /*is_checkout_flow=*/!card_authorization_token_.empty(),
        current_flow_ == OPT_IN_FETCH_CHALLENGE_FLOW
            ? AutofillMetrics::WebauthnOptInPromoUserDecisionMetric::
                  kDeclinedAfterAccepting
            : AutofillMetrics::WebauthnOptInPromoUserDecisionMetric::
                  kDeclinedImmediately);
    payments_client_->CancelRequest();
    card_authorization_token_ = std::string();
    current_flow_ = NONE_FLOW;
    GetOrCreateFidoAuthenticationStrikeDatabase()->AddStrikes(
        FidoAuthenticationStrikeDatabase::kStrikesToAddWhenOptInOfferDeclined);
    user_is_opted_in_ = false;
    UpdateUserPref();
  }
}
#endif

FidoAuthenticationStrikeDatabase*
CreditCardFIDOAuthenticator::GetOrCreateFidoAuthenticationStrikeDatabase() {
  if (!fido_authentication_strike_database_) {
    fido_authentication_strike_database_ =
        std::make_unique<FidoAuthenticationStrikeDatabase>(
            FidoAuthenticationStrikeDatabase(
                autofill_client_->GetStrikeDatabase()));
  }
  return fido_authentication_strike_database_.get();
}

void CreditCardFIDOAuthenticator::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr request_options) {
#if !BUILDFLAG(IS_ANDROID)
  // On desktop, during an opt-in flow, close the WebAuthn offer dialog and get
  // ready to show the OS level authentication dialog. If dialog is already
  // closed, then the offer was declined during the fetching challenge process,
  // and thus returned early.
  if (current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW) {
    if (autofill_client_->CloseWebauthnDialog()) {
      // Now that the dialog has closed and will proceed to a WebAuthn prompt,
      // the user must have accepted the dialog without cancelling.
      AutofillMetrics::LogWebauthnOptInPromoUserDecision(
          /*is_checkout_flow=*/!card_authorization_token_.empty(),
          AutofillMetrics::WebauthnOptInPromoUserDecisionMetric::kAccepted);
    } else {
      current_flow_ = NONE_FLOW;
      return;
    }
  }
#endif
  authenticator()->GetAssertion(
      std::move(request_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr creation_options) {
#if !BUILDFLAG(IS_ANDROID)
  // On desktop, close the WebAuthn offer dialog and get ready to show the OS
  // level authentication dialog. If dialog is already closed, then the offer
  // was declined during the fetching challenge process, and thus returned
  // early.
  if (autofill_client_->CloseWebauthnDialog()) {
    // Now that the dialog has closed and will proceed to a WebAuthn prompt,
    // the user must have accepted the dialog without cancelling.
    AutofillMetrics::LogWebauthnOptInPromoUserDecision(
        /*is_checkout_flow=*/!card_authorization_token_.empty(),
        AutofillMetrics::WebauthnOptInPromoUserDecisionMetric::kAccepted);
  } else {
    current_flow_ = NONE_FLOW;
    return;
  }
#endif
  authenticator()->MakeCredential(
      std::move(creation_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidMakeCredential,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::OptChange(
    base::Value authenticator_response) {
  payments::PaymentsClient::OptChangeRequestDetails request_details;
  request_details.app_locale =
      autofill_client_->GetPersonalDataManager()->app_locale();

  switch (current_flow_) {
    case OPT_IN_WITH_CHALLENGE_FLOW:
    case OPT_IN_FETCH_CHALLENGE_FLOW:
      request_details.reason =
          payments::PaymentsClient::OptChangeRequestDetails::ENABLE_FIDO_AUTH;
      break;
    case OPT_OUT_FLOW:
      request_details.reason =
          payments::PaymentsClient::OptChangeRequestDetails::DISABLE_FIDO_AUTH;
      break;
    case FOLLOWUP_AFTER_CVC_AUTH_FLOW:
      request_details.reason = payments::PaymentsClient::
          OptChangeRequestDetails::ADD_CARD_FOR_FIDO_AUTH;
      break;
    default:
      NOTREACHED();
      break;
  }

  // If |authenticator_response| is set, that means the user just signed a
  // challenge. In which case, if |card_authorization_token_| is not empty, then
  // that will be required to bind a previous CVC check with this signature.
  // This will opt the user in and authorize the card corresponding to
  // |card_authorization_token_|.
  // If |authenticator_response| is not set, that means the user was fetching a
  // challenge, in which case |card_authorization_token_| will be required for
  // the subsequent OptChange call.
  AutofillMetrics::WebauthnOptInParameters opt_change_metric;
  bool is_checkout_flow = !card_authorization_token_.empty();
  if (authenticator_response.is_dict()) {
    request_details.fido_authenticator_response =
        std::move(authenticator_response);
    opt_change_metric =
        request_details.fido_authenticator_response->FindKey(
            "fido_assertion_info")
            ? AutofillMetrics::WebauthnOptInParameters::kWithRequestChallenge
            : AutofillMetrics::WebauthnOptInParameters::kWithCreationChallenge;
    if (!card_authorization_token_.empty()) {
      request_details.card_authorization_token = card_authorization_token_;
      card_authorization_token_ = std::string();
    }
  } else {
    opt_change_metric =
        AutofillMetrics::WebauthnOptInParameters::kFetchingChallenge;
  }
  payments_client_->OptChange(
      request_details,
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetOptChangeResult,
                     weak_ptr_factory_.GetWeakPtr()));

  // Logging call if user was attempting to change their opt-in state.
  if (current_flow_ != FOLLOWUP_AFTER_CVC_AUTH_FLOW) {
    bool request_to_opt_in = (current_flow_ != OPT_OUT_FLOW);
    AutofillMetrics::LogWebauthnOptChangeCalled(
        request_to_opt_in, is_checkout_flow, opt_change_metric);
  }
}

void CreditCardFIDOAuthenticator::OnDidGetAssertion(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  LogWebauthnResult(status);

  if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    HandleGetAssertionSuccess(std::move(assertion_response));
  } else {
    HandleGetAssertionFailure();
  }
}

void CreditCardFIDOAuthenticator::OnDidMakeCredential(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr attestation_response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  LogWebauthnResult(status);

  // End the flow if there was an authentication error.
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    // Treat failure to perform user verification as a strong signal not to
    // offer opt-in in the future.
    if (current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW) {
      GetOrCreateFidoAuthenticationStrikeDatabase()->AddStrikes(
          FidoAuthenticationStrikeDatabase::
              kStrikesToAddWhenUserVerificationFailsOnOptInAttempt);
      user_is_opted_in_ = false;
      UpdateUserPref();
    }

    current_flow_ = NONE_FLOW;
    return;
  }

  OptChange(ParseAttestationResponse(std::move(attestation_response)));
}

void CreditCardFIDOAuthenticator::OnDidGetOptChangeResult(
    AutofillClient::PaymentsRpcResult result,
    payments::PaymentsClient::OptChangeResponseDetails& response) {
  DCHECK(current_flow_ == OPT_IN_FETCH_CHALLENGE_FLOW ||
         current_flow_ == OPT_OUT_FLOW ||
         current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW ||
         current_flow_ == FOLLOWUP_AFTER_CVC_AUTH_FLOW);

  // Update user preference to keep in sync with server.
  user_is_opted_in_ = response.user_is_opted_in.value_or(user_is_opted_in_);

  // When fetching the challenge on the settings page, don't update the user
  // preference yet. Otherwise the toggle will be visibly turned off, which may
  // seem confusing.
  bool is_settings_page = card_authorization_token_.empty();
  if (!is_settings_page || current_flow_ != OPT_IN_FETCH_CHALLENGE_FLOW)
    UpdateUserPref();

  // End the flow if the server responded with an error.
  if (result != AutofillClient::PaymentsRpcResult::kSuccess) {
#if !BUILDFLAG(IS_ANDROID)
    if (current_flow_ == OPT_IN_FETCH_CHALLENGE_FLOW)
      autofill_client_->UpdateWebauthnOfferDialogWithError();
#endif
    current_flow_ = NONE_FLOW;
    return;
  }

  // If response contains |creation_options| or |request_options| and the last
  // opt-in attempt did not include a challenge, then invoke WebAuthn
  // registration/verification prompt. Otherwise end the flow.
  if (current_flow_ == OPT_IN_FETCH_CHALLENGE_FLOW) {
    if (response.fido_creation_options.has_value()) {
      Register(card_authorization_token_,
               std::move(response.fido_creation_options.value()));
    } else if (response.fido_request_options.has_value()) {
      Authorize(/*requester=*/nullptr, card_authorization_token_,
                std::move(response.fido_request_options.value()));
    }
  } else {
    current_flow_ = NONE_FLOW;
  }
}

void CreditCardFIDOAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const std::u16string& cvc) {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;

  if (!requester_)
    return;

  FidoAuthenticationResponse response{
      .did_succeed = true, .card = &card, .cvc = cvc};
  requester_->OnFIDOAuthenticationComplete(response);
}

void CreditCardFIDOAuthenticator::OnFullCardRequestFailed(
    CreditCard::RecordType card_type,
    payments::FullCardRequest::FailureType failure_type) {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;

  if (!requester_)
    return;

  FidoAuthenticationResponse response{.did_succeed = false,
                                      .failure_type = failure_type};
  requester_->OnFIDOAuthenticationComplete(response);
}

blink::mojom::PublicKeyCredentialRequestOptionsPtr
CreditCardFIDOAuthenticator::ParseRequestOptions(
    const base::Value& request_options) {
  auto options = blink::mojom::PublicKeyCredentialRequestOptions::New();

  const auto* rpid = request_options.FindStringKey("relying_party_id");
  options->relying_party_id = rpid ? *rpid : std::string(kGooglePaymentsRpid);

  const auto* challenge = request_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* timeout = request_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->timeout =
      base::Milliseconds(timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  options->user_verification = device::UserVerificationRequirement::kRequired;

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  DCHECK(key_info_list);
  for (const base::Value& key_info : key_info_list->GetList()) {
    options->allow_credentials.push_back(ParseCredentialDescriptor(key_info));
  }

  return options;
}

blink::mojom::PublicKeyCredentialCreationOptionsPtr
CreditCardFIDOAuthenticator::ParseCreationOptions(
    const base::Value& creation_options) {
  auto options = blink::mojom::PublicKeyCredentialCreationOptions::New();

  const auto* rpid = creation_options.FindStringKey("relying_party_id");
  options->relying_party.id = rpid ? *rpid : kGooglePaymentsRpid;

  const auto* relying_party_name =
      creation_options.FindStringKey("relying_party_name");
  options->relying_party.name =
      relying_party_name ? *relying_party_name : kGooglePaymentsRpName;

  const std::string gaia =
      autofill_client_->GetIdentityManager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
          .gaia;
  options->user.id = std::vector<uint8_t>(gaia.begin(), gaia.end());
  options->user.name = autofill_client_->GetIdentityManager()
                           ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync)
                           .email;

  AccountInfo account_info =
      autofill_client_->GetIdentityManager()->FindExtendedAccountInfo(
          autofill_client_->GetPersonalDataManager()
              ->GetAccountInfoForPaymentsServer());
  options->user.display_name = account_info.given_name;

  const auto* challenge = creation_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* identifier_list = creation_options.FindKeyOfType(
      "algorithm_identifier", base::Value::Type::LIST);
  if (identifier_list) {
    for (const base::Value& algorithm_identifier : identifier_list->GetList()) {
      device::PublicKeyCredentialParams::CredentialInfo parameter;
      parameter.type = device::CredentialType::kPublicKey;
      parameter.algorithm = algorithm_identifier.GetInt();
      options->public_key_parameters.push_back(parameter);
    }
  }

  const auto* timeout = creation_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->timeout =
      base::Milliseconds(timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  const auto* attestation =
      creation_options.FindStringKey("attestation_conveyance_preference");
  if (!attestation || base::EqualsCaseInsensitiveASCII(*attestation, "NONE")) {
    options->attestation = device::AttestationConveyancePreference::kNone;
  } else if (base::EqualsCaseInsensitiveASCII(*attestation, "INDIRECT")) {
    options->attestation = device::AttestationConveyancePreference::kIndirect;
  } else if (base::EqualsCaseInsensitiveASCII(*attestation, "DIRECT")) {
    options->attestation = device::AttestationConveyancePreference::kDirect;
  } else {
    NOTREACHED();
  }

  // Only allow user-verifying platform authenticators.
  options->authenticator_selection = device::AuthenticatorSelectionCriteria(
      device::AuthenticatorAttachment::kPlatform,
      device::ResidentKeyRequirement::kDiscouraged,
      device::UserVerificationRequirement::kRequired);

  // List of keys that Payments already knows about, and so should not make a
  // new credential.
  const auto* excluded_keys_list =
      creation_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  if (excluded_keys_list) {
    for (const base::Value& key_info : excluded_keys_list->GetList()) {
      options->exclude_credentials.push_back(
          ParseCredentialDescriptor(key_info));
    }
  }

  return options;
}

device::PublicKeyCredentialDescriptor
CreditCardFIDOAuthenticator::ParseCredentialDescriptor(
    const base::Value& key_info) {
  std::vector<uint8_t> credential_id;
  const auto* id = key_info.FindStringKey("credential_id");
  DCHECK(id);
  credential_id = Base64ToBytes(*id);

  base::flat_set<device::FidoTransportProtocol> authenticator_transports;
  const auto* transports = key_info.FindKeyOfType(
      "authenticator_transport_support", base::Value::Type::LIST);
  if (transports && !transports->GetList().empty()) {
    for (const base::Value& transport_type : transports->GetList()) {
      absl::optional<device::FidoTransportProtocol> protocol =
          device::ConvertToFidoTransportProtocol(
              base::ToLowerASCII(transport_type.GetString()));
      if (protocol.has_value())
        authenticator_transports.insert(*protocol);
    }
  }

  return device::PublicKeyCredentialDescriptor(
      device::CredentialType::kPublicKey, credential_id,
      authenticator_transports);
}

base::Value CreditCardFIDOAuthenticator::ParseAssertionResponse(
    blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);
  response.SetKey("credential_id",
                  BytesToBase64(assertion_response->info->raw_id));
  response.SetKey("authenticator_data",
                  BytesToBase64(assertion_response->info->authenticator_data));
  response.SetKey("client_data",
                  BytesToBase64(assertion_response->info->client_data_json));
  response.SetKey("signature", BytesToBase64(assertion_response->signature));
  return response;
}

base::Value CreditCardFIDOAuthenticator::ParseAttestationResponse(
    blink::mojom::MakeCredentialAuthenticatorResponsePtr attestation_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);

  base::Value fido_attestation_info =
      base::Value(base::Value::Type::DICTIONARY);
  fido_attestation_info.SetKey(
      "client_data",
      BytesToBase64(attestation_response->info->client_data_json));
  fido_attestation_info.SetKey(
      "attestation_object",
      BytesToBase64(attestation_response->attestation_object));

  base::Value authenticator_transport_list =
      base::Value(base::Value::Type::LIST);
  for (device::FidoTransportProtocol protocol :
       attestation_response->transports) {
    authenticator_transport_list.Append(
        base::Value(base::ToUpperASCII(device::ToString(protocol))));
  }

  response.SetKey("fido_attestation_info", std::move(fido_attestation_info));
  response.SetKey("authenticator_transport",
                  std::move(authenticator_transport_list));

  return response;
}

bool CreditCardFIDOAuthenticator::IsValidRequestOptions(
    const base::Value& request_options) {
  if (!request_options.is_dict() || request_options.DictEmpty() ||
      !request_options.FindStringKey("challenge") ||
      !request_options.FindKeyOfType("key_info", base::Value::Type::LIST)) {
    return false;
  }

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);

  if (key_info_list->GetList().empty())
    return false;

  for (const base::Value& key_info : key_info_list->GetList()) {
    if (!key_info.is_dict() || !key_info.FindStringKey("credential_id"))
      return false;
  }

  return true;
}

bool CreditCardFIDOAuthenticator::IsValidCreationOptions(
    const base::Value& creation_options) {
  return creation_options.is_dict() &&
         creation_options.FindStringKey("challenge");
}

void CreditCardFIDOAuthenticator::LogWebauthnResult(
    blink::mojom::AuthenticatorStatus status) {
  AutofillMetrics::WebauthnFlowEvent event;
  switch (current_flow_) {
    case AUTHENTICATION_FLOW:
      event = AutofillMetrics::WebauthnFlowEvent::kImmediateAuthentication;
      break;
    case FOLLOWUP_AFTER_CVC_AUTH_FLOW:
      event = AutofillMetrics::WebauthnFlowEvent::kAuthenticationAfterCvc;
      break;
    case OPT_IN_WITH_CHALLENGE_FLOW:
      event = card_authorization_token_.empty()
                  ? AutofillMetrics::WebauthnFlowEvent::kSettingsPageOptIn
                  : AutofillMetrics::WebauthnFlowEvent::kCheckoutOptIn;
      break;
    default:
      NOTREACHED();
      return;
  }

  // TODO(crbug.com/949269): Add metrics for revoked pending WebAuthn requests.
  AutofillMetrics::WebauthnResultMetric metric;
  switch (status) {
    case blink::mojom::AuthenticatorStatus::SUCCESS:
      metric = AutofillMetrics::WebauthnResultMetric::kSuccess;
      break;
    case blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR:
      metric = AutofillMetrics::WebauthnResultMetric::kNotAllowedError;
      break;
    default:
      metric = AutofillMetrics::WebauthnResultMetric::kOtherError;
      break;
  }
  AutofillMetrics::LogWebauthnResult(event, metric);
}

void CreditCardFIDOAuthenticator::UpdateUserPref() {
  prefs::SetCreditCardFIDOAuthEnabled(autofill_client_->GetPrefs(),
                                      user_is_opted_in_);
}

void CreditCardFIDOAuthenticator::HandleGetAssertionSuccess(
    blink::mojom::GetAssertionAuthenticatorResponsePtr assertion_response) {
  switch (current_flow_) {
    case AUTHENTICATION_FLOW: {
      base::Value response =
          ParseAssertionResponse(std::move(assertion_response));
      full_card_request_ = std::make_unique<payments::FullCardRequest>(
          autofill_client_, autofill_client_->GetPaymentsClient(),
          autofill_client_->GetPersonalDataManager());

      absl::optional<GURL> last_committed_primary_main_frame_origin;
      if (card_->record_type() == CreditCard::VIRTUAL_CARD &&
          autofill_client_->GetLastCommittedPrimaryMainFrameURL().is_valid()) {
        last_committed_primary_main_frame_origin =
            autofill_client_->GetLastCommittedPrimaryMainFrameURL()
                .DeprecatedGetOriginAsURL();
      }
#if BUILDFLAG(IS_ANDROID)
      if (base::FeatureList::IsEnabled(
              features::kAutofillEnableFIDOProgressDialog)) {
        // Open the progress dialog when authenticating and getting the full
        // card from FIDO.
        autofill_client_->ShowAutofillProgressDialog(
            AutofillProgressDialogType::kAndroidFIDOProgressDialog,
            base::BindOnce(&CreditCardFIDOAuthenticator::CancelVerification,
                           weak_ptr_factory_.GetWeakPtr()));
      }
#endif
      full_card_request_->GetFullCardViaFIDO(
          *card_, AutofillClient::UnmaskCardReason::kAutofill,
          weak_ptr_factory_.GetWeakPtr(), std::move(response),
          last_committed_primary_main_frame_origin, context_token_);
      // Return here to skip the OptChange call.
      return;
    }

    case FOLLOWUP_AFTER_CVC_AUTH_FLOW: {
      // The user-facing portion of the authorization is complete, which should
      // be reported so that the form can be filled.
      if (requester_)
        requester_->OnFidoAuthorizationComplete(/*did_succeed=*/true);
      break;
    }

    case OPT_IN_WITH_CHALLENGE_FLOW: {
#if BUILDFLAG(IS_ANDROID)
      // For Android, opt-in flow (OPT_IN_WITH_CHALLENGE_FLOW) also delays form
      // filling.
      if (requester_)
        requester_->OnFidoAuthorizationComplete(/*did_succeed=*/true);
#endif
      break;
    }

    case NONE_FLOW:
    case OPT_IN_FETCH_CHALLENGE_FLOW:
    case OPT_OUT_FLOW: {
      NOTREACHED();
      return;
    }
  }

  base::Value response = base::Value(base::Value::Type::DICTIONARY);
  response.SetKey("fido_assertion_info",
                  ParseAssertionResponse(std::move(assertion_response)));
  OptChange(std::move(response));
}

void CreditCardFIDOAuthenticator::HandleGetAssertionFailure() {
  switch (current_flow_) {
    case AUTHENTICATION_FLOW: {
      // End the flow if there was an authentication error.
      if (requester_) {
        requester_->OnFIDOAuthenticationComplete(
            FidoAuthenticationResponse{.did_succeed = false});
      }
      break;
    }

    case FOLLOWUP_AFTER_CVC_AUTH_FLOW: {
      if (requester_)
        requester_->OnFidoAuthorizationComplete(/*did_succeed=*/false);
      break;
    }

    case OPT_IN_WITH_CHALLENGE_FLOW: {
      // Treat failure to perform user verification as a strong signal not to
      // offer opt-in in the future.
#if BUILDFLAG(IS_ANDROID)
      // For Android, even if GetAssertion fails for opting-in, we still report
      // success to |requester_| to fill the form with the fetched card info.
      if (requester_)
        requester_->OnFidoAuthorizationComplete(/*did_succeed=*/true);
#endif  // BUILDFLAG(IS_ANDROID)
      GetOrCreateFidoAuthenticationStrikeDatabase()->AddStrikes(
          FidoAuthenticationStrikeDatabase::
              kStrikesToAddWhenUserVerificationFailsOnOptInAttempt);
      user_is_opted_in_ = false;
      UpdateUserPref();
      break;
    }

    case NONE_FLOW:
    case OPT_IN_FETCH_CHALLENGE_FLOW:
    case OPT_OUT_FLOW: {
      NOTREACHED();
      break;
    }
  }
  current_flow_ = NONE_FLOW;
}

webauthn::InternalAuthenticator* CreditCardFIDOAuthenticator::authenticator() {
  if (!authenticator_) {
    authenticator_ = autofill_client_->CreateCreditCardInternalAuthenticator(
        autofill_driver_.get());
    // `authenticator_` may be null for unsupported platforms.
    if (authenticator_) {
      authenticator_->SetEffectiveOrigin(
          url::Origin::Create(payments::GetBaseSecureUrl()));
    }
  }
  return authenticator_.get();
}

}  // namespace autofill
