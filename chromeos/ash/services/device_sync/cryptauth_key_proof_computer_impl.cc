// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/device_sync/cryptauth_key_proof_computer_impl.h"

#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_view_util.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/services/device_sync/cryptauth_key.h"
#include "chromeos/ash/services/device_sync/proto/cryptauth_common.pb.h"
#include "crypto/hkdf.h"
#include "crypto/hmac.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"

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
      NOTREACHED();
  }
}

bool IsValidAsymmetricKey(const CryptAuthKey& key) {
  return key.IsAsymmetricKey() && !key.private_key().empty() &&
         key.type() == cryptauthv2::KeyType::P256;
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

  return std::string(base::as_string_view(crypto::hmac::SignSha256(
      base::as_byte_span(derived_symmetric_key_material),
      base::as_byte_span(payload))));
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

  auto private_key = crypto::keypair::PrivateKey::FromPrivateKeyInfo(
      base::as_byte_span(asymmetric_key.private_key()));
  if (!private_key || !private_key->IsEc()) {
    PA_LOG(ERROR) << "Failed to compute asymmetric key proof for key handle "
                  << asymmetric_key.handle() << ". "
                  << "Invalid private key material; expect DER-encoded PKCS #8 "
                  << "PrivateKeyInfo format (RFC 5208).";
    return std::nullopt;
  }

  auto signature =
      crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256,
                         *private_key, base::as_byte_span(salt + payload));
  return std::string(base::as_string_view(signature));
}

}  // namespace device_sync

}  // namespace ash
