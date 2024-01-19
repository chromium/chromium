// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/unexportable_private_key_factory.h"

#include <array>
#include <utility>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/unexportable_private_key.h"
#include "crypto/unexportable_key.h"

namespace client_certificates {

namespace {

scoped_refptr<UnexportablePrivateKey> CreateKey() {
  auto provider = crypto::GetUnexportableKeyProvider();
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

}  // namespace

// static
std::unique_ptr<UnexportablePrivateKeyFactory>
UnexportablePrivateKeyFactory::TryCreate() {
  auto provider = crypto::GetUnexportableKeyProvider();

  if (!provider) {
    // Unexportable keys are not supported.
    return nullptr;
  }

  return base::WrapUnique<UnexportablePrivateKeyFactory>(
      new UnexportablePrivateKeyFactory());
}

UnexportablePrivateKeyFactory::UnexportablePrivateKeyFactory() = default;

UnexportablePrivateKeyFactory::~UnexportablePrivateKeyFactory() = default;

void UnexportablePrivateKeyFactory::CreatePrivateKey(
    PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(CreateKey),
                                               std::move(callback));
}

}  // namespace client_certificates
