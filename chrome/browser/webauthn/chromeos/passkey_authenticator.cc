// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/chromeos/passkey_authenticator.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/webauthn/chromeos/passkey_in_session_auth.h"
#include "chrome/browser/webauthn/chromeos/passkey_service.h"
#include "components/device_event_log/device_event_log.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/sha2.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "ui/aura/window.h"

using device::AuthenticatorData;
using device::AuthenticatorGetAssertionResponse;
using device::AuthenticatorSupportedOptions;
using device::AuthenticatorType;
using device::CoseAlgorithmIdentifier;
using device::CredentialType;
using device::CtapGetAssertionOptions;
using device::CtapGetAssertionRequest;
using device::CtapMakeCredentialRequest;
using device::FidoAuthenticator;
using device::FidoRequestHandlerBase;
using device::FidoTransportProtocol;
using device::GetAssertionStatus;
using device::MakeCredentialOptions;
using device::PublicKeyCredentialDescriptor;
using device::PublicKeyCredentialUserEntity;

namespace chromeos {
namespace {

AuthenticatorSupportedOptions PasskeyAuthenticatorOptions() {
  AuthenticatorSupportedOptions options;
  options.is_platform_device =
      AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability = AuthenticatorSupportedOptions::
      UserVerificationAvailability::kSupportedAndConfigured;
  return options;
}

// Returns the WebAuthn authenticator data for this authenticator. See
// https://w3c.github.io/webauthn/#authenticator-data.
AuthenticatorData MakeAuthenticatorDataForAssertion(std::string_view rp_id) {
  using Flag = AuthenticatorData::Flag;
  return AuthenticatorData(
      crypto::SHA256Hash(base::as_byte_span(rp_id)),
      {Flag::kTestOfUserPresence, Flag::kTestOfUserVerification,
       Flag::kBackupEligible, Flag::kBackupState},
      /*sign_counter=*/0u,
      /*attested_credential_data=*/std::nullopt,
      /*extensions=*/std::nullopt);
}

std::optional<std::vector<uint8_t>> GenerateEcSignature(
    base::span<const uint8_t> pkcs8_ec_private_key,
    base::span<const uint8_t> signed_over_data) {
  auto ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(pkcs8_ec_private_key);
  if (!ec_private_key) {
    return std::nullopt;
  }
  auto signer = crypto::ECSignatureCreator::Create(ec_private_key.get());
  std::vector<uint8_t> signature;
  if (!signer->Sign(signed_over_data, &signature)) {
    return std::nullopt;
  }
  return signature;
}

}  // namespace

PasskeyAuthenticator::PasskeyAuthenticator(
    content::RenderFrameHost* rfh,
    PasskeyService* passkey_service,
    webauthn::PasskeyModel* passkey_model)
    : render_frame_host_id_(rfh->GetGlobalId()),
      passkey_service_(passkey_service),
      passkey_model_(passkey_model) {}

PasskeyAuthenticator::~PasskeyAuthenticator() = default;

AuthenticatorType PasskeyAuthenticator::GetType() const {
  return AuthenticatorType::kChromeOSPasskeys;
}

std::string PasskeyAuthenticator::GetId() const {
  return "ChromeOSPasskeysAuthenticator";
}

std::optional<base::span<const int32_t>> PasskeyAuthenticator::GetAlgorithms() {
  constexpr std::array<int32_t, 1> kAlgorithms{
      static_cast<int32_t>(CoseAlgorithmIdentifier::kEs256)};
  return kAlgorithms;
}

const AuthenticatorSupportedOptions& PasskeyAuthenticator::Options() const {
  static const base::NoDestructor<AuthenticatorSupportedOptions> options(
      PasskeyAuthenticatorOptions());
  return *options;
}

std::optional<FidoTransportProtocol>
PasskeyAuthenticator::AuthenticatorTransport() const {
  return FidoTransportProtocol::kInternal;
}

void PasskeyAuthenticator::GetTouch(base::OnceClosure callback) {}

void PasskeyAuthenticator::InitializeAuthenticator(base::OnceClosure callback) {
  std::move(callback).Run();
}

void PasskeyAuthenticator::MakeCredential(CtapMakeCredentialRequest request,
                                          MakeCredentialOptions request_options,
                                          MakeCredentialCallback callback) {}

void PasskeyAuthenticator::GetAssertion(CtapGetAssertionRequest request,
                                        CtapGetAssertionOptions options,
                                        GetAssertionCallback callback) {
  std::string rp_id = request.rp_id;
  PasskeyInSessionAuthProvider::Get()->ShowPasskeyInSessionAuthDialog(
      content::RenderFrameHost::FromID(render_frame_host_id_)
          ->GetNativeView()
          ->GetToplevelWindow(),
      rp_id,
      base::BindOnce(&PasskeyAuthenticator::FinishGetAssertion,
                     weak_factory_.GetWeakPtr(), std::move(request),
                     std::move(options), std::move(callback)));
  return;
}

void PasskeyAuthenticator::FinishGetAssertion(CtapGetAssertionRequest request,
                                              CtapGetAssertionOptions options,
                                              GetAssertionCallback callback,
                                              bool user_verification_success) {
  if (!user_verification_success) {
    std::move(callback).Run(
        GetAssertionStatus::kUserConsentButCredentialNotRecognized, {});
    return;
  }

  CHECK_EQ(request.allow_list.size(), 1u);
  const std::vector<uint8_t>& credential_id = request.allow_list.begin()->id;
  const std::string credential_id_str = {credential_id.begin(),
                                         credential_id.end()};

  std::optional<sync_pb::WebauthnCredentialSpecifics> credential_specifics =
      passkey_model_->GetPasskeyByCredentialId(request.rp_id,
                                               credential_id_str);
  if (!credential_specifics) {
    FIDO_LOG(ERROR) << "Could not find a matching GPM credential.";
    std::move(callback).Run(
        GetAssertionStatus::kUserConsentButCredentialNotRecognized, {});
    return;
  }

  const std::optional<std::vector<uint8_t>> security_domain_secret =
      passkey_service_->GetCachedSecurityDomainSecret();
  if (!security_domain_secret) {
    FIDO_LOG(ERROR) << "Security domain secret unavailable.";
    std::move(callback).Run(
        GetAssertionStatus::kUserConsentButCredentialNotRecognized, {});
    return;
  }

  // Decrypt the sealed data from `credential_specifics` into
  // `credential_secrets`. Note that `DecryptWebauthnCredentialSpecificsData()`
  // internally maps both the `encrypted` and `private_key` case of the
  // `encrypted_data` oneof to `WebauthnCredentialSpecifics_Encrypted`. In the
  // latter case, only the `private_key` field will be set.
  sync_pb::WebauthnCredentialSpecifics_Encrypted unsealed_credential_secrets;
  if (!webauthn::passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
          base::make_span(*security_domain_secret), *credential_specifics,
          &unsealed_credential_secrets)) {
    FIDO_LOG(ERROR) << "Decrypting WebauthnCredentialSpecifics failed.";
    std::move(callback).Run(
        GetAssertionStatus::kUserConsentButCredentialNotRecognized, {});
    return;
  }

  AuthenticatorData authenticator_data =
      MakeAuthenticatorDataForAssertion(request.rp_id);
  std::vector<uint8_t> signed_over_data(
      authenticator_data.SerializeToByteArray());
  signed_over_data.insert(signed_over_data.end(),
                          request.client_data_hash.begin(),
                          request.client_data_hash.end());
  std::optional<std::vector<uint8_t>> assertion_signature = GenerateEcSignature(
      base::as_byte_span(unsealed_credential_secrets.private_key()),
      signed_over_data);
  if (!assertion_signature) {
    FIDO_LOG(ERROR) << "Generating assertion signature failed";
    std::move(callback).Run(
        GetAssertionStatus::kUserConsentButCredentialNotRecognized, {});
    return;
  }

  AuthenticatorGetAssertionResponse assertion_response(
      std::move(authenticator_data), std::move(*assertion_signature),
      /*transport_used=*/std::nullopt);
  assertion_response.credential =
      PublicKeyCredentialDescriptor(CredentialType::kPublicKey, credential_id);
  assertion_response.user_entity = PublicKeyCredentialUserEntity(
      std::vector<uint8_t>(credential_specifics->user_id().begin(),
                           credential_specifics->user_id().end()));
  std::vector<AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(std::move(assertion_response));
  std::move(callback).Run(GetAssertionStatus::kSuccess, std::move(responses));
}

void PasskeyAuthenticator::Cancel() {
  NOTIMPLEMENTED();
}

base::WeakPtr<FidoAuthenticator> PasskeyAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace chromeos
