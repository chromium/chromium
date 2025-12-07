// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/android_private_key.h"

#include "base/check.h"
#include "base/synchronization/waitable_event.h"
#include "components/enterprise/client_certificates/android/browser_binding/browser_key_android.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace client_certificates {

AndroidPrivateKey::AndroidPrivateKey(std::unique_ptr<BrowserKey> key)
    : PrivateKey(PrivateKeySource::kAndroidKey, key->GetSSLPrivateKey()),
      key_(std::move(key)) {
  CHECK(key_);
}

AndroidPrivateKey::~AndroidPrivateKey() = default;

std::optional<std::vector<uint8_t>> AndroidPrivateKey::SignSlowly(
    base::span<const uint8_t> data) const {
  return key_->Sign(std::vector<uint8_t>(data.begin(), data.end()));
}

std::vector<uint8_t> AndroidPrivateKey::GetSubjectPublicKeyInfo() const {
  return key_->GetPublicKeyAsSPKI();
}

crypto::SignatureVerifier::SignatureAlgorithm AndroidPrivateKey::GetAlgorithm()
    const {
  // TODO(crbug.com/432304139): Add support for other algorithms.
  return crypto::SignatureVerifier::ECDSA_SHA256;
}

client_certificates_pb::PrivateKey AndroidPrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(PrivateKeySource::kAndroidKey));
  auto wrapped = key_->GetIdentifier();
  private_key.set_wrapped_key(std::string(wrapped.begin(), wrapped.end()));
  return private_key;
}

base::Value::Dict AndroidPrivateKey::ToDict() const {
  std::vector<uint8_t> wrapped = key_->GetIdentifier();
  if (wrapped.size() == 0) {
    return base::Value::Dict();
  }
  return BuildSerializedPrivateKey(wrapped);
}

BrowserKey::SecurityLevel AndroidPrivateKey::GetSecurityLevel() const {
  return key_->GetSecurityLevel();
}

}  // namespace client_certificates
