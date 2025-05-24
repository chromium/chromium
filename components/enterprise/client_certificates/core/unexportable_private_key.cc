// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key.h"

#include "base/check.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/unexportable_key.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

UnexportablePrivateKey::UnexportablePrivateKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key)
    : UnexportablePrivateKey(std::move(key),
                             PrivateKeySource::kUnexportableKey) {}

UnexportablePrivateKey::UnexportablePrivateKey(
    std::unique_ptr<crypto::UnexportableSigningKey> key,
    PrivateKeySource key_source)
    : PrivateKey(key_source,
                 SSLPrivateKeyFromUnexportableSigningKeySlowly(*key)),
      key_(std::move(key)) {
  CHECK(key_);
}

UnexportablePrivateKey::~UnexportablePrivateKey() = default;

std::optional<std::vector<uint8_t>> UnexportablePrivateKey::SignSlowly(
    base::span<const uint8_t> data) const {
  return key_->SignSlowly(data);
}

std::vector<uint8_t> UnexportablePrivateKey::GetSubjectPublicKeyInfo() const {
  return key_->GetSubjectPublicKeyInfo();
}

crypto::SignatureVerifier::SignatureAlgorithm
UnexportablePrivateKey::GetAlgorithm() const {
  return key_->Algorithm();
}

client_certificates_pb::PrivateKey UnexportablePrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));
  auto wrapped = key_->GetWrappedKey();
  private_key.set_wrapped_key(std::string(wrapped.begin(), wrapped.end()));
  return private_key;
}

base::Value::Dict UnexportablePrivateKey::ToDict() const {
  std::vector<uint8_t> wrapped = key_->GetWrappedKey();
  if (wrapped.size() == 0) {
    return base::Value::Dict();
  }

  return BuildSerializedPrivateKey(wrapped);
}

}  // namespace client_certificates
