// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/fake_unexportable_key_service.h"

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"

namespace unexportable_keys {

void FakeUnexportableKeyService::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::SignSlowlyAsync(
    const UnexportableKeyId& key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
ServiceErrorOr<std::vector<uint8_t>>
FakeUnexportableKeyService::GetSubjectPublicKeyInfo(
    UnexportableKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}
ServiceErrorOr<std::vector<uint8_t>> FakeUnexportableKeyService::GetWrappedKey(
    UnexportableKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}
ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
FakeUnexportableKeyService::GetAlgorithm(UnexportableKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}

}  // namespace unexportable_keys
