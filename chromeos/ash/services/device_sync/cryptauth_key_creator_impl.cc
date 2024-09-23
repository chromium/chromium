// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_creator_impl.h"

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_util.h"
#include "chromeos/ash/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/ash/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"

namespace ash {

namespace device_sync {

namespace {

bool IsValidSymmetricKeyType(const cryptauthv2::KeyType& type) {
  return type == cryptauthv2::KeyType::RAW128 ||
         type == cryptauthv2::KeyType::RAW256;
}

bool IsValidAsymmetricKeyType(const cryptauthv2::KeyType& type) {
  return type == cryptauthv2::KeyType::P256;
}

// If we need to create any symmetric keys, we first need to generate the
// client's ephemeral Diffie-Hellman key-pair and derive the handshake secret
// from the server's public key and the client's private key. This secret is
// used to derive new symmetric keys using HKDF.
bool IsClientEphemeralDhKeyNeeded(
    const base::flat_map<CryptAuthKeyBundle::Name,
                         CryptAuthKeyCreator::CreateKeyData>& keys_to_create) {
  for (const auto& key_to_create : keys_to_create) {
    if (IsValidSymmetricKeyType(key_to_create.second.type))
      return true;
  }

  return false;
}

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case cryptauthv2::KeyType::RAW128:
      return 16u;
    case cryptauthv2::KeyType::RAW256:
      return 32u;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0u;
  }
}

}  // namespace

// static
CryptAuthKeyCreatorImpl::Factory*
    CryptAuthKeyCreatorImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthKeyCreator>
CryptAuthKeyCreatorImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new CryptAuthKeyCreatorImpl());
}

// static
void CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyCreatorImpl::Factory::~Factory() = default;

CryptAuthKeyCreatorImpl::CryptAuthKeyCreatorImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::Create()) {}

CryptAuthKeyCreatorImpl::~CryptAuthKeyCreatorImpl() = default;

void CryptAuthKeyCreatorImpl::CreateKeys(
    const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
        keys_to_create,
    const std::optional<CryptAuthKey>& server_ephemeral_dh,
    CreateKeysCallback create_keys_callback) {
  DCHECK(!keys_to_create.empty());

  // Fail if CreateKeys() has already been called.
  DCHECK(num_keys_to_create_ == 0 && new_keys_.empty() &&
         !create_keys_callback_);

  num_keys_to_create_ = keys_to_create.size();
  keys_to_create_ = keys_to_create;
  create_keys_callback_ = std::move(create_keys_callback);

  if (IsClientEphemeralDhKeyNeeded(keys_to_create_)) {
    DCHECK(server_ephemeral_dh && server_ephemeral_dh->IsAsymmetricKey());
    secure_message_delegate_->GenerateKeyPair(
        base::BindOnce(&CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated,
                       base::Unretained(this), *server_ephemeral_dh));
    return;
  }

  StartKeyCreation(std::nullopt /* dh_handshake_secret */);
}

void CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated(
    const CryptAuthKey& server_ephemeral_dh,
    const std::string& public_key,
    const std::string& private_key) {
  // If the client ephemeral key-pair generation failed, we cannot generate the
  // Diffie-Hellman handshake secret and, consequently, cannot generate
  // symmetric keys. Start the key creation process with a null
  // |dh_handshake_secret|; the symmetric key creation code will handle the
  // errors.
  if (public_key.empty() || private_key.empty()) {
    StartKeyCreation(std::nullopt /* dh_handshake_secret */);
    return;
  }

  client_ephemeral_dh_ =
      CryptAuthKey(public_key, private_key, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::P256);

  secure_message_delegate_->DeriveKey(
      client_ephemeral_dh_->private_key(), server_ephemeral_dh.public_key(),
      base::BindOnce(
          &CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived,
          base::Unretained(this)));
}

void CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived(
    const std::string& symmetric_key) {
  // If the Diffie-Hellman handshake secret could not be derived, then we cannot
  // generate symmetric keys. Start the key creation process with a null
  // |dh_handshake_secret|; the symmetric key creation code will handle the
  // errors.
  if (symmetric_key.empty()) {
    StartKeyCreation(std::nullopt /* dh_handshake_secret */);
    return;
  }

  StartKeyCreation(CryptAuthKey(symmetric_key, CryptAuthKey::Status::kActive,
                                cryptauthv2::KeyType::RAW256));
}

void CryptAuthKeyCreatorImpl::StartKeyCreation(
    const std::optional<CryptAuthKey>& dh_handshake_secret) {
  for (const auto& key_to_create : keys_to_create_) {
    const CryptAuthKeyBundle::Name& bundle_name = key_to_create.first;
    const CreateKeyData& key_data = key_to_create.second;

    // If the key to create is symmetric, derive a symmetric key from the
    // Diffie-Hellman handshake secrect using HKDF. The CryptAuth v2
    // Enrollment protocol specifies that the salt should be "CryptAuth
    // Enrollment" and the info should be the key handle. This process is
    // synchronous, unlike SecureMessageDelegate calls.
    if (IsValidSymmetricKeyType(key_data.type)) {
      // Without a Diffie-Hellman secret, no symmetric keys can be created.
      if (!dh_handshake_secret) {
        OnSymmetricKeyDerived(bundle_name, std::string() /* symmetric_key */);
        continue;
      }

      std::string derived_symmetric_key_material = crypto::HkdfSha256(
          dh_handshake_secret->symmetric_key(),
          kCryptAuthSymmetricKeyDerivationSalt,
          CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name),
          NumBytesForSymmetricKeyType(key_data.type));

      OnSymmetricKeyDerived(bundle_name, derived_symmetric_key_material);

      continue;
    }

    DCHECK(IsValidAsymmetricKeyType(key_data.type));

    // If the key material was explicitly set in CreateKeyData, bypass the
    // standard key creation.
    if (key_data.public_key) {
      DCHECK(key_data.private_key);
      OnAsymmetricKeyPairGenerated(bundle_name, *key_data.public_key,
                                   *key_data.private_key);
      continue;
    }

    secure_message_delegate_->GenerateKeyPair(
        base::BindOnce(&CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated,
                       base::Unretained(this), key_to_create.first));
  }
}

void CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(num_keys_to_create_ > 0);
  if (public_key.empty() || private_key.empty()) {
    // Use null CryptAuthKey if key generation failed.
    new_keys_.insert_or_assign(bundle_name, std::nullopt);
  } else {
    const CryptAuthKeyCreator::CreateKeyData& create_key_data =
        keys_to_create_.find(bundle_name)->second;

    new_keys_.insert_or_assign(
        bundle_name,
        CryptAuthKey(public_key, private_key, create_key_data.status,
                     create_key_data.type, create_key_data.handle));
  }

  --num_keys_to_create_;
  if (num_keys_to_create_ == 0)
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

void CryptAuthKeyCreatorImpl::OnSymmetricKeyDerived(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& symmetric_key) {
  DCHECK(num_keys_to_create_ > 0);
  if (symmetric_key.empty()) {
    // Use null CryptAuthKey if key generation failed.
    new_keys_.insert_or_assign(bundle_name, std::nullopt);
  } else {
    const CryptAuthKeyCreator::CreateKeyData& create_key_data =
        keys_to_create_.find(bundle_name)->second;

    new_keys_.insert_or_assign(
        bundle_name,
        CryptAuthKey(symmetric_key, create_key_data.status,
                     create_key_data.type, create_key_data.handle));
  }

  --num_keys_to_create_;
  if (num_keys_to_create_ == 0)
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

}  // namespace device_sync

}  // namespace ash
