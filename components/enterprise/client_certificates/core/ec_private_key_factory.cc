// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/ec_private_key_factory.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/ec_private_key.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "crypto/ec_private_key.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

scoped_refptr<ECPrivateKey> CreateKey() {
  auto key = crypto::ECPrivateKey::Create();
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<ECPrivateKey>(std::move(key));
}

scoped_refptr<ECPrivateKey> LoadKeyFromWrapped(
    const std::vector<uint8_t>& wrapped_key) {
  auto key = crypto::ECPrivateKey::CreateFromPrivateKeyInfo(wrapped_key);
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<ECPrivateKey>(std::move(key));
}

}  // namespace

ECPrivateKeyFactory::ECPrivateKeyFactory() = default;

ECPrivateKeyFactory::~ECPrivateKeyFactory() = default;

void ECPrivateKeyFactory::CreatePrivateKey(PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(FROM_HERE, {base::MayBlock()},
                                               base::BindOnce(CreateKey),
                                               std::move(callback));
}

void ECPrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  auto private_key_source = ToPrivateKeySource(serialized_private_key.source());
  CHECK(private_key_source.has_value());
  CHECK(private_key_source.value() == PrivateKeySource::kSoftwareKey);

  const auto& wrapped_key_str = serialized_private_key.wrapped_key();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          LoadKeyFromWrapped,
          std::vector<uint8_t>(wrapped_key_str.begin(), wrapped_key_str.end())),
      std::move(callback));
}

void ECPrivateKeyFactory::LoadPrivateKeyFromDict(
    const base::Value::Dict& serialized_private_key,
    PrivateKeyCallback callback) {
  std::optional<int> source = serialized_private_key.FindInt(kKeySource);
  CHECK(ToPrivateKeySource(*source) == PrivateKeySource::kSoftwareKey);

  auto* encoded_wrapped_private_key = serialized_private_key.FindString(kKey);
  std::string decoded_wrapped_private_key;
  if (!base::Base64Decode(*encoded_wrapped_private_key,
                          &decoded_wrapped_private_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(LoadKeyFromWrapped,
                     std::vector<uint8_t>(decoded_wrapped_private_key.begin(),
                                          decoded_wrapped_private_key.end())),
      std::move(callback));
}

}  // namespace client_certificates
