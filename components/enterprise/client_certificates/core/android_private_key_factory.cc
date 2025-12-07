// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/android_private_key_factory.h"

#include <array>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/rand_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/android_private_key.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

// Function to generate a random identity for the Android KeyStore.
std::array<uint8_t, 32> GenerateRandomIdentity() {
  std::array<uint8_t, 32> identity;
  base::RandBytes(identity);
  return identity;
}

static constexpr std::array<device::PublicKeyCredentialParams::CredentialInfo,
                            1>
    kEs256Allowed = {device::PublicKeyCredentialParams::CredentialInfo{
        .algorithm = base::strict_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kEs256)}};

scoped_refptr<AndroidPrivateKey> CreateOrLoadKeyFromKeyStore(
    scoped_refptr<BrowserKeyStore> bk_key_store,
    std::vector<uint8_t> credential_id) {
  std::unique_ptr<BrowserKey> bk =
      bk_key_store->GetOrCreateBrowserKeyForCredentialId(
          credential_id, base::ToVector(kEs256Allowed));
  if (!bk) {
    return nullptr;
  }

  // Record the security level of the key.
  RecordClankKeySecurityLevel(bk->GetSecurityLevel());

  return base::MakeRefCounted<AndroidPrivateKey>(std::move(bk));
}

}  // namespace

// static
std::unique_ptr<AndroidPrivateKeyFactory>
AndroidPrivateKeyFactory::TryCreate() {
  return base::WrapUnique<AndroidPrivateKeyFactory>(
      new AndroidPrivateKeyFactory());
}

AndroidPrivateKeyFactory::AndroidPrivateKeyFactory() = default;
AndroidPrivateKeyFactory::~AndroidPrivateKeyFactory() = default;

void AndroidPrivateKeyFactory::CreatePrivateKey(PrivateKeyCallback callback) {
  auto identity = GenerateRandomIdentity();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          CreateOrLoadKeyFromKeyStore, CreateBrowserKeyStoreInstance(),
          std::vector<uint8_t>(std::begin(identity), std::end(identity))),
      std::move(callback));
}

void AndroidPrivateKeyFactory::LoadPrivateKey(
    const client_certificates_pb::PrivateKey& serialized_private_key,
    PrivateKeyCallback callback) {
  auto private_key_source = ToPrivateKeySource(serialized_private_key.source());
  CHECK(private_key_source.has_value());
  CHECK(private_key_source.value() == PrivateKeySource::kAndroidKey);

  const auto& wrapped_key_str = serialized_private_key.wrapped_key();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(
          CreateOrLoadKeyFromKeyStore, CreateBrowserKeyStoreInstance(),
          std::vector<uint8_t>(wrapped_key_str.begin(), wrapped_key_str.end())),
      std::move(callback));
}

void AndroidPrivateKeyFactory::LoadPrivateKeyFromDict(
    const base::Value::Dict& serialized_private_key,
    PrivateKeyCallback callback) {
  std::optional<int> source = serialized_private_key.FindInt(kKeySource);
  CHECK(ToPrivateKeySource(*source) == PrivateKeySource::kAndroidKey);

  auto* encoded_wrapped_private_key = serialized_private_key.FindString(kKey);
  std::string decoded_wrapped_private_key;
  if (encoded_wrapped_private_key->empty() ||
      !base::Base64Decode(*encoded_wrapped_private_key,
                          &decoded_wrapped_private_key)) {
    std::move(callback).Run(nullptr);
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(CreateOrLoadKeyFromKeyStore,
                     CreateBrowserKeyStoreInstance(),
                     std::vector<uint8_t>(decoded_wrapped_private_key.begin(),
                                          decoded_wrapped_private_key.end())),
      std::move(callback));
}

}  // namespace client_certificates
