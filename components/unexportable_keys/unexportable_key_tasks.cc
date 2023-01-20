// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_tasks.h"

#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace unexportable_keys {

namespace {

absl::optional<std::vector<uint8_t>> SignSlowlyWithRefCountedKey(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data) {
  if (!signing_key) {
    return absl::nullopt;
  }

  return signing_key->key().SignSlowly(data);
}

}  // namespace

GenerateKeyTask::GenerateKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    base::OnceCallback<void(GenerateKeyTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<GenerateKeyTask::ReturnType>(
          base::BindOnce(
              &crypto::UnexportableKeyProvider::GenerateSigningKeySlowly,
              std::move(key_provider),
              std::vector<const crypto::SignatureVerifier::SignatureAlgorithm>(
                  acceptable_algorithms.begin(),
                  acceptable_algorithms.end())),
          std::move(callback)) {}

FromWrappedKeyTask::FromWrappedKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const uint8_t> wrapped_key,
    base::OnceCallback<void(FromWrappedKeyTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<FromWrappedKeyTask::ReturnType>(
          base::BindOnce(
              &crypto::UnexportableKeyProvider::FromWrappedSigningKeySlowly,
              std::move(key_provider),
              std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end())),
          std::move(callback)) {}

SignTask::SignTask(scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
                   base::span<const uint8_t> data,
                   base::OnceCallback<void(SignTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<SignTask::ReturnType>(
          base::BindOnce(&SignSlowlyWithRefCountedKey,
                         std::move(signing_key),
                         std::vector<uint8_t>(data.begin(), data.end())),
          std::move(callback)) {}

}  // namespace unexportable_keys
