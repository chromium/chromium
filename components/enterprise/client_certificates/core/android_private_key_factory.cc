// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/android_private_key_factory.h"

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/enterprise/client_certificates/core/android_private_key.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "net/ssl/ssl_private_key.h"

namespace client_certificates {

namespace {

static constexpr std::array<device::PublicKeyCredentialParams::CredentialInfo,
                            1>
    kEs256Allowed = {device::PublicKeyCredentialParams::CredentialInfo{
        .algorithm = base::strict_cast<int32_t>(
            device::CoseAlgorithmIdentifier::kEs256)}};

scoped_refptr<AndroidPrivateKey> CreateOrLoadKeyFromKeyStore(
    scoped_refptr<BrowserKeyStore> bk_key_store,
    std::vector<uint8_t> credential_id) {
  if (!bk_key_store->GetDeviceSupportsHardwareKeys()) {
    // StrongBox is required for this key.
    // TODO(crbug.com/432304139): Add support for software keys.
    return nullptr;
  }

  std::unique_ptr<BrowserKey> bk =
      bk_key_store->GetOrCreateBrowserKeyForCredentialId(
          credential_id, base::ToVector(kEs256Allowed));
  if (!bk) {
    return nullptr;
  }

  return base::MakeRefCounted<AndroidPrivateKey>(std::move(bk));
}

}  // namespace

// static
std::unique_ptr<AndroidPrivateKeyFactory> AndroidPrivateKeyFactory::TryCreate(
    scoped_refptr<BrowserKeyStore> bk_key_store) {
  if (!bk_key_store) {
    return nullptr;
  }

  return base::WrapUnique<AndroidPrivateKeyFactory>(
      new AndroidPrivateKeyFactory(bk_key_store));
}

// static
std::unique_ptr<AndroidPrivateKeyFactory>
AndroidPrivateKeyFactory::TryCreate() {
  return TryCreate(CreateBrowserKeyStoreInstance());
}

AndroidPrivateKeyFactory::AndroidPrivateKeyFactory(
    scoped_refptr<BrowserKeyStore> bk_key_store)
    : bk_key_store_(bk_key_store) {}

AndroidPrivateKeyFactory::~AndroidPrivateKeyFactory() = default;

void AndroidPrivateKeyFactory::CreatePrivateKey(PrivateKeyCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(CreateOrLoadKeyFromKeyStore,
                     CreateBrowserKeyStoreInstance(),
                     kManagedProfileAndroidKeyStoreIdentity),
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
