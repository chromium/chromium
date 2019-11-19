// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/containers/flat_set.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/fido_authentication_strike_database.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "third_party/blink/public/mojom/webauthn/internal_authenticator.mojom.h"
#include "url/gurl.h"

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
          base::WaitableEvent::InitialState::NOT_SIGNALED) {}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {}

void CreditCardFIDOAuthenticator::ShowWebauthnOfferDialog(
    std::string card_authorization_token) {
  card_authorization_token_ = card_authorization_token;
  autofill_client_->ShowWebauthnOfferDialog(base::BindRepeating(
      &CreditCardFIDOAuthenticator::OnWebauthnOfferDialogUserResponse,
      weak_ptr_factory_.GetWeakPtr()));
  AutofillMetrics::LogWebauthnOptInPromoShown(
      /*is_checkout_flow=*/!card_authorization_token_.empty());
}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::TimeTicks form_parsed_timestamp,
    base::Value request_options) {
  card_ = card;
  requester_ = requester;
  form_parsed_timestamp_ = form_parsed_timestamp;

  if (card_ && IsValidRequestOptions(request_options.Clone())) {
    current_flow_ = AUTHENTICATION_FLOW;
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  } else {
    requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
  }
}

void CreditCardFIDOAuthenticator::Register(std::string card_authorization_token,
                                           base::Value creation_options) {
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
    std::string card_authorization_token,
    base::Value request_options) {
  card_authorization_token_ = card_authorization_token;
  if (IsValidRequestOptions(request_options)) {
    // If user is already opted-in, then a new card is trying to be
    // authorized. Otherwise, a user with a credential on file is trying to
    // opt-in.
    current_flow_ = IsUserOptedIn() ? FOLLOWUP_AFTER_CVC_AUTH_FLOW
                                    : OPT_IN_WITH_CHALLENGE_FLOW;
    GetAssertion(ParseRequestOptions(std::move(request_options)));
  }
}

void CreditCardFIDOAuthenticator::OptOut() {
  current_flow_ = OPT_OUT_FLOW;
  card_authorization_token_ = std::string();
  OptChange();
}

void CreditCardFIDOAuthenticator::IsUserVerifiable(
    base::OnceCallback<void(bool)> callback) {
  if (base::FeatureList::IsEnabled(
          features::kAutofillCreditCardAuthentication)) {
    if (!authenticator_.is_bound()) {
      autofill_driver_->ConnectToAuthenticator(
          authenticator_.BindNewPipeAndPassReceiver());
    }
    authenticator_->IsUserVerifyingPlatformAuthenticatorAvailable(
        std::move(callback));
  } else {
    std::move(callback).Run(false);
  }
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  return base::FeatureList::IsEnabled(
             features::kAutofillCreditCardAuthentication) &&
         ::autofill::prefs::IsCreditCardFIDOAuthEnabled(
             autofill_client_->GetPrefs());
}

void CreditCardFIDOAuthenticator::SyncUserOptIn(
    AutofillClient::UnmaskDetails& unmask_details) {
  bool is_user_opted_in = IsUserOptedIn();

  // If payments is offering to opt-in, then that means user is not opted in.
  if (unmask_details.offer_fido_opt_in) {
    is_user_opted_in = false;
  }

  // If payments is requesting a FIDO auth, then that means user is opted in.
  if (unmask_details.unmask_auth_method ==
      AutofillClient::UnmaskAuthMethod::FIDO) {
    is_user_opted_in = true;
  }

  // Update pref setting if needed.
  ::autofill::prefs::SetCreditCardFIDOAuthEnabled(autofill_client_->GetPrefs(),
                                                  is_user_opted_in);
}

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
    PublicKeyCredentialRequestOptionsPtr request_options) {
  if (!authenticator_.is_bound()) {
    autofill_driver_->ConnectToAuthenticator(
        authenticator_.BindNewPipeAndPassReceiver());
  }
#if !defined(OS_ANDROID)
  // On desktop, during an opt-in flow, close the WebAuthn offer dialog and get
  // ready to show the OS level authentication dialog. If dialog is already
  // closed, then the offer was declined during the fetching challenge process,
  // and thus returned early.
  if (current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW) {
    if (autofill_client_->CloseWebauthnOfferDialog()) {
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
  authenticator_->GetAssertion(
      std::move(request_options),
      base::BindOnce(&CreditCardFIDOAuthenticator::OnDidGetAssertion,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CreditCardFIDOAuthenticator::MakeCredential(
    PublicKeyCredentialCreationOptionsPtr creation_options) {
  if (!authenticator_.is_bound()) {
    autofill_driver_->ConnectToAuthenticator(
        authenticator_.BindNewPipeAndPassReceiver());
  }
#if !defined(OS_ANDROID)
  // On desktop, close the WebAuthn offer dialog and get ready to show the OS
  // level authentication dialog. If dialog is already closed, then the offer
  // was declined during the fetching challenge process, and thus returned
  // early.
  if (autofill_client_->CloseWebauthnOfferDialog()) {
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
  authenticator_->MakeCredential(
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
        request_details.fido_authenticator_response.FindKey(
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
    AuthenticatorStatus status,
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  LogWebauthnResult(status);

  // End the flow if there was an authentication error.
  if (status != AuthenticatorStatus::SUCCESS) {
    // Report failure to |requester_| if card unmasking was requested.
    if (current_flow_ == AUTHENTICATION_FLOW)
      requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);

    // Treat failure to perform user verification as a strong signal not to
    // offer opt-in in the future.
    if (current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW) {
      GetOrCreateFidoAuthenticationStrikeDatabase()->AddStrikes(
          FidoAuthenticationStrikeDatabase::
              kStrikesToAddWhenUserVerificationFailsOnOptInAttempt);
    }

    current_flow_ = NONE_FLOW;
    return;
  }

  if (current_flow_ == AUTHENTICATION_FLOW) {
    base::Value response =
        ParseAssertionResponse(std::move(assertion_response));
    full_card_request_.reset(new payments::FullCardRequest(
        autofill_client_, autofill_client_->GetPaymentsClient(),
        autofill_client_->GetPersonalDataManager(), form_parsed_timestamp_));
    full_card_request_->GetFullCardViaFIDO(
        *card_, AutofillClient::UNMASK_FOR_AUTOFILL,
        weak_ptr_factory_.GetWeakPtr(), std::move(response));
  } else {
    DCHECK(current_flow_ == FOLLOWUP_AFTER_CVC_AUTH_FLOW ||
           current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW);
    base::Value response = base::Value(base::Value::Type::DICTIONARY);
    response.SetKey("fido_assertion_info",
                    ParseAssertionResponse(std::move(assertion_response)));
    OptChange(std::move(response));
  }
}

void CreditCardFIDOAuthenticator::OnDidMakeCredential(
    AuthenticatorStatus status,
    MakeCredentialAuthenticatorResponsePtr attestation_response) {
  LogWebauthnResult(status);

  // End the flow if there was an authentication error.
  if (status != AuthenticatorStatus::SUCCESS) {
    // Treat failure to perform user verification as a strong signal not to
    // offer opt-in in the future.
    if (current_flow_ == OPT_IN_WITH_CHALLENGE_FLOW) {
      GetOrCreateFidoAuthenticationStrikeDatabase()->AddStrikes(
          FidoAuthenticationStrikeDatabase::
              kStrikesToAddWhenUserVerificationFailsOnOptInAttempt);
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
  ::autofill::prefs::SetCreditCardFIDOAuthEnabled(
      autofill_client_->GetPrefs(),
      response.user_is_opted_in.value_or(IsUserOptedIn()));

  // End the flow if the server responded with an error.
  if (result != AutofillClient::PaymentsRpcResult::SUCCESS) {
    if (current_flow_ == OPT_IN_FETCH_CHALLENGE_FLOW)
      autofill_client_->UpdateWebauthnOfferDialogWithError();
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
      Authorize(card_authorization_token_,
                std::move(response.fido_request_options.value()));
    }
  } else {
    current_flow_ = NONE_FLOW;
  }
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
    ::autofill::prefs::SetCreditCardFIDOAuthEnabled(
        autofill_client_->GetPrefs(), false);
  }
}

void CreditCardFIDOAuthenticator::OnFullCardRequestSucceeded(
    const payments::FullCardRequest& full_card_request,
    const CreditCard& card,
    const base::string16& cvc) {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/true, &card);
}

void CreditCardFIDOAuthenticator::OnFullCardRequestFailed() {
  DCHECK_EQ(AUTHENTICATION_FLOW, current_flow_);
  current_flow_ = NONE_FLOW;
  requester_->OnFIDOAuthenticationComplete(/*did_succeed=*/false);
}

PublicKeyCredentialRequestOptionsPtr
CreditCardFIDOAuthenticator::ParseRequestOptions(
    const base::Value& request_options) {
  auto options = PublicKeyCredentialRequestOptions::New();

  const auto* rpid = request_options.FindStringKey("relying_party_id");
  options->relying_party_id = rpid ? *rpid : std::string(kGooglePaymentsRpid);

  const auto* challenge = request_options.FindStringKey("challenge");
  DCHECK(challenge);
  options->challenge = Base64ToBytes(*challenge);

  const auto* timeout = request_options.FindKeyOfType(
      "timeout_millis", base::Value::Type::INTEGER);
  options->adjusted_timeout = base::TimeDelta::FromMilliseconds(
      timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  options->user_verification = UserVerificationRequirement::kRequired;

  const auto* key_info_list =
      request_options.FindKeyOfType("key_info", base::Value::Type::LIST);
  DCHECK(key_info_list);
  for (const base::Value& key_info : key_info_list->GetList()) {
    options->allow_credentials.push_back(ParseCredentialDescriptor(key_info));
  }

  return options;
}

PublicKeyCredentialCreationOptionsPtr
CreditCardFIDOAuthenticator::ParseCreationOptions(
    const base::Value& creation_options) {
  auto options = PublicKeyCredentialCreationOptions::New();

  const auto* rpid = creation_options.FindStringKey("relying_party_id");
  options->relying_party.id = rpid ? *rpid : kGooglePaymentsRpid;

  const auto* relying_party_name =
      creation_options.FindStringKey("relying_party_name");
  options->relying_party.name =
      relying_party_name ? *relying_party_name : kGooglePaymentsRpName;

  const auto* icon_url = creation_options.FindStringKey("icon_url");
  if (icon_url)
    options->relying_party.icon_url = GURL(*icon_url);

  const std::string gaia =
      autofill_client_->GetIdentityManager()->GetPrimaryAccountInfo().gaia;
  options->user.id = std::vector<uint8_t>(gaia.begin(), gaia.end());
  options->user.name =
      autofill_client_->GetIdentityManager()->GetPrimaryAccountInfo().email;

  base::Optional<AccountInfo> account_info =
      autofill_client_->GetIdentityManager()
          ->FindExtendedAccountInfoForAccountWithRefreshToken(
              autofill_client_->GetPersonalDataManager()
                  ->GetAccountInfoForPaymentsServer());
  if (account_info.has_value()) {
    options->user.display_name = account_info.value().given_name;
    options->user.icon_url = GURL(account_info.value().picture_url);
  } else {
    options->user.display_name = "";
  }

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
  options->adjusted_timeout = base::TimeDelta::FromMilliseconds(
      timeout ? timeout->GetInt() : kWebAuthnTimeoutMs);

  const auto* attestation =
      creation_options.FindStringKey("attestation_conveyance_preference");
  if (!attestation || base::EqualsCaseInsensitiveASCII(*attestation, "NONE")) {
    options->attestation = AttestationConveyancePreference::kNone;
  } else if (base::EqualsCaseInsensitiveASCII(*attestation, "INDIRECT")) {
    options->attestation = AttestationConveyancePreference::kIndirect;
  } else if (base::EqualsCaseInsensitiveASCII(*attestation, "DIRECT")) {
    options->attestation = AttestationConveyancePreference::kDirect;
  } else {
    NOTREACHED();
  }

  // Only allow user-verifying platform authenticators.
  options->authenticator_selection = AuthenticatorSelectionCriteria(
      AuthenticatorAttachment::kPlatform, /*require_resident_key=*/false,
      UserVerificationRequirement::kRequired);

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

PublicKeyCredentialDescriptor
CreditCardFIDOAuthenticator::ParseCredentialDescriptor(
    const base::Value& key_info) {
  std::vector<uint8_t> credential_id;
  const auto* id = key_info.FindStringKey("credential_id");
  DCHECK(id);
  credential_id = Base64ToBytes(*id);

  base::flat_set<FidoTransportProtocol> authenticator_transports;
  const auto* transports = key_info.FindKeyOfType(
      "authenticator_transport_support", base::Value::Type::LIST);
  if (transports && !transports->GetList().empty()) {
    for (const base::Value& transport_type : transports->GetList()) {
      base::Optional<FidoTransportProtocol> protocol =
          device::ConvertToFidoTransportProtocol(
              base::ToLowerASCII(transport_type.GetString()));
      if (protocol.has_value())
        authenticator_transports.insert(*protocol);
    }
  }

  return PublicKeyCredentialDescriptor(CredentialType::kPublicKey,
                                       credential_id, authenticator_transports);
}

base::Value CreditCardFIDOAuthenticator::ParseAssertionResponse(
    GetAssertionAuthenticatorResponsePtr assertion_response) {
  base::Value response = base::Value(base::Value::Type::DICTIONARY);
  response.SetKey("credential_id",
                  BytesToBase64(assertion_response->info->raw_id));
  response.SetKey("authenticator_data",
                  BytesToBase64(assertion_response->authenticator_data));
  response.SetKey("client_data",
                  BytesToBase64(assertion_response->info->client_data_json));
  response.SetKey("signature", BytesToBase64(assertion_response->signature));
  return response;
}

base::Value CreditCardFIDOAuthenticator::ParseAttestationResponse(
    MakeCredentialAuthenticatorResponsePtr attestation_response) {
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
  for (FidoTransportProtocol protocol : attestation_response->transports) {
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
    AuthenticatorStatus status) {
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

  AutofillMetrics::WebauthnResultMetric metric;
  switch (status) {
    case AuthenticatorStatus::SUCCESS:
      metric = AutofillMetrics::WebauthnResultMetric::kSuccess;
      break;
    case AuthenticatorStatus::NOT_ALLOWED_ERROR:
      metric = AutofillMetrics::WebauthnResultMetric::kNotAllowedError;
      break;
    default:
      metric = AutofillMetrics::WebauthnResultMetric::kOtherError;
      break;
  }
  AutofillMetrics::LogWebauthnResult(event, metric);
}
}  // namespace autofill
