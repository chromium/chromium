// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_

#include <cstdint>
#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/background_task_impl.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace crypto {
class UnexportableSigningKey;
class UnexportableKeyProvider;
}  // namespace crypto

namespace unexportable_keys {

class RefCountedUnexportableSigningKey;

// A `BackgroundTask` to generate a new `crypto::UnexportableSigningKey`.
class GenerateKeyTask : public internal::BackgroundTaskImpl<
                            std::unique_ptr<crypto::UnexportableSigningKey>> {
 public:
  GenerateKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      base::OnceCallback<void(GenerateKeyTask::ReturnType)> callback);
};

// A `BackgroundTask` to create a `crypto::UnexportableSigningKey` from a
// wrapped key.
class FromWrappedKeyTask
    : public internal::BackgroundTaskImpl<
          std::unique_ptr<crypto::UnexportableSigningKey>> {
 public:
  FromWrappedKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const uint8_t> wrapped_key,
      base::OnceCallback<void(FromWrappedKeyTask::ReturnType)> callback);
};

// A `BackgroundTask` to sign data with `crypto::UnexportableSigningKey`.
class SignTask : public internal::BackgroundTaskImpl<
                     absl::optional<std::vector<uint8_t>>> {
 public:
  SignTask(scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
           base::span<const uint8_t> data,
           base::OnceCallback<void(SignTask::ReturnType)> callback);
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
