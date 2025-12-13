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
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
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
      std::move(key), UnexportableKeyId());
}

ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
GetAllSigningKeysSlowly(crypto::UnexportableKeyProvider* key_provider,
                        void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::GetAllSigningKeysSlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);

  ASSIGN_OR_RETURN(
      std::vector<std::unique_ptr<crypto::UnexportableSigningKey>> keys,
      CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
          .GetAllSigningKeysSlowly(),
      [] { return ServiceError::kCryptoApiFailed; });

  return base::ToVector(keys, [](auto& key) {
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

ServiceErrorOr<void> DeleteSigningKeySlowly(
    crypto::UnexportableKeyProvider* key_provider,
    base::span<const uint8_t> wrapped_key,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::DeleteSigningKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  if (!CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
           .DeleteSigningKeySlowly(wrapped_key)) {
    return base::unexpected(ServiceError::kCryptoApiFailed);
  }

  return base::ok();
}

ServiceErrorOr<size_t> DeleteAllSigningKeysSlowly(
    crypto::UnexportableKeyProvider* key_provider,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::DeleteAllSigningKeysSlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));

  return base::OptionalToExpected(
      CHECK_DEREF(key_provider->AsStatefulUnexportableKeyProvider())
          .DeleteAllSigningKeysSlowly(),
      ServiceError::kCryptoApiFailed);
}

}  // namespace

GetAllKeysTask::GetAllKeysTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(GetAllKeysTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<GetAllKeysTask::ReturnType>(
          base::BindRepeating(&GetAllSigningKeysSlowly,
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
          base::BindRepeating(
              &GenerateSigningKeySlowly,
              base::Owned(std::move(key_provider)),
              std::vector<crypto::SignatureVerifier::SignatureAlgorithm>(
                  acceptable_algorithms.begin(),
                  acceptable_algorithms.end()),
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
          base::BindRepeating(
              &FromWrappedSigningKeySlowly,
              base::Owned(std::move(key_provider)),
              std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end()),
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
                              std::vector<uint8_t>(data.begin(), data.end()),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kSign,
          max_retries) {}

bool SignTask::ShouldRetryBasedOnResult(
    const ServiceErrorOr<std::vector<uint8_t>>& result) const {
  return !result.has_value();
}

DeleteKeyTask::DeleteKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    std::vector<uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(DeleteKeyTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<DeleteKeyTask::ReturnType>(
          base::BindRepeating(&DeleteSigningKeySlowly,
                              base::Owned(std::move(key_provider)),
                              std::move(wrapped_key),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kDeleteKey,
          /*max_retries=*/0) {}

DeleteAllKeysTask::DeleteAllKeysTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(DeleteAllKeysTask::ReturnType, size_t)> callback)
    : internal::BackgroundTaskImpl<DeleteAllKeysTask::ReturnType>(
          base::BindRepeating(&DeleteAllSigningKeysSlowly,
                              base::Owned(std::move(key_provider)),
                              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kDeleteAllKeys,
          /*max_retries=*/0) {}

}  // namespace unexportable_keys
