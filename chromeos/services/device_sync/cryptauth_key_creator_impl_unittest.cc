// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_creator_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/fake_secure_message_delegate.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/cryptauth_key_bundle.h"
#include "chromeos/services/device_sync/cryptauth_key_creator.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace device_sync {

namespace {

const char kFakeServerEphemeralDhPublicKeyMaterial[] = "server_ephemeral_dh";
const char kFakeClientEphemeralDhPublicKeyMaterial[] = "client_ephemeral_dh";
const char kFakeAsymmetricKeyHandle[] = "asymmetric_key_handle";
const char kFakePublicKeyMaterial[] = "public_key";
const char kFakeSymmetricKeyHandle[] = "symmetric_key_handle";
const char kFakeProvidedPublicKeyMaterial[] = "provided_public_key";
const char kFakeProvidedPrivateKeyMaterial[] = "provided_private_key";

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case cryptauthv2::KeyType::RAW128:
      return 16u;
    case cryptauthv2::KeyType::RAW256:
      return 32u;
    default:
      NOTREACHED();
      return 0u;
  }
}

}  // namespace

class DeviceSyncCryptAuthKeyCreatorImplTest : public testing::Test {
 protected:
  DeviceSyncCryptAuthKeyCreatorImplTest()
      : fake_secure_message_delegate_factory_(
            std::make_unique<multidevice::FakeSecureMessageDelegateFactory>()),
        fake_server_ephemeral_dh_(CryptAuthKey(
            kFakeServerEphemeralDhPublicKeyMaterial,
            fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
                kFakeServerEphemeralDhPublicKeyMaterial),
            CryptAuthKey::Status::kActive,
            cryptauthv2::KeyType::P256)) {}

  ~DeviceSyncCryptAuthKeyCreatorImplTest() override = default;

  void SetUp() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        fake_secure_message_delegate_factory_.get());

    key_creator_ = CryptAuthKeyCreatorImpl::Factory::Get()->BuildInstance();
  }

  void TearDown() override {
    multidevice::SecureMessageDelegateImpl::Factory::SetInstanceForTesting(
        nullptr);
  }

  void CallCreateKeys(
      const base::flat_map<CryptAuthKeyBundle::Name,
                           CryptAuthKeyCreator::CreateKeyData>& keys_to_create,
      const base::Optional<CryptAuthKey>& server_ephemeral_dh) {
    key_creator_->CreateKeys(
        keys_to_create, server_ephemeral_dh,
        base::BindOnce(&DeviceSyncCryptAuthKeyCreatorImplTest::OnKeysCreated,
                       base::Unretained(this)));
  }

  void VerifyKeyCreation(
      const base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>&
          expected_new_keys,
      const base::Optional<CryptAuthKey>& expected_client_ephemeral_dh) {
    EXPECT_TRUE(new_keys_);
    EXPECT_TRUE(client_ephemeral_dh_);
    EXPECT_EQ(expected_new_keys, *new_keys_);
    EXPECT_EQ(expected_client_ephemeral_dh, *client_ephemeral_dh_);
  }

  std::string DeriveSecret(const CryptAuthKey& server_ephemeral_dh,
                           const CryptAuthKey& client_ephemeral_dh) {
    std::string derived_key;
    fake_secure_message_delegate()->DeriveKey(
        client_ephemeral_dh.private_key(), server_ephemeral_dh.public_key(),
        base::Bind([](std::string* derived_key,
                      const std::string& key) { *derived_key = key; },
                   &derived_key));
    return derived_key;
  }

  CryptAuthKey DeriveSymmetricKey(
      const CryptAuthKeyBundle::Name& bundle_name,
      const CryptAuthKeyCreator::CreateKeyData& key_to_create,
      const CryptAuthKey& client_ephemeral_dh) {
    std::string expected_handshake_secret =
        DeriveSecret(fake_server_ephemeral_dh_, client_ephemeral_dh);

    std::string expected_symmetric_key_material = crypto::HkdfSha256(
        expected_handshake_secret, kCryptAuthSymmetricKeyDerivationSalt,
        CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name),
        NumBytesForSymmetricKeyType(key_to_create.type));

    return CryptAuthKey(expected_symmetric_key_material, key_to_create.status,
                        key_to_create.type, key_to_create.handle);
  }

  multidevice::FakeSecureMessageDelegate* fake_secure_message_delegate() {
    return fake_secure_message_delegate_factory_->instance();
  }

  const CryptAuthKey& fake_server_ephemeral_dh() {
    return fake_server_ephemeral_dh_;
  }

 private:
  void OnKeysCreated(
      const base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>& new_keys,
      const base::Optional<CryptAuthKey>& client_ephemeral_dh) {
    new_keys_ = new_keys;
    client_ephemeral_dh_ = client_ephemeral_dh;
  }

  std::unique_ptr<multidevice::FakeSecureMessageDelegateFactory>
      fake_secure_message_delegate_factory_;
  std::unique_ptr<CryptAuthKeyCreator> key_creator_;

  CryptAuthKey fake_server_ephemeral_dh_;

  // A null value (for the outermost Optional) indicates that OnKeysCreated()
  // was not called.
  base::Optional<base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey>>
      new_keys_;
  base::Optional<base::Optional<CryptAuthKey>> client_ephemeral_dh_;

  DISALLOW_COPY_AND_ASSIGN(DeviceSyncCryptAuthKeyCreatorImplTest);
};

TEST_F(DeviceSyncCryptAuthKeyCreatorImplTest, AsymmetricKeyCreation) {
  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      keys_to_create = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           CryptAuthKeyCreator::CreateKeyData(CryptAuthKey::Status::kActive,
                                              cryptauthv2::KeyType::P256,
                                              kFakeAsymmetricKeyHandle)}};

  CryptAuthKey expected_asymmetric_key(
      kFakePublicKeyMaterial,
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
          kFakePublicKeyMaterial),
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
      kFakeAsymmetricKeyHandle);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> expected_new_keys = {
      {CryptAuthKeyBundle::Name::kUserKeyPair, expected_asymmetric_key}};

  fake_secure_message_delegate()->set_next_public_key(kFakePublicKeyMaterial);

  CallCreateKeys(keys_to_create, base::nullopt /* fake_server_ephemeral_dh */);
  VerifyKeyCreation(expected_new_keys,
                    base::nullopt /* expected_client_ephemeral_dh */);
}

TEST_F(DeviceSyncCryptAuthKeyCreatorImplTest, SymmetricKeyCreation) {
  CryptAuthKeyCreator::CreateKeyData symmetric_key_to_create(
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::RAW256,
      kFakeSymmetricKeyHandle);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      keys_to_create = {{CryptAuthKeyBundle::Name::kLegacyMasterKey,
                         symmetric_key_to_create}};

  CryptAuthKey expected_client_ephemeral_dh(
      kFakeClientEphemeralDhPublicKeyMaterial,
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
          kFakeClientEphemeralDhPublicKeyMaterial),
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  CryptAuthKey expected_symmetric_key =
      DeriveSymmetricKey(CryptAuthKeyBundle::Name::kLegacyMasterKey,
                         symmetric_key_to_create, expected_client_ephemeral_dh);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> expected_new_keys = {
      {CryptAuthKeyBundle::Name::kLegacyMasterKey, expected_symmetric_key}};

  fake_secure_message_delegate()->set_next_public_key(
      kFakeClientEphemeralDhPublicKeyMaterial);

  CallCreateKeys(keys_to_create, fake_server_ephemeral_dh());
  VerifyKeyCreation(expected_new_keys, expected_client_ephemeral_dh);
}

TEST_F(DeviceSyncCryptAuthKeyCreatorImplTest,
       MultipleKeyCreation_KeyMaterialProvidedForAsymmetricKey) {
  CryptAuthKeyCreator::CreateKeyData symmetric_key_to_create(
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::RAW256,
      kFakeSymmetricKeyHandle);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKeyCreator::CreateKeyData>
      keys_to_create = {
          {CryptAuthKeyBundle::Name::kUserKeyPair,
           CryptAuthKeyCreator::CreateKeyData(
               CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
               kFakeAsymmetricKeyHandle, kFakeProvidedPublicKeyMaterial,
               kFakeProvidedPrivateKeyMaterial)},
          {CryptAuthKeyBundle::Name::kLegacyMasterKey,
           symmetric_key_to_create}};

  CryptAuthKey expected_client_ephemeral_dh(
      kFakeClientEphemeralDhPublicKeyMaterial,
      fake_secure_message_delegate()->GetPrivateKeyForPublicKey(
          kFakeClientEphemeralDhPublicKeyMaterial),
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256);

  CryptAuthKey expected_asymmetric_key(
      kFakeProvidedPublicKeyMaterial, kFakeProvidedPrivateKeyMaterial,
      CryptAuthKey::Status::kActive, cryptauthv2::KeyType::P256,
      kFakeAsymmetricKeyHandle);

  CryptAuthKey expected_symmetric_key =
      DeriveSymmetricKey(CryptAuthKeyBundle::Name::kLegacyMasterKey,
                         symmetric_key_to_create, expected_client_ephemeral_dh);

  base::flat_map<CryptAuthKeyBundle::Name, CryptAuthKey> expected_new_keys = {
      {CryptAuthKeyBundle::Name::kUserKeyPair, expected_asymmetric_key},
      {CryptAuthKeyBundle::Name::kLegacyMasterKey, expected_symmetric_key}};

  // There is no need to generate an asymmetric key for kUserKeyPair since we
  // passed in the key material to CreateKeyData.
  fake_secure_message_delegate()->set_next_public_key(
      kFakeClientEphemeralDhPublicKeyMaterial);

  CallCreateKeys(keys_to_create, fake_server_ephemeral_dh());
  VerifyKeyCreation(expected_new_keys, expected_client_ephemeral_dh);
}

}  // namespace device_sync

}  // namespace chromeos
