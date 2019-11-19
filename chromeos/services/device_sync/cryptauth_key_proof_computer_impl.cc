// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"
#include "chromeos/components/multidevice/logging/logging.h"
#include "chromeos/services/device_sync/cryptauth_key.h"
#include "chromeos/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"

namespace chromeos {

namespace device_sync {

namespace {

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case (cryptauthv2::KeyType::RAW128):
      return 16u;
    case (cryptauthv2::KeyType::RAW256):
      return 32u;
    default:
      NOTREACHED();
      return 0u;
  }
}

bool IsValidAsymmetricKey(const CryptAuthKey& key) {
  return key.IsAsymmetricKey() && !key.private_key().empty() &&
         key.type() == cryptauthv2::KeyType::P256;
}

std::string ByteVectorToString(const std::vector<uint8_t>& byte_array) {
  return std::string(byte_array.begin(), byte_array.end());
}

std::vector<uint8_t> StringToByteVector(const std::string& str) {
  return std::vector<uint8_t>(str.begin(), str.end());
}

}  // namespace

// static
CryptAuthKeyProofComputerImpl::Factory*
    CryptAuthKeyProofComputerImpl::Factory::test_factory_ = nullptr;

// static
CryptAuthKeyProofComputerImpl::Factory*
CryptAuthKeyProofComputerImpl::Factory::Get() {
  if (test_factory_)
    return test_factory_;

  static base::NoDestructor<CryptAuthKeyProofComputerImpl::Factory> factory;
  return factory.get();
}

// static
void CryptAuthKeyProofComputerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyProofComputerImpl::Factory::~Factory() = default;

std::unique_ptr<CryptAuthKeyProofComputer>
CryptAuthKeyProofComputerImpl::Factory::BuildInstance() {
  return base::WrapUnique(new CryptAuthKeyProofComputerImpl());
}

CryptAuthKeyProofComputerImpl::CryptAuthKeyProofComputerImpl() = default;

CryptAuthKeyProofComputerImpl::~CryptAuthKeyProofComputerImpl() = default;

base::Optional<std::string> CryptAuthKeyProofComputerImpl::ComputeKeyProof(
    const CryptAuthKey& key,
    const std::string& payload,
    const std::string& salt,
    const base::Optional<std::string>& info) {
  if (key.IsAsymmetricKey())
    return ComputeAsymmetricKeyProof(key, payload, salt);

  DCHECK(info);
  return ComputeSymmetricKeyProof(key, payload, salt, *info);
}

base::Optional<std::string>
CryptAuthKeyProofComputerImpl::ComputeSymmetricKeyProof(
    const CryptAuthKey& symmetric_key,
    const std::string& payload,
    const std::string& salt,
    const std::string& info) {
  std::string derived_symmetric_key_material =
      crypto::HkdfSha256(symmetric_key.symmetric_key(), salt, info,
                         NumBytesForSymmetricKeyType(symmetric_key.type()));

  crypto::HMAC hmac(crypto::HMAC::HashAlgorithm::SHA256);
  std::vector<unsigned char> signed_payload(hmac.DigestLength());
  bool success =
      hmac.Init(derived_symmetric_key_material) &&
      hmac.Sign(payload, signed_payload.data(), signed_payload.size());

  if (!success) {
    PA_LOG(ERROR) << "Failed to compute symmetric key proof for key handle "
                  << symmetric_key.handle();
    return base::nullopt;
  }

  return std::string(signed_payload.begin(), signed_payload.end());
}

base::Optional<std::string>
CryptAuthKeyProofComputerImpl::ComputeAsymmetricKeyProof(
    const CryptAuthKey& asymmetric_key,
    const std::string& payload,
    const std::string& salt) {
  if (!IsValidAsymmetricKey(asymmetric_key)) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle()
                  << ". Invalid key type: " << asymmetric_key.type();
    return base::nullopt;
  }

  std::unique_ptr<crypto::ECPrivateKey> ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(
          StringToByteVector(asymmetric_key.private_key()));
  if (!ec_private_key) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle() << ". "
                  << "Invalid private key material; expect DER-encoded PKCS #8 "
                  << "PrivateKeyInfo format (RFC 5208).";
    return base::nullopt;
  }

  std::unique_ptr<crypto::ECSignatureCreator> ec_signature_creator =
      crypto::ECSignatureCreator::Create(ec_private_key.get());
  if (!ec_signature_creator) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle();
    return base::nullopt;
  }

  std::string to_sign = salt + payload;
  std::vector<uint8_t> key_proof;
  bool success = ec_signature_creator->Sign(
      StringToByteVector(salt + payload).data(), to_sign.size(), &key_proof);
  if (!success) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle();
    return base::nullopt;
  }

  return ByteVectorToString(key_proof);
}

}  // namespace device_sync

}  // namespace chromeos
