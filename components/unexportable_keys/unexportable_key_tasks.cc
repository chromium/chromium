// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_tasks.h"

#include <memory>
#include <optional>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/memory/scoped_refptr.h"
#include "base/trace_event/typed_macros.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "components/unexportable_keys/background_task_type.h"
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
MakeSigningKeyRefCounted(std::unique_ptr<crypto::UnexportableSigningKey> key) {
  if (!key) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }

  return base::MakeRefCounted<RefCountedUnexportableSigningKey>(
      std::move(key), UnexportableSigningKeyId());
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>
MakeAttestationKeyRefCounted(
    std::unique_ptr<crypto::UnexportableAttestationKey> key) {
  if (!key) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }

  return base::MakeRefCounted<RefCountedUnexportableAttestationKey>(
      std::move(key), UnexportableAttestationKeyId());
}

ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableKey>>>
GetAllKeysSlowly(crypto::UnexportableKeyProvider* key_provider,
                 void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::GetAllKeysSlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);

  ASSIGN_OR_RETURN(
      std::vector<std::unique_ptr<crypto::UnexportableSigningKey>> keys,
      CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
          .GetAllKeysSlowly(),
      [] { return ServiceError::kCryptoApiFailed; });

  return base::ToVector<scoped_refptr<RefCountedUnexportableKey>>(
      std::move(keys),
      [](std::unique_ptr<crypto::UnexportableSigningKey>& key) {
        return MakeSigningKeyRefCounted(std::move(key)).value();
      });
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
GenerateSigningKeySlowly(
    crypto::UnexportableKeyProvider* key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::GenerateSigningKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return MakeSigningKeyRefCounted(
      key_provider->GenerateSigningKeySlowly(acceptable_algorithms));
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
FromWrappedSigningKeySlowly(crypto::UnexportableKeyProvider* key_provider,
                            base::span<const uint8_t> wrapped_key,
                            void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::FromWrappedSigningKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return MakeSigningKeyRefCounted(
      key_provider->FromWrappedSigningKeySlowly(wrapped_key));
}

ServiceErrorOr<std::vector<uint8_t>> SignSlowlyWithRefCountedKey(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::SignSlowlyWithRefCountedKey",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(signing_key);
  std::optional<std::vector<uint8_t>> signature =
      signing_key->key().SignSlowly(data);
  if (!signature.has_value()) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }
  // The analysis has proven that the returned signature does not always verify
  // correctly. This is a very rare occurrence. Return an error if it does
  // happen to force the retry mechanism to kick in as it is very likely to
  // succeed on the next attempt.
  crypto::SignatureVerifier verifier;
  if (!verifier.VerifyInit(signing_key->key().Algorithm(), *signature,
                           signing_key->key().GetSubjectPublicKeyInfo())) {
    return base::unexpected(ServiceError::kVerifySignatureFailed);
  }
  verifier.VerifyUpdate(data);
  if (!verifier.VerifyFinal()) {
    return base::unexpected(ServiceError::kVerifySignatureFailed);
  }
  return *std::move(signature);
}

ServiceErrorOr<size_t> DeleteKeysSlowly(
    crypto::UnexportableKeyProvider* key_provider,
    base::span<const scoped_refptr<RefCountedUnexportableKey>> keys,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::DeleteKeysSlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  return base::OptionalToExpected(
      CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
          .DeleteKeysSlowly(
              base::ToVector(keys, [](auto& key) { return &key->key(); })),
      ServiceError::kCryptoApiFailed);
}

ServiceErrorOr<size_t> DeleteAllKeysSlowly(
    crypto::UnexportableKeyProvider* key_provider,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::DeleteAllKeysSlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));

  return base::OptionalToExpected(
      CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
          .DeleteAllKeysSlowly(),
      ServiceError::kCryptoApiFailed);
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>
GenerateAttestationKeySlowly(
    crypto::UnexportableKeyProvider* key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::GenerateAttestationKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return MakeAttestationKeyRefCounted(
      key_provider->GenerateAttestationKeySlowly(acceptable_algorithms));
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>
FromWrappedAttestationKeySlowly(crypto::UnexportableKeyProvider* key_provider,
                                base::span<const uint8_t> wrapped_key,
                                void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::FromWrappedAttestationKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return MakeAttestationKeyRefCounted(
      key_provider->FromWrappedAttestationKeySlowly(wrapped_key));
}

ServiceErrorOr<crypto::AttestationStatement> CertifySlowly(
    scoped_refptr<RefCountedUnexportableAttestationKey> attestation_key,
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> challenge,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::CertifySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(attestation_key);
  CHECK(signing_key);
  return base::OptionalToExpected(
      attestation_key->key().CertifySlowly(signing_key->key(), challenge),
      ServiceError::kCryptoApiFailed);
}

}  // namespace

GetAllKeysTask::GetAllKeysTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(GetAllKeysTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<GetAllKeysTask::ReturnType>(
          base::BindRepeating(&GetAllKeysSlowly,
                              base::Owned(std::move(key_provider)),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kGetAllKeys,
          /*max_retries=*/0) {}

GenerateKeyTask::GenerateKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(GenerateKeyTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<GenerateKeyTask::ReturnType>(
          base::BindRepeating(&GenerateSigningKeySlowly,
                              base::Owned(std::move(key_provider)),
                              base::ToVector(acceptable_algorithms),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kGenerateKey,
          /*max_retries=*/0) {}

FromWrappedKeyTask::FromWrappedKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(FromWrappedKeyTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<FromWrappedKeyTask::ReturnType>(
          base::BindRepeating(&FromWrappedSigningKeySlowly,
                              base::Owned(std::move(key_provider)),
                              base::ToVector(wrapped_key),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kFromWrappedKey,
          /*max_retries=*/0) {}

SignTask::SignTask(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    size_t max_retries,
    base::OnceCallback<void(SignTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<SignTask::ReturnType>(
          base::BindRepeating(&SignSlowlyWithRefCountedKey,
                              std::move(signing_key),
                              base::ToVector(data),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kSign,
          max_retries) {}

bool SignTask::ShouldRetryBasedOnResult(
    const ServiceErrorOr<std::vector<uint8_t>>& result) const {
  return !result.has_value();
}

DeleteKeysTask::DeleteKeysTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    std::vector<scoped_refptr<RefCountedUnexportableKey>> keys,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(DeleteKeysTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<DeleteKeysTask::ReturnType>(
          base::BindRepeating(&DeleteKeysSlowly,
                              base::Owned(std::move(key_provider)),
                              std::move(keys),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kDeleteKeys,
          /*max_retries=*/0) {}

DeleteAllKeysTask::DeleteAllKeysTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(DeleteAllKeysTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<DeleteAllKeysTask::ReturnType>(
          base::BindRepeating(&DeleteAllKeysSlowly,
                              base::Owned(std::move(key_provider)),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kDeleteAllKeys,
          /*max_retries=*/0) {}

GenerateAttestationKeyTask::GenerateAttestationKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(GenerateAttestationKeyTask::ReturnType, size_t)>
        callback)
    : internal::BackgroundTaskImpl<GenerateAttestationKeyTask::ReturnType>(
          base::BindRepeating(&GenerateAttestationKeySlowly,
                              base::Owned(std::move(key_provider)),
                              base::ToVector(acceptable_algorithms),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kGenerateAttestationKey,
          /*max_retries=*/0) {}

FromWrappedAttestationKeyTask::FromWrappedAttestationKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(FromWrappedAttestationKeyTask::ReturnType, size_t)>
        callback)
    : internal::BackgroundTaskImpl<FromWrappedAttestationKeyTask::ReturnType>(
          base::BindRepeating(&FromWrappedAttestationKeySlowly,
                              base::Owned(std::move(key_provider)),
                              base::ToVector(wrapped_key),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kFromWrappedAttestationKey,
          /*max_retries=*/0) {}

CertifyTask::CertifyTask(
    scoped_refptr<RefCountedUnexportableAttestationKey> attestation_key,
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> challenge,
    BackgroundTaskPriority priority,
    size_t max_retries,
    base::OnceCallback<void(CertifyTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<CertifyTask::ReturnType>(
          base::BindRepeating(&CertifySlowly,
                              std::move(attestation_key),
                              std::move(signing_key),
                              base::ToVector(challenge),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kCertify,
          max_retries) {}

bool CertifyTask::ShouldRetryBasedOnResult(
    const ServiceErrorOr<crypto::AttestationStatement>& result) const {
  return !result.has_value();
}

}  // namespace unexportable_keys
