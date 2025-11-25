// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mojom/unexportable_key_service_proxied.h"

#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/notimplemented.h"
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace unexportable_keys {

UnexportableKeyServiceProxied::CachedKeyData::CachedKeyData() = default;

UnexportableKeyServiceProxied::CachedKeyData::CachedKeyData(
    const UnexportableKeyServiceProxied::CachedKeyData& other) = default;
UnexportableKeyServiceProxied::CachedKeyData&
UnexportableKeyServiceProxied::CachedKeyData::operator=(
    const UnexportableKeyServiceProxied::CachedKeyData& other) = default;
UnexportableKeyServiceProxied::CachedKeyData::CachedKeyData(
    UnexportableKeyServiceProxied::CachedKeyData&& other) noexcept = default;
UnexportableKeyServiceProxied::CachedKeyData&
UnexportableKeyServiceProxied::CachedKeyData::operator=(
    UnexportableKeyServiceProxied::CachedKeyData&& other) = default;

UnexportableKeyServiceProxied::CachedKeyData::~CachedKeyData() = default;

UnexportableKeyServiceProxied::UnexportableKeyServiceProxied(
    mojo::PendingRemote<mojom::UnexportableKeyService> pending_remote)
    : remote_(std::move(pending_remote)) {}

UnexportableKeyServiceProxied::~UnexportableKeyServiceProxied() = default;

void UnexportableKeyServiceProxied::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  remote_->GenerateSigningKey(
      base::ToVector(acceptable_algorithms), priority,
      // remote_ will not call any pending callbacks after it is destroyed.
      // Since we own remote_, it is guaranteed that this will be alive when a
      // callback is called.
      base::BindOnce(&UnexportableKeyServiceProxied::OnKeyGenerated,
                     base::Unretained(this), std::move(callback)));
}

void UnexportableKeyServiceProxied::OnKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewKeyDataPtr> result) {
  if (!result.has_value()) {
    std::move(original_callback).Run(base::unexpected(result.error()));
    return;
  }

  const mojom::NewKeyDataPtr& new_key_data = result.value();
  UnexportableKeyId key_id(new_key_data->key_id);

  CachedKeyData cached_data;
  cached_data.subject_public_key_info = new_key_data->subject_public_key_info;
  cached_data.wrapped_key = new_key_data->wrapped_key;
  cached_data.algorithm = new_key_data->algorithm;

  if (!key_cache_.try_emplace(key_id, std::move(cached_data)).second) {
    std::move(original_callback)
        .Run(base::unexpected(ServiceError::kKeyCollision));
    return;
  }

  std::move(original_callback).Run(key_id);
}

void UnexportableKeyServiceProxied::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  remote_->FromWrappedSigningKey(
      base::ToVector(wrapped_key), priority,
      // remote_ will not call any pending callbacks after it is destroyed.
      // Since we own remote_, it is guaranteed that this will be alive when a
      // callback is called.
      base::BindOnce(&UnexportableKeyServiceProxied::OnKeyLoaded,
                     base::Unretained(this), std::move(callback)));
}

void UnexportableKeyServiceProxied::OnKeyLoaded(
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewKeyDataPtr> result) {
  if (!result.has_value()) {
    std::move(original_callback).Run(base::unexpected(result.error()));
    return;
  }

  const mojom::NewKeyDataPtr& new_key_data = result.value();
  UnexportableKeyId key_id(new_key_data->key_id);

  key_cache_.lazy_emplace(key_id, [&](const auto& ctor) {
    CachedKeyData cached_data;
    cached_data.subject_public_key_info = new_key_data->subject_public_key_info;
    cached_data.wrapped_key = new_key_data->wrapped_key;
    cached_data.algorithm = new_key_data->algorithm;
    ctor(key_id, std::move(cached_data));
  });
  std::move(original_callback).Run(key_id);
}

void UnexportableKeyServiceProxied::CopyKeyFromOtherService(
    const UnexportableKeyService& other_service,
    UnexportableKeyId key_id_from_other_service,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  NOTIMPLEMENTED();
}

void UnexportableKeyServiceProxied::SignSlowlyAsync(
    const UnexportableKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  remote_->Sign(key_id, base::ToVector(data), priority, std::move(callback));
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceProxied::GetSubjectPublicKeyInfo(
    UnexportableKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.subject_public_key_info;
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceProxied::GetWrappedKey(UnexportableKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.wrapped_key;
}

ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
UnexportableKeyServiceProxied::GetAlgorithm(UnexportableKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.algorithm;
}

void UnexportableKeyServiceProxied::DeleteKeySlowlyAsync(
    UnexportableKeyId key_id,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<void>)> callback) {
  NOTIMPLEMENTED();
}

void UnexportableKeyServiceProxied::DeleteAllKeysSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  NOTIMPLEMENTED();
}

void UnexportableKeyServiceProxied::
    GetAllSigningKeysForGarbageCollectionSlowlyAsync(
        BackgroundTaskPriority priority,
        base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
            callback) {
  NOTIMPLEMENTED();
}

}  // namespace unexportable_keys
