// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "net/ssl/openssl_private_key.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

namespace client_certificates {

ECPrivateKey::ECPrivateKey(crypto::keypair::PrivateKey key)
    : PrivateKey(PrivateKeySource::kSoftwareKey,
                 net::WrapOpenSSLPrivateKey(bssl::UpRef(key.key()))),
      key_(std::move(key)) {}

ECPrivateKey::~ECPrivateKey() = default;

std::optional<std::vector<uint8_t>> ECPrivateKey::SignSlowly(
    base::span<const uint8_t> data) const {
  return crypto::sign::Sign(crypto::sign::SignatureKind::ECDSA_SHA256, key_,
                            data);
}

std::vector<uint8_t> ECPrivateKey::GetSubjectPublicKeyInfo() const {
  return key_.ToSubjectPublicKeyInfo();
}

crypto::SignatureVerifier::SignatureAlgorithm ECPrivateKey::GetAlgorithm()
    const {
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

client_certificates_pb::PrivateKey ECPrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));

  std::vector<uint8_t> wrapped = key_.ToPrivateKeyInfo();
  private_key.set_wrapped_key(std::string(wrapped.begin(), wrapped.end()));

  return private_key;
}

base::Value::Dict ECPrivateKey::ToDict() const {
  return BuildSerializedPrivateKey(key_.ToPrivateKeyInfo());
}

}  // namespace client_certificates
