// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_creator_impl.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "base/strings/string_util.h"
#include "chromeos/components/multidevice/secure_message_delegate_impl.h"
#include "chromeos/services/device_sync/cryptauth_enrollment_constants.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"

namespace chromeos {

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
      NOTREACHED();
      return 0u;
  }
}

}  // namespace

// static
CryptAuthKeyCreatorImpl::Factory*
    CryptAuthKeyCreatorImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthKeyCreatorImpl::Factory* CryptAuthKeyCreatorImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthKeyCreatorImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthKeyCreatorImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyCreatorImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthKeyCreator>
CryptAuthKeyCreatorImpl::Factory::BuildInstance() {
  return base::WrapUnique(new CryptAuthKeyCreatorImpl());
}

CryptAuthKeyCreatorImpl::CryptAuthKeyCreatorImpl()
    : secure_message_delegate_(
          multidevice::SecureMessageDelegateImpl::Factory::NewInstance()) {}

void CryptAuthKeyCreatorImpl::CreateKeys(
    const base::flat_map<CryptAuthKeyBundle::Name, CreateKeyData>&
        keys_to_create,
    const base::Optional<CryptAuthKey>& server_ephemeral_dh,
    CreateKeysCallback create_keys_callback) {
  DCHECK(!keys_to_create.empty());

  // Fail if CreateKeys() has already been called.
  DCHECK(num_keys_to_create_ == 0 && new_keys_.empty() &&
         !create_keys_callback_);

  num_keys_to_create_ = keys_to_create.size();
  keys_to_create_ = keys_to_create;
  server_ephemeral_dh_ = server_ephemeral_dh;
  create_keys_callback_ = std::move(create_keys_callback);

  if (IsClientEphemeralDhKeyNeeded(keys_to_create_)) {
    DCHECK(server_ephemeral_dh_ && server_ephemeral_dh_->IsAsymmetricKey());
    secure_message_delegate_->GenerateKeyPair(
        base::Bind(&CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated,
                   base::Unretained(this)));
    return;
  }

  StartKeyCreation();
}

CryptAuthKeyCreatorImpl::~CryptAuthKeyCreatorImpl() = default;

void CryptAuthKeyCreatorImpl::OnClientDiffieHellmanGenerated(
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(!public_key.empty() && !private_key.empty());

  client_ephemeral_dh_ =
      CryptAuthKey(public_key, private_key, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::P256);

  secure_message_delegate_->DeriveKey(
      client_ephemeral_dh_->private_key(), server_ephemeral_dh_->public_key(),
      base::Bind(
          &CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived,
          base::Unretained(this)));
}

void CryptAuthKeyCreatorImpl::OnDiffieHellmanHandshakeSecretDerived(
    const std::string& symmetric_key) {
  DCHECK(!symmetric_key.empty());

  dh_handshake_secret_ =
      CryptAuthKey(symmetric_key, CryptAuthKey::Status::kActive,
                   cryptauthv2::KeyType::RAW256);

  StartKeyCreation();
}

void CryptAuthKeyCreatorImpl::StartKeyCreation() {
  for (const auto& key_to_create : keys_to_create_) {
    const CryptAuthKeyBundle::Name& bundle_name = key_to_create.first;
    const CreateKeyData& key_data = key_to_create.second;

    // If the key to create is symmetric, derive a symmetric key from the
    // Diffie-Hellman handshake secrect using HKDF. The CryptAuth v2
    // Enrollment protocol specifies that the salt should be "CryptAuth
    // Enrollment" and the info should be the key handle. This process is
    // synchronous, unlike SecureMessageDelegate calls.
    if (IsValidSymmetricKeyType(key_data.type)) {
      std::string derived_symmetric_key_material = crypto::HkdfSha256(
          dh_handshake_secret_->symmetric_key(),
          kCryptAuthSymmetricKeyDerivationSalt,
          CryptAuthKeyBundle::KeyBundleNameEnumToString(bundle_name),
          NumBytesForSymmetricKeyType(key_data.type));

      OnSymmetricKeyDerived(bundle_name, derived_symmetric_key_material);

      continue;
    }

    DCHECK(IsValidAsymmetricKeyType(key_data.type));

    // If the key material was explicitly set in CreateKeyData, bypass the
    // standard key creation.
    if (key_data.public_key && key_data.private_key) {
      OnAsymmetricKeyPairGenerated(bundle_name, *key_data.public_key,
                                   *key_data.private_key);
      continue;
    }

    secure_message_delegate_->GenerateKeyPair(
        base::Bind(&CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated,
                   base::Unretained(this), key_to_create.first));
  }
}

void CryptAuthKeyCreatorImpl::OnAsymmetricKeyPairGenerated(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& public_key,
    const std::string& private_key) {
  DCHECK(num_keys_to_create_ > 0);
  DCHECK(!public_key.empty() && !private_key.empty());

  const CryptAuthKeyCreator::CreateKeyData& create_key_data =
      keys_to_create_.find(bundle_name)->second;

  new_keys_.try_emplace(bundle_name, public_key, private_key,
                        create_key_data.status, create_key_data.type,
                        create_key_data.handle);

  --num_keys_to_create_;
  if (num_keys_to_create_ == 0)
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

void CryptAuthKeyCreatorImpl::OnSymmetricKeyDerived(
    CryptAuthKeyBundle::Name bundle_name,
    const std::string& symmetric_key) {
  DCHECK(num_keys_to_create_ > 0);
  DCHECK(!symmetric_key.empty());

  const CryptAuthKeyCreator::CreateKeyData& create_key_data =
      keys_to_create_.find(bundle_name)->second;

  new_keys_.try_emplace(bundle_name, symmetric_key, create_key_data.status,
                        create_key_data.type, create_key_data.handle);

  --num_keys_to_create_;
  if (num_keys_to_create_ == 0)
    std::move(create_keys_callback_).Run(new_keys_, client_ephemeral_dh_);
}

}  // namespace device_sync

}  // namespace chromeos
