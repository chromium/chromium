// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/gpm_browser_authenticator.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_utils.h"
#include "crypto/hash.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "device/fido/attested_credential_data.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/authenticator_make_credential_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/prf_input.h"
#include "device/fido/public/fido_constants.h"
#include "device/fido/public/fido_transport_protocol.h"
#include "device/fido/public/fido_types.h"
#include "device/fido/public/public_key_credential_descriptor.h"
#include "device/fido/public/public_key_credential_params.h"
#include "device/fido/public/public_key_credential_rp_entity.h"
#include "device/fido/public/public_key_credential_user_entity.h"
#include "device/fido/public_key.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

namespace passkey_model_utils = webauthn::passkey_model_utils;

using MakeCredentialFuture = base::test::TestFuture<
    device::MakeCredentialStatus,
    std::optional<device::AuthenticatorMakeCredentialResponse>>;
using GetAssertionFuture = base::test::TestFuture<
    device::GetAssertionStatus,
    std::vector<device::AuthenticatorGetAssertionResponse>>;

constexpr char kRpId[] = "example.com";
constexpr char kClientDataJson[] = R"({"type": "webauthn.test"})";
constexpr int32_t kKeyVersion = 7;

const std::vector<uint8_t> kUserId = {1, 2, 3, 4};
const std::vector<uint8_t> kTrustedVaultKey1(32, 0x11);
const std::vector<uint8_t> kTrustedVaultKey2(32, 0x22);
const std::vector<uint8_t> kPrfInput1 = {'s', 'a', 'l', 't'};

device::CtapMakeCredentialRequest MakeCredentialRequest() {
  device::CtapMakeCredentialRequest request(
      kClientDataJson, device::PublicKeyCredentialRpEntity(kRpId),
      device::PublicKeyCredentialUserEntity(kUserId, "user@example.com",
                                            "Example User"),
      device::PublicKeyCredentialParams(
          {{device::CredentialType::kPublicKey,
            static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256)}}));
  request.resident_key_required = true;
  return request;
}

device::CtapGetAssertionRequest GetAssertionRequest(
    const sync_pb::WebauthnCredentialSpecifics& passkey) {
  device::CtapGetAssertionRequest request(kRpId, kClientDataJson);
  request.allow_list.emplace_back(
      device::CredentialType::kPublicKey,
      base::ToVector(base::as_byte_span(passkey.credential_id())));
  return request;
}

device::PRFInput MakePrfInput(
    std::optional<std::vector<uint8_t>> credential_id = std::nullopt) {
  device::PRFInput prf_input =
      device::PRFInput::FromUnhashedInputs(kPrfInput1, std::nullopt);
  prf_input.credential_id = std::move(credential_id);
  return prf_input;
}

std::pair<sync_pb::WebauthnCredentialSpecifics, std::vector<uint8_t>>
CreatePasskey(base::span<const uint8_t> trusted_vault_key,
              bool with_prf = false) {
  return passkey_model_utils::GeneratePasskeyAndEncryptSecrets(
      kRpId,
      webauthn::PasskeyModel::UserEntity(kUserId, "user@example.com",
                                         "Example User"),
      trusted_vault_key, kKeyVersion, /*device_authorization_key=*/std::nullopt,
      passkey_model_utils::ExtensionInputData(with_prf, std::nullopt),
      /*extension_output_data=*/nullptr);
}

class GpmBrowserAuthenticatorTest : public testing::Test {
 protected:
  GpmBrowserAuthenticatorRequest MakeCredentialUiRequest(
      bool user_verified = true,
      std::vector<std::vector<uint8_t>> existing_cred_ids = {}) {
    GpmBrowserAuthenticatorRequest::MakeCredential make_data(
        trusted_vault::TrustedVaultKeyAndVersion(kTrustedVaultKey2,
                                                 kKeyVersion));
    make_data.existing_cred_ids = std::move(existing_cred_ids);
    GpmBrowserAuthenticatorRequest request(std::move(make_data));
    request.user_verified = user_verified;
    request.save_passkey_callback = base::BindRepeating(
        [](std::optional<sync_pb::WebauthnCredentialSpecifics>* out,
           sync_pb::WebauthnCredentialSpecifics passkey) {
          *out = std::move(passkey);
        },
        &saved_passkey_);
    return request;
  }

  GpmBrowserAuthenticatorRequest GetAssertionUiRequest(
      sync_pb::WebauthnCredentialSpecifics passkey,
      std::vector<std::vector<uint8_t>> trusted_vault_keys =
          {kTrustedVaultKey1, kTrustedVaultKey2},
      bool user_verified = true) {
    GpmBrowserAuthenticatorRequest request(
        GpmBrowserAuthenticatorRequest::GetAssertion(
            std::move(trusted_vault_keys), std::move(passkey)));
    request.user_verified = user_verified;
    return request;
  }

  base::test::TaskEnvironment task_environment_;
  std::optional<sync_pb::WebauthnCredentialSpecifics> saved_passkey_;
};

TEST_F(GpmBrowserAuthenticatorTest, MakeCredential) {
  for (const bool user_verified : {false, true}) {
    SCOPED_TRACE(user_verified);
    saved_passkey_.reset();
    GpmBrowserAuthenticator authenticator(
        MakeCredentialUiRequest(user_verified));
    MakeCredentialFuture future;
    authenticator.MakeCredential(MakeCredentialRequest(),
                                 device::MakeCredentialOptions(),
                                 future.GetCallback());
    auto [status, response] = future.Take();
    ASSERT_EQ(status, device::MakeCredentialStatus::kSuccess);
    ASSERT_TRUE(response);

    ASSERT_TRUE(saved_passkey_);
    EXPECT_TRUE(passkey_model_utils::IsGpmPasskeyValid(*saved_passkey_));
    EXPECT_EQ(saved_passkey_->rp_id(), kRpId);
    EXPECT_EQ(base::as_byte_span(saved_passkey_->user_id()),
              base::span(kUserId));
    EXPECT_EQ(saved_passkey_->user_name(), "user@example.com");
    EXPECT_EQ(saved_passkey_->user_display_name(), "Example User");
    EXPECT_EQ(saved_passkey_->key_version(), kKeyVersion);
    sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
    EXPECT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
        kTrustedVaultKey2, /*device_authorization_key=*/{}, *saved_passkey_,
        &decrypted));

    const device::AuthenticatorData& authenticator_data =
        response->attestation_object.authenticator_data();
    EXPECT_EQ(authenticator_data.application_parameter(),
              crypto::hash::Sha256(kRpId));
    EXPECT_TRUE(authenticator_data.obtained_user_presence());
    EXPECT_EQ(authenticator_data.obtained_user_verification(), user_verified);
    EXPECT_TRUE(authenticator_data.backup_eligible());
    EXPECT_TRUE(authenticator_data.backup_state());
    EXPECT_EQ(
        authenticator_data.GetCredentialId(),
        base::ToVector(base::as_byte_span(saved_passkey_->credential_id())));
    EXPECT_EQ(response->transport_used,
              device::FidoTransportProtocol::kInternal);
    EXPECT_THAT(
        *response->transports,
        testing::UnorderedElementsAre(device::FidoTransportProtocol::kInternal,
                                      device::FidoTransportProtocol::kHybrid));
    EXPECT_EQ(response->is_resident_key, true);
    EXPECT_FALSE(response->prf_enabled);
    EXPECT_FALSE(response->prf_results);
  }
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialExcluded) {
  auto [existing_passkey, public_key] = CreatePasskey(kTrustedVaultKey2);
  GpmBrowserAuthenticator authenticator(MakeCredentialUiRequest(
      /*user_verified=*/true,
      /*existing_cred_ids=*/{base::ToVector(
          base::as_byte_span(existing_passkey.credential_id()))}));

  device::CtapMakeCredentialRequest request = MakeCredentialRequest();
  request.exclude_list.emplace_back(
      device::CredentialType::kPublicKey,
      base::ToVector(base::as_byte_span(existing_passkey.credential_id())));
  MakeCredentialFuture future;
  authenticator.MakeCredential(std::move(request),
                               device::MakeCredentialOptions(),
                               future.GetCallback());
  auto [status, response] = future.Take();
  EXPECT_EQ(status,
            device::MakeCredentialStatus::kUserConsentButCredentialExcluded);
  EXPECT_FALSE(response);
  EXPECT_FALSE(saved_passkey_);
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialExcludeListNoMatch) {
  GpmBrowserAuthenticator authenticator(MakeCredentialUiRequest());
  device::CtapMakeCredentialRequest request = MakeCredentialRequest();
  request.exclude_list.emplace_back(device::CredentialType::kPublicKey,
                                    std::vector<uint8_t>(16, 0xff));
  MakeCredentialFuture future;
  authenticator.MakeCredential(std::move(request),
                               device::MakeCredentialOptions(),
                               future.GetCallback());
  EXPECT_EQ(future.Get<0>(), device::MakeCredentialStatus::kSuccess);
  EXPECT_TRUE(saved_passkey_);
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialNoSupportedAlgorithm) {
  GpmBrowserAuthenticator authenticator(MakeCredentialUiRequest());
  device::CtapMakeCredentialRequest request = MakeCredentialRequest();
  request.public_key_credential_params = device::PublicKeyCredentialParams(
      {{device::CredentialType::kPublicKey,
        static_cast<int32_t>(device::CoseAlgorithmIdentifier::kRs256)}});
  MakeCredentialFuture future;
  authenticator.MakeCredential(std::move(request),
                               device::MakeCredentialOptions(),
                               future.GetCallback());
  auto [status, response] = future.Take();
  EXPECT_EQ(status, device::MakeCredentialStatus::kNoCommonAlgorithms);
  EXPECT_FALSE(response);
  EXPECT_FALSE(saved_passkey_);
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialWithPrf) {
  // Create a credential with an HMAC secret but don't evaluate the PRF.
  GpmBrowserAuthenticator authenticator(MakeCredentialUiRequest());
  device::CtapMakeCredentialRequest request = MakeCredentialRequest();
  request.prf = true;
  MakeCredentialFuture future;
  authenticator.MakeCredential(std::move(request),
                               device::MakeCredentialOptions(),
                               future.GetCallback());
  auto [status, response] = future.Take();
  ASSERT_EQ(status, device::MakeCredentialStatus::kSuccess);
  EXPECT_TRUE(response->prf_enabled);
  EXPECT_FALSE(response->prf_results);

  ASSERT_TRUE(saved_passkey_);
  sync_pb::WebauthnCredentialSpecifics_Encrypted decrypted;
  ASSERT_TRUE(passkey_model_utils::DecryptWebauthnCredentialSpecificsData(
      kTrustedVaultKey2, /*device_authorization_key=*/{}, *saved_passkey_,
      &decrypted));
  EXPECT_EQ(decrypted.hmac_secret().size(),
            passkey_model_utils::kHmacSecretSize);
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialWithPrfEvaluation) {
  // Create a credential with an HMAC secret, evaluate the PRF.
  std::vector<uint8_t> creation_prf_results;
  {
    GpmBrowserAuthenticator authenticator(MakeCredentialUiRequest());
    device::CtapMakeCredentialRequest request = MakeCredentialRequest();
    request.prf = true;
    request.prf_input = MakePrfInput();
    MakeCredentialFuture future;
    authenticator.MakeCredential(std::move(request),
                                 device::MakeCredentialOptions(),
                                 future.GetCallback());
    auto [status, response] = future.Take();
    ASSERT_EQ(status, device::MakeCredentialStatus::kSuccess);
    EXPECT_TRUE(response->prf_enabled);
    ASSERT_TRUE(response->prf_results);
    EXPECT_EQ(response->prf_results->size(), 32u);
    creation_prf_results = *response->prf_results;
  }
  ASSERT_TRUE(saved_passkey_);

  // PRF output matches on assertion.
  GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(*saved_passkey_));
  device::CtapGetAssertionRequest request =
      GetAssertionRequest(*saved_passkey_);
  request.prf_inputs.push_back(MakePrfInput());
  GetAssertionFuture future;
  authenticator.GetAssertion(std::move(request),
                             device::CtapGetAssertionOptions(),
                             future.GetCallback());
  auto [status, responses] = future.Take();
  ASSERT_EQ(status, device::GetAssertionStatus::kSuccess);
  ASSERT_EQ(responses.size(), 1u);
  EXPECT_EQ(responses[0].hmac_secret, creation_prf_results);
}

TEST_F(GpmBrowserAuthenticatorTest, MakeCredentialThenGetAssertion) {
  GpmBrowserAuthenticator make_authenticator(MakeCredentialUiRequest());
  MakeCredentialFuture make_future;
  make_authenticator.MakeCredential(MakeCredentialRequest(),
                                    device::MakeCredentialOptions(),
                                    make_future.GetCallback());
  auto [make_status, make_response] = make_future.Take();
  ASSERT_EQ(make_status, device::MakeCredentialStatus::kSuccess);
  ASSERT_TRUE(make_response);
  ASSERT_TRUE(saved_passkey_);

  const std::optional<device::AttestedCredentialData>& attested_data =
      make_response->attestation_object.authenticator_data().attested_data();
  ASSERT_TRUE(attested_data);
  ASSERT_TRUE(attested_data->public_key());
  ASSERT_TRUE(attested_data->public_key()->der_bytes);

  GpmBrowserAuthenticator get_authenticator(
      GetAssertionUiRequest(*saved_passkey_));
  device::CtapGetAssertionRequest get_request =
      GetAssertionRequest(*saved_passkey_);
  const auto client_data_hash = get_request.client_data_hash;
  GetAssertionFuture get_future;
  get_authenticator.GetAssertion(std::move(get_request),
                                 device::CtapGetAssertionOptions(),
                                 get_future.GetCallback());
  auto [get_status, get_responses] = get_future.Take();
  ASSERT_EQ(get_status, device::GetAssertionStatus::kSuccess);
  ASSERT_EQ(get_responses.size(), 1u);
  const device::AuthenticatorGetAssertionResponse& get_response =
      get_responses[0];

  std::vector<uint8_t> signed_over_data =
      get_response.authenticator_data.SerializeToByteArray();
  signed_over_data.insert(signed_over_data.end(), client_data_hash.begin(),
                          client_data_hash.end());
  std::optional<crypto::keypair::PublicKey> public_key =
      crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(
          *attested_data->public_key()->der_bytes);
  ASSERT_TRUE(public_key);
  EXPECT_TRUE(crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                                   *public_key, signed_over_data,
                                   get_response.signature));
}

TEST_F(GpmBrowserAuthenticatorTest, GetAssertion) {
  for (const bool user_verified : {false, true}) {
    SCOPED_TRACE(user_verified);
    auto [passkey, public_key_spki] = CreatePasskey(kTrustedVaultKey2);
    GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(
        passkey, {kTrustedVaultKey1, kTrustedVaultKey2}, user_verified));
    device::CtapGetAssertionRequest request = GetAssertionRequest(passkey);
    const auto client_data_hash = request.client_data_hash;
    GetAssertionFuture future;
    authenticator.GetAssertion(std::move(request),
                               device::CtapGetAssertionOptions(),
                               future.GetCallback());
    auto [status, responses] = future.Take();
    ASSERT_EQ(status, device::GetAssertionStatus::kSuccess);
    ASSERT_EQ(responses.size(), 1u);
    const device::AuthenticatorGetAssertionResponse& response = responses[0];

    EXPECT_EQ(response.authenticator_data.application_parameter(),
              crypto::hash::Sha256(kRpId));
    EXPECT_TRUE(response.authenticator_data.obtained_user_presence());
    EXPECT_EQ(response.authenticator_data.obtained_user_verification(),
              user_verified);
    EXPECT_FALSE(response.authenticator_data.attested_data());
    ASSERT_TRUE(response.credential);
    EXPECT_EQ(response.credential->id,
              base::ToVector(base::as_byte_span(passkey.credential_id())));
    ASSERT_TRUE(response.user_entity);
    EXPECT_EQ(response.user_entity->id, kUserId);
    EXPECT_EQ(response.transport_used,
              device::FidoTransportProtocol::kInternal);
    EXPECT_FALSE(response.hmac_secret);

    std::vector<uint8_t> signed_over_data =
        response.authenticator_data.SerializeToByteArray();
    signed_over_data.insert(signed_over_data.end(), client_data_hash.begin(),
                            client_data_hash.end());
    std::optional<crypto::keypair::PublicKey> public_key =
        crypto::keypair::PublicKey::FromSubjectPublicKeyInfo(public_key_spki);
    ASSERT_TRUE(public_key);
    EXPECT_TRUE(crypto::sign::Verify(crypto::sign::SignatureKind::ECDSA_SHA256,
                                     *public_key, signed_over_data,
                                     response.signature));
  }
}

// Credentials encrypted with a previous trusted vault secret can still be
// asserted.
TEST_F(GpmBrowserAuthenticatorTest, GetAssertionWithOlderTrustedVaultKey) {
  auto [passkey, public_key_spki] = CreatePasskey(kTrustedVaultKey1);
  GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(passkey));
  GetAssertionFuture future;
  authenticator.GetAssertion(GetAssertionRequest(passkey),
                             device::CtapGetAssertionOptions(),
                             future.GetCallback());
  EXPECT_EQ(future.Get<0>(), device::GetAssertionStatus::kSuccess);
}

// Assertion fails when none of the provided trusted vault keys can decrypt the
// passkey.
TEST_F(GpmBrowserAuthenticatorTest, GetAssertionWrongTrustedVaultKey) {
  auto [passkey, public_key_spki] = CreatePasskey(kTrustedVaultKey1);
  GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(
      passkey, /*trusted_vault_keys=*/{kTrustedVaultKey2}));
  GetAssertionFuture future;
  authenticator.GetAssertion(GetAssertionRequest(passkey),
                             device::CtapGetAssertionOptions(),
                             future.GetCallback());
  auto [status, responses] = future.Take();
  EXPECT_EQ(status, device::GetAssertionStatus::kEnclaveError);
  EXPECT_TRUE(responses.empty());
}

TEST_F(GpmBrowserAuthenticatorTest, GetAssertionPrf) {
  auto [passkey, public_key_spki] =
      CreatePasskey(kTrustedVaultKey2, /*with_prf=*/true);
  const std::vector<uint8_t> credential_id =
      base::ToVector(base::as_byte_span(passkey.credential_id()));

  auto evaluate = [&](std::vector<device::PRFInput> prf_inputs)
      -> std::optional<std::vector<uint8_t>> {
    GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(passkey));
    device::CtapGetAssertionRequest request = GetAssertionRequest(passkey);
    request.prf_inputs = std::move(prf_inputs);
    GetAssertionFuture future;
    authenticator.GetAssertion(std::move(request),
                               device::CtapGetAssertionOptions(),
                               future.GetCallback());
    auto [status, responses] = future.Take();
    EXPECT_EQ(status, device::GetAssertionStatus::kSuccess);
    if (responses.size() != 1) {
      return std::nullopt;
    }
    return responses[0].hmac_secret;
  };

  // Evaluate default PRF input.
  std::optional<std::vector<uint8_t>> default_result =
      evaluate({MakePrfInput()});
  ASSERT_TRUE(default_result);
  EXPECT_EQ(default_result->size(), 32u);

  EXPECT_EQ(evaluate({MakePrfInput(credential_id)}), default_result);

  // Evaluate per-credential PRF input.
  device::PRFInput other_input = MakePrfInput(credential_id);
  other_input.input1 = {'o', 't', 'h', 'e', 'r'};
  other_input.HashInputsIntoSalts();
  std::optional<std::vector<uint8_t>> specific_result =
      evaluate({MakePrfInput(), other_input});
  ASSERT_TRUE(specific_result);
  EXPECT_NE(specific_result, default_result);

  // Evaluate per-credential PRF input for an unrecognized ID.
  EXPECT_FALSE(evaluate({MakePrfInput(std::vector<uint8_t>(16, 0xff))}));
}

// PRF can be evaluated even when the passkey was created without PRF enabled.
TEST_F(GpmBrowserAuthenticatorTest, GetAssertionPrfWithoutHmacSecret) {
  auto [passkey, public_key_spki] =
      CreatePasskey(kTrustedVaultKey2, /*with_prf=*/false);
  GpmBrowserAuthenticator authenticator(GetAssertionUiRequest(passkey));
  device::CtapGetAssertionRequest request = GetAssertionRequest(passkey);
  request.prf_inputs.push_back(MakePrfInput());
  GetAssertionFuture future;
  authenticator.GetAssertion(std::move(request),
                             device::CtapGetAssertionOptions(),
                             future.GetCallback());
  auto [status, responses] = future.Take();
  ASSERT_EQ(status, device::GetAssertionStatus::kSuccess);
  ASSERT_EQ(responses.size(), 1u);
  ASSERT_TRUE(responses[0].hmac_secret);
  EXPECT_EQ(responses[0].hmac_secret->size(), 32u);
}

}  // namespace
