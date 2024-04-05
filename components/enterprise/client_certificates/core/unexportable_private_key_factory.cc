// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

#include <array>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key.h"
#include "crypto/unexportable_key.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

scoped_refptr<UnexportablePrivateKey> CreateKey(
    crypto::UnexportableKeyProvider::Config config) {
  auto provider = crypto::GetUnexportableKeyProvider(std::move(config));
  if (!provider) {
    return nullptr;
  }

  static constexpr std::array<crypto::SignatureVerifier::SignatureAlgorithm, 2>
      kAcceptableAlgorithms = {crypto::SignatureVerifier::ECDSA_SHA256,
                               crypto::SignatureVerifier::RSA_PKCS1_SHA256};
  auto key = provider->GenerateSigningKeySlowly(kAcceptableAlgorithms);

  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<UnexportablePrivateKey>(std::move(key));
}

scoped_refptr<UnexportablePrivateKey> LoadKeyFromWrapped(
    const std::vector<uint8_t>& wrapped_key,
    crypto::UnexportableKeyProvider::Config config) {
  auto provider = crypto::GetUnexportableKeyProvider(std::move(config));
  if (!provider) {
    return nullptr;
  }

  auto key = provider->FromWrappedSigningKeySlowly(wrapped_key);
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<UnexportablePrivateKey>(std::move(key));
}

}  // namespace

// static
std::unique_ptr<UnexportablePrivateKeyFactory>
UnexportablePrivateKeyFactory::TryCreate(
    crypto::UnexportableKeyProvider::Config config) {
  auto provider = crypto::GetUnexportableKeyProvider(config);

  if (!provider) {
    // Unexportable keys are not supported.
    return nullptr;
  }

  return base::WrapUnique<UnexportablePrivateKeyFactory>(
      new UnexportablePrivateKeyFactory(std::move(config)));
}

UnexportablePrivateKeyFactory::UnexportablePrivateKeyFactory(
    crypto::UnexportableKeyProvider::Config config)
    : config_(std::move(config)) {}

UnexportablePrivateKeyFactory::~UnexportablePrivateKeyFactory() = default;

void UnexportablePrivateKeyFactory::CreatePrivateKey(
    PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()}, base::BindOnce(CreateKey, config_),
      std::move(callback));
}

void UnexportablePrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  auto private_key_source = ToPrivateKeySource(serialized_private_key.source());
  CHECK(private_key_source.has_value());
  CHECK(private_key_source.value() == PrivateKeySource::kUnexportableKey);

  const auto& wrapped_key_str = serialized_private_key.wrapped_key();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          LoadKeyFromWrapped,
          std::vector<uint8_t>(wrapped_key_str.begin(), wrapped_key_str.end()),
          config_),
      std::move(callback));
}

}  // namespace client_certificates
