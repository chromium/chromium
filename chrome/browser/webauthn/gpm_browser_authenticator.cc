// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_browser_authenticator.h"

#include <algorithm>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/no_destructor.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_number_conversions.h"
#include "components/device_event_log/device_event_log.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "crypto/random.h"
#include "device/fido/attestation_object.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/authenticator_supported_options.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/prf_input.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/public/public_key_credential_user_entity.h"

namespace {

namespace passkey_model_utils = webauthn::passkey_model_utils;

// GPM passkeys sync across devices and can be used both locally and via hybrid
// transport.
constexpr std::initializer_list<device::FidoTransportProtocol> kGpmTransports =
    {device::FidoTransportProtocol::kInternal,
     device::FidoTransportProtocol::kHybrid};

device::AuthenticatorSupportedOptions GpmBrowserAuthenticatorOptions() {
  device::AuthenticatorSupportedOptions options;
  options.is_platform_device =
      device::AuthenticatorSupportedOptions::PlatformDevice::kYes;
  options.supports_resident_key = true;
  options.user_verification_availability =
      device::AuthenticatorSupportedOptions::UserVerificationAvailability::
          kSupportedAndConfigured;
  options.supports_user_presence = false;
  options.supports_prf = true;
  return options;
}

const device::PRFInput* SelectPrfInput(
    const std::vector<device::PRFInput>& prf_inputs,
    base::span<const uint8_t> credential_id) {
  const device::PRFInput* default_input = nullptr;
  for (const device::PRFInput& prf_input : prf_inputs) {
    if (!prf_input.credential_id) {
      default_input = &prf_input;
    } else if (std::ranges::equal(*prf_input.credential_id, credential_id)) {
      return &prf_input;
    }
  }
  return default_input;
}

}  // namespace

GpmBrowserAuthenticatorRequest::MakeCredential::MakeCredential(
    trusted_vault::TrustedVaultKeyAndVersion trusted_vault_key)
    : trusted_vault_key(std::move(trusted_vault_key)) {}
GpmBrowserAuthenticatorRequest::MakeCredential::MakeCredential(
    MakeCredential&&) = default;
GpmBrowserAuthenticatorRequest::MakeCredential&
GpmBrowserAuthenticatorRequest::MakeCredential::operator=(MakeCredential&&) =
    default;
GpmBrowserAuthenticatorRequest::MakeCredential::~MakeCredential() = default;

GpmBrowserAuthenticatorRequest::GetAssertion::GetAssertion(
    std::vector<std::vector<uint8_t>> trusted_vault_keys,
    sync_pb::WebauthnCredentialSpecifics entity)
    : trusted_vault_keys(std::move(trusted_vault_keys)),
      entity(std::move(entity)) {}
GpmBrowserAuthenticatorRequest::GetAssertion::GetAssertion(GetAssertion&&) =
    default;
GpmBrowserAuthenticatorRequest::GetAssertion&
GpmBrowserAuthenticatorRequest::GetAssertion::operator=(GetAssertion&&) =
    default;
GpmBrowserAuthenticatorRequest::GetAssertion::~GetAssertion() = default;

GpmBrowserAuthenticatorRequest::GpmBrowserAuthenticatorRequest(
    std::variant<MakeCredential, GetAssertion> data)
    : data(std::move(data)) {}
GpmBrowserAuthenticatorRequest::GpmBrowserAuthenticatorRequest(
    GpmBrowserAuthenticatorRequest&&) = default;
GpmBrowserAuthenticatorRequest& GpmBrowserAuthenticatorRequest::operator=(
    GpmBrowserAuthenticatorRequest&&) = default;
GpmBrowserAuthenticatorRequest::~GpmBrowserAuthenticatorRequest() = default;

GpmBrowserAuthenticator::GpmBrowserAuthenticator(
    GpmBrowserAuthenticatorRequest request)
    : id_(crypto::RandBytesAsArray<8>()), request_(std::move(request)) {
  if (const auto* make =
          std::get_if<GpmBrowserAuthenticatorRequest::MakeCredential>(
              &request_.data)) {
    CHECK(!make->trusted_vault_key.key.empty());
  } else {
    const auto& get =
        std::get<GpmBrowserAuthenticatorRequest::GetAssertion>(request_.data);
    CHECK(!get.trusted_vault_keys.empty());
  }
}

GpmBrowserAuthenticator::~GpmBrowserAuthenticator() = default;

void GpmBrowserAuthenticator::InitializeAuthenticator(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

void GpmBrowserAuthenticator::MakeCredential(
    device::CtapMakeCredentialRequest request,
    device::MakeCredentialOptions options,
    MakeCredentialCallback callback) {
  CHECK(request_.save_passkey_callback);
  CHECK(request.user.name.has_value());
  CHECK(request.user.display_name.has_value());

  const auto& make_credential_data =
      std::get<GpmBrowserAuthenticatorRequest::MakeCredential>(request_.data);

  if (std::ranges::none_of(
          request.public_key_credential_params.public_key_credential_params(),
          [](const auto& param) {
            return param.type == device::CredentialType::kPublicKey &&
                   passkey_model_utils::IsSupportedAlgorithm(param.algorithm);
          })) {
    FIDO_LOG(ERROR) << "No supported algorithm in GPM MakeCredential request";
    std::move(callback).Run(device::MakeCredentialStatus::kNoCommonAlgorithms,
                            std::nullopt);
    return;
  }

  if (std::ranges::any_of(
          request.exclude_list, [&make_credential_data](const auto& excluded) {
            return std::ranges::contains(make_credential_data.existing_cred_ids,
                                         excluded.id);
          })) {
    FIDO_LOG(EVENT) << "Matching credential ID in exclude list for GPM";
    std::move(callback).Run(
        device::MakeCredentialStatus::kUserConsentButCredentialExcluded,
        std::nullopt);
    return;
  }

  passkey_model_utils::ExtensionOutputData extension_output_data;
  // TODO(crbug.com/570170982): Plumb DeviceAuthorizationKey once
  // DeviceAuthorizationService is available on desktop.
  auto [passkey, public_key_spki_der] =
      passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
          request.rp.id,
          webauthn::PasskeyModel::UserEntity(
              request.user.id, *request.user.name, *request.user.display_name),
          make_credential_data.trusted_vault_key.key,
          make_credential_data.trusted_vault_key.version,
          /*device_authorization_key=*/std::nullopt,
          passkey_model_utils::ExtensionInputData(request.prf,
                                                  request.prf_input),
          &extension_output_data);

  device::AuthenticatorMakeCredentialResponse response(
      device::FidoTransportProtocol::kInternal,
      passkey_model_utils::MakeAttestationObjectForCreation(
          request.rp.id, request_.user_verified,
          base::as_byte_span(passkey.credential_id()), public_key_spki_der));
  response.is_resident_key = true;
  response.transports.emplace(kGpmTransports);
  response.prf_enabled = request.prf;
  if (!extension_output_data.prf_result.empty()) {
    response.prf_results = std::move(extension_output_data.prf_result);
  }

  request_.save_passkey_callback.Run(std::move(passkey));
  std::move(callback).Run(device::MakeCredentialStatus::kSuccess,
                          std::move(response));
}

void GpmBrowserAuthenticator::GetAssertion(
    device::CtapGetAssertionRequest request,
    device::CtapGetAssertionOptions options,
    GetAssertionCallback callback) {
  const auto& get_assertion_data =
      std::get<GpmBrowserAuthenticatorRequest::GetAssertion>(request_.data);
  const sync_pb::WebauthnCredentialSpecifics& entity =
      get_assertion_data.entity;
  const base::span<const uint8_t> credential_id =
      base::as_byte_span(entity.credential_id());

  CHECK_EQ(request.rp_id, entity.rp_id());
  CHECK_EQ(request.allow_list.size(), 1u);
  CHECK(std::ranges::equal(request.allow_list[0].id, credential_id));

  // `TrustedVaultClient::FetchKeys()` does not expose key versions and
  // `entity.key_version()` might be unset, so try keys from newest to oldest.
  std::optional<sync_pb::WebauthnCredentialSpecifics_Encrypted> secrets;
  for (const std::vector<uint8_t>& trusted_vault_key :
       base::Reversed(get_assertion_data.trusted_vault_keys)) {
    sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
    // TODO(crbug.com/570170982): Plumb DeviceAuthorizationKey once
    // DeviceAuthorizationService is available on desktop.
    if (passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
            trusted_vault_key,
            /*device_authorization_key=*/base::span<const uint8_t>(), entity,
            &decrypted)) {
      secrets = std::move(decrypted);
      break;
    }
  }
  if (!secrets) {
    FIDO_LOG(ERROR) << "Failed to decrypt GPM passkey";
    std::move(callback).Run(device::GetAssertionStatus::kEnclaveError, {});
    return;
  }

  device::AuthenticatorData authenticator_data =
      passkey_model_utils::MakeAuthenticatorDataForAssertion(
          request.rp_id, request_.user_verified);
  std::vector<uint8_t> signed_over_data =
      authenticator_data.SerializeToByteArray();
  signed_over_data.insert(signed_over_data.end(),
                          request.client_data_hash.begin(),
                          request.client_data_hash.end());
  std::optional<std::vector<uint8_t>> signature =
      passkey_model_utils::GenerateEcSignature(
          base::as_byte_span(secrets->private_key()), signed_over_data);
  if (!signature) {
    FIDO_LOG(ERROR) << "Failed to sign with GPM passkey";
    std::move(callback).Run(device::GetAssertionStatus::kEnclaveError, {});
    return;
  }

  device::AuthenticatorGetAssertionResponse response(
      std::move(authenticator_data), std::move(*signature),
      device::FidoTransportProtocol::kInternal);
  response.credential = device::PublicKeyCredentialDescriptor(
      device::CredentialType::kPublicKey, base::ToVector(credential_id));
  response.user_entity = device::PublicKeyCredentialUserEntity(
      base::ToVector(base::as_byte_span(entity.user_id())));

  if (const device::PRFInput* prf_input =
          SelectPrfInput(request.prf_inputs, credential_id)) {
    response.hmac_secret =
        passkey_model_utils::EvaluateHMAC(*secrets, *prf_input);
  }

  std::vector<device::AuthenticatorGetAssertionResponse> responses;
  responses.emplace_back(std::move(response));
  std::move(callback).Run(device::GetAssertionStatus::kSuccess,
                          std::move(responses));
}

std::optional<base::span<const int32_t>>
GpmBrowserAuthenticator::GetAlgorithms() {
  static constexpr std::array<int32_t, 1> kSupportedAlgorithms = {
      base::strict_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)};
  return kSupportedAlgorithms;
}

void GpmBrowserAuthenticator::Cancel() {}

device::AuthenticatorType GpmBrowserAuthenticator::GetType() const {
  // TODO(crbug.com/570170982): Change to `kGpmBrowser`.
  return device::AuthenticatorType::kEnclave;
}

std::string GpmBrowserAuthenticator::GetId() const {
  return "gpm-browser-" + base::HexEncode(id_);
}

const device::AuthenticatorSupportedOptions& GpmBrowserAuthenticator::Options()
    const {
  static const base::NoDestructor<device::AuthenticatorSupportedOptions>
      options(GpmBrowserAuthenticatorOptions());
  return *options;
}

std::optional<device::FidoTransportProtocol>
GpmBrowserAuthenticator::AuthenticatorTransport() const {
  return device::FidoTransportProtocol::kInternal;
}

base::WeakPtr<device::FidoAuthenticator> GpmBrowserAuthenticator::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}
