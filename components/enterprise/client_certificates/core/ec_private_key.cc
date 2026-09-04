// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/keypair.h"
#include "crypto/sign.h"
#include "net/ssl/crypto_private_key.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

ECPrivateKey::ECPrivateKey(crypto::keypair::PrivateKey key)
    : PrivateKey(PrivateKeySource::kSoftwareKey,
                 net::WrapCryptoPrivateKey(key)),
      key_(std::move(key)) {}

ECPrivateKey::~ECPrivateKey() = default;

void ECPrivateKey::Sign(
    base::span<const uint8_t> data,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
    const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](scoped_refptr<const ECPrivateKey> key, std::vector<uint8_t> data) {
            return crypto::sign::Sign(crypto::sign::ECDSA_SHA256, key->key_,
                                      data);
          },
          base::WrapRefCounted(this), base::ToVector(data)),
      std::move(callback));
}

std::vector<uint8_t> ECPrivateKey::GetSubjectPublicKeyInfo() const {
  return key_.ToSubjectPublicKeyInfo();
}

crypto::sign::SignatureKind ECPrivateKey::GetAlgorithm() const {
  return crypto::sign::ECDSA_SHA256;
}

client_certificates_pb::PrivateKey ECPrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));

  std::vector<uint8_t> wrapped = key_.ToPrivateKeyInfo();
  private_key.set_wrapped_key(std::string(wrapped.begin(), wrapped.end()));

  return private_key;
}

base::DictValue ECPrivateKey::ToDict() const {
  return BuildSerializedPrivateKey(key_.ToPrivateKeyInfo());
}

}  // namespace client_certificates
