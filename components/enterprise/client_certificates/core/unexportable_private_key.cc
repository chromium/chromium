// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key.h"

#include "base/check.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/enterprise/client_certificates/core/ssl_key_converter.h"
#include "crypto/sign.h"
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

void UnexportablePrivateKey::Sign(
    base::span<const uint8_t> data,
    base::OnceCallback<void(std::optional<std::vector<uint8_t>>)> callback)
    const {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](scoped_refptr<const UnexportablePrivateKey> key,
             std::vector<uint8_t> data) { return key->key_->SignSlowly(data); },
          base::WrapRefCounted(this),
          std::vector<uint8_t>(data.begin(), data.end())),
      std::move(callback));
}

std::vector<uint8_t> UnexportablePrivateKey::GetSubjectPublicKeyInfo() const {
  return key_->GetSubjectPublicKeyInfo();
}

crypto::sign::SignatureKind UnexportablePrivateKey::GetAlgorithm() const {
  return key_->Algorithm();
}

client_certificates_pb::PrivateKey UnexportablePrivateKey::ToProto() const {
  client_certificates_pb::PrivateKey private_key;
  private_key.set_source(ToProtoKeySource(source_));
  auto wrapped = key_->GetWrappedKey();
  private_key.set_wrapped_key(std::string(wrapped.begin(), wrapped.end()));
  return private_key;
}

base::DictValue UnexportablePrivateKey::ToDict() const {
  std::vector<uint8_t> wrapped = key_->GetWrappedKey();
  if (wrapped.empty()) {
    return base::DictValue();
  }

  return BuildSerializedPrivateKey(wrapped);
}

#if BUILDFLAG(IS_IOS)
SecKeyRef UnexportablePrivateKey::GetSecKeyRef() const {
  return key_->GetSecKeyRef();
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace client_certificates
