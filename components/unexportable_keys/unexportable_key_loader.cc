// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_loader.h"

#include <memory>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"

namespace unexportable_keys {

// static
std::unique_ptr<UnexportableKeyLoader>
UnexportableKeyLoader::CreateFromWrappedKey(
    UnexportableKeyService& unexportable_key_service,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority) {
  std::unique_ptr<UnexportableKeyLoader> loader =
      base::WrapUnique(new UnexportableKeyLoader());
  loader->LoadFromWrappedKey(unexportable_key_service, wrapped_key, priority);
  return loader;
}

// static
std::unique_ptr<UnexportableKeyLoader> UnexportableKeyLoader::CreateWithNewKey(
    UnexportableKeyService& unexportable_key_service,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority) {
  std::unique_ptr<UnexportableKeyLoader> loader =
      base::WrapUnique(new UnexportableKeyLoader());
  loader->GenerateNewKey(unexportable_key_service, acceptable_algorithms,
                         priority);
  return loader;
}

UnexportableKeyLoader::~UnexportableKeyLoader() = default;

void UnexportableKeyLoader::InvokeCallbackAfterKeyLoaded(
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  if (state_ == State::kReady) {
    // The key is ready, we can invoke the callback immediately.
    std::move(callback).Run(key_id_or_error_);
    return;
  }
  CHECK_EQ(state_, State::kLoading);
  on_load_callbacks_.push_back(std::move(callback));
}

ServiceErrorOr<UnexportableKeyId> UnexportableKeyLoader::GetKeyIdOrError() {
  return key_id_or_error_;
}

UnexportableKeyLoader::State UnexportableKeyLoader::GetStateForTesting() {
  return state_;
}

UnexportableKeyLoader::UnexportableKeyLoader() = default;

void UnexportableKeyLoader::LoadFromWrappedKey(
    UnexportableKeyService& unexportable_key_service,
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority) {
  CHECK_EQ(state_, State::kNotStarted);
  state_ = State::kLoading;
  unexportable_key_service.FromWrappedSigningKeySlowlyAsync(
      wrapped_key, priority,
      base::BindOnce(&UnexportableKeyLoader::OnKeyLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}
void UnexportableKeyLoader::GenerateNewKey(
    UnexportableKeyService& unexportable_key_service,
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority) {
  CHECK_EQ(state_, State::kNotStarted);
  state_ = State::kLoading;
  unexportable_key_service.GenerateSigningKeySlowlyAsync(
      acceptable_algorithms, priority,
      base::BindOnce(&UnexportableKeyLoader::OnKeyLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void UnexportableKeyLoader::OnKeyLoaded(
    ServiceErrorOr<UnexportableKeyId> key_id_or_error) {
  CHECK_EQ(state_, State::kLoading);
  state_ = State::kReady;
  key_id_or_error_ = key_id_or_error;

  std::vector<base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>>
      callbacks;
  callbacks.swap(on_load_callbacks_);
  // `this` may be destroyed after invoking a callback.
  for (auto& callback : callbacks) {
    std::move(callback).Run(key_id_or_error);
  }
}

}  // namespace unexportable_keys
