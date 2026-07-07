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
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

void FakeUnexportableKeyService::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::GenerateAttestationKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::FromWrappedAttestationKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::GetAllKeysForGarbageCollectionSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<
        void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::SignSlowlyAsync(
    UnexportableSigningKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::CertifySlowlyAsync(
    UnexportableAttestationKeyId attestation_key_id,
    UnexportableSigningKeyId signing_key_id,
    base::span<const uint8_t> challenge,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
        callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::DeleteKeysSlowlyAsync(
    base::span<const UnexportableSigningKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
void FakeUnexportableKeyService::DeleteAllKeysSlowlyAsync(
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}
ServiceErrorOr<std::vector<uint8_t>>
FakeUnexportableKeyService::GetSubjectPublicKeyInfo(
    UnexportableSigningKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}
ServiceErrorOr<std::vector<uint8_t>> FakeUnexportableKeyService::GetWrappedKey(
    UnexportableSigningKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}
ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
FakeUnexportableKeyService::GetAlgorithm(
    UnexportableSigningKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}

ServiceErrorOr<std::string> FakeUnexportableKeyService::GetKeyTag(
    UnexportableSigningKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}

ServiceErrorOr<base::Time> FakeUnexportableKeyService::GetCreationTime(
    UnexportableSigningKeyId key_id) const {
  return base::unexpected(ServiceError::kKeyNotFound);
}

}  // namespace unexportable_keys
