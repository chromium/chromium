// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/ec_private_key.h"
#include "crypto/ec_signature_creator.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"

namespace ash {

namespace device_sync {

namespace {

size_t NumBytesForSymmetricKeyType(cryptauthv2::KeyType key_type) {
  switch (key_type) {
    case (cryptauthv2::KeyType::RAW128):
      return 16u;
    case (cryptauthv2::KeyType::RAW256):
      return 32u;
    default:
      NOTREACHED_IN_MIGRATION();
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

}  // namespace

// static
CryptAuthKeyProofComputerImpl::Factory*
    CryptAuthKeyProofComputerImpl::Factory::test_factory_ = nullptr;

// static
std::unique_ptr<CryptAuthKeyProofComputer>
CryptAuthKeyProofComputerImpl::Factory::Create() {
  if (test_factory_)
    return test_factory_->CreateInstance();

  return base::WrapUnique(new CryptAuthKeyProofComputerImpl());
}

// static
void CryptAuthKeyProofComputerImpl::Factory::SetFactoryForTesting(
    Factory* test_factory) {
  test_factory_ = test_factory;
}

CryptAuthKeyProofComputerImpl::Factory::~Factory() = default;

CryptAuthKeyProofComputerImpl::CryptAuthKeyProofComputerImpl() = default;

CryptAuthKeyProofComputerImpl::~CryptAuthKeyProofComputerImpl() = default;

std::optional<std::string> CryptAuthKeyProofComputerImpl::ComputeKeyProof(
    const CryptAuthKey& key,
    const std::string& payload,
    const std::string& salt,
    const std::optional<std::string>& info) {
  if (key.IsAsymmetricKey())
    return ComputeAsymmetricKeyProof(key, payload, salt);

  DCHECK(info);
  return ComputeSymmetricKeyProof(key, payload, salt, *info);
}

std::optional<std::string>
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
    return std::nullopt;
  }

  return std::string(signed_payload.begin(), signed_payload.end());
}

std::optional<std::string>
CryptAuthKeyProofComputerImpl::ComputeAsymmetricKeyProof(
    const CryptAuthKey& asymmetric_key,
    const std::string& payload,
    const std::string& salt) {
  if (!IsValidAsymmetricKey(asymmetric_key)) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle()
                  << ". Invalid key type: " << asymmetric_key.type();
    return std::nullopt;
  }

  std::unique_ptr<crypto::ECPrivateKey> ec_private_key =
      crypto::ECPrivateKey::CreateFromPrivateKeyInfo(
          base::as_bytes(base::make_span(asymmetric_key.private_key())));
  if (!ec_private_key) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle() << ". "
                  << "Invalid private key material; expect DER-encoded PKCS #8 "
                  << "PrivateKeyInfo format (RFC 5208).";
    return std::nullopt;
  }

  std::unique_ptr<crypto::ECSignatureCreator> ec_signature_creator =
      crypto::ECSignatureCreator::Create(ec_private_key.get());
  if (!ec_signature_creator) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle();
    return std::nullopt;
  }

  std::string to_sign = salt + payload;
  std::vector<uint8_t> key_proof;
  bool success = ec_signature_creator->Sign(
      base::as_bytes(base::make_span(to_sign)), &key_proof);
  if (!success) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle();
    return std::nullopt;
  }

  return ByteVectorToString(key_proof);
}

}  // namespace device_sync

}  // namespace ash
