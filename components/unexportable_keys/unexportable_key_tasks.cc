// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_tasks.h"

#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/trace_event/typed_macros.h"
#include "components/unexportable_keys/background_task_type.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

std::unique_ptr<crypto::UnexportableSigningKey> GenerateSigningKeySlowly(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::GenerateSigningKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return key_provider->GenerateSigningKeySlowly(acceptable_algorithms);
}

std::unique_ptr<crypto::UnexportableSigningKey> FromWrappedSigningKeySlowly(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const uint8_t> wrapped_key,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::FromWrappedSigningKeySlowly",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(key_provider);
  return key_provider->FromWrappedSigningKeySlowly(wrapped_key);
}

std::optional<std::vector<uint8_t>> SignSlowlyWithRefCountedKey(
    scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
    base::span<const uint8_t> data,
    void* task_ptr_for_tracing) {
  TRACE_EVENT("browser", "unexportable_keys::SignSlowlyWithRefCountedKey",
              perfetto::Flow::FromPointer(task_ptr_for_tracing));
  CHECK(signing_key);
  return signing_key->key().SignSlowly(data);
}

}  // namespace

GenerateKeyTask::GenerateKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(GenerateKeyTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<GenerateKeyTask::ReturnType>(
          base::BindOnce(
              &GenerateSigningKeySlowly,
              std::move(key_provider),
              std::vector<crypto::SignatureVerifier::SignatureAlgorithm>(
                  acceptable_algorithms.begin(),
                  acceptable_algorithms.end()),
              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kGenerateKey) {}

FromWrappedKeyTask::FromWrappedKeyTask(
    std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(FromWrappedKeyTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<FromWrappedKeyTask::ReturnType>(
          base::BindOnce(
              &FromWrappedSigningKeySlowly,
              std::move(key_provider),
              std::vector<uint8_t>(wrapped_key.begin(), wrapped_key.end()),
              this),
          std::move(callback),
          priority,
          BackgroundTaskType::kFromWrappedKey) {}

SignTask::SignTask(scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
                   base::span<const uint8_t> data,
                   BackgroundTaskPriority priority,
                   base::OnceCallback<void(SignTask::ReturnType)> callback)
    : internal::BackgroundTaskImpl<SignTask::ReturnType>(
          base::BindOnce(&SignSlowlyWithRefCountedKey,
                         std::move(signing_key),
                         std::vector<uint8_t>(data.begin(), data.end()),
                         this),
          std::move(callback),
          priority,
          BackgroundTaskType::kSign) {}

}  // namespace unexportable_keys
