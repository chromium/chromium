// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_task_manager.h"

#include <memory>

#include "base/containers/span.h"
#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace unexportable_keys {

namespace {
scoped_refptr<RefCountedUnexportableSigningKey> MakeSigningKeyRefCounted(
    std::unique_ptr<crypto::UnexportableSigningKey> key) {
  if (!key) {
    return nullptr;
  }

  return base::MakeRefCounted<RefCountedUnexportableSigningKey>(std::move(key));
}
}  // namespace

UnexportableKeyTaskManager::UnexportableKeyTaskManager() = default;
UnexportableKeyTaskManager::~UnexportableKeyTaskManager() = default;

void UnexportableKeyTaskManager::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
        callback) {
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      crypto::GetUnexportableKeyProvider();

  if (!key_provider) {
    std::move(callback).Run(nullptr);
    return;
  }

  // TODO(b/263249728): run this on a background thread.
  std::move(callback).Run(MakeSigningKeyRefCounted(
      key_provider->GenerateSigningKeySlowly(acceptable_algorithms)));
}

void UnexportableKeyTaskManager::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
        callback) {
  std::unique_ptr<crypto::UnexportableKeyProvider> key_provider =
      crypto::GetUnexportableKeyProvider();

  if (!key_provider) {
    std::move(callback).Run(nullptr);
    return;
  }

  // TODO(b/263249728): run this on a background thread.
  std::move(callback).Run(MakeSigningKeyRefCounted(
      key_provider->FromWrappedSigningKeySlowly(wrapped_key)));
}

void UnexportableKeyTaskManager::SignSlowlyAsync(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback) {
  // TODO(b/263249728): run this on a background thread.
  // TODO(b/263249728): deduplicate tasks with the same parameters.
  // TODO(b/263249728): implement a cache of recent signings.
  if (!signing_key) {
    std::move(callback).Run(absl::nullopt);
    return;
  }

  std::move(callback).Run(signing_key->key().SignSlowly(data));
}

}  // namespace unexportable_keys
