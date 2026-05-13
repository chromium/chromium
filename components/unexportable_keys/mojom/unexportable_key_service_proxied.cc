// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/mojom/unexportable_key_service_proxied.h"

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/types/optional_util.h"
#include "base/unguessable_token.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace unexportable_keys {
namespace {

ServiceErrorOr<size_t> AdaptSizeType(ServiceErrorOr<uint64_t> result) {
  return result.transform(
      [](uint64_t r) { return base::checked_cast<size_t>(r); });
}
}  // namespace

UnexportableKeyServiceProxied::CachedKeyData::CachedKeyData() = default;

UnexportableKeyServiceProxied::CachedKeyData::CachedKeyData(
    const mojom::NewKeyMetadataPtr& metadata)
    : subject_public_key_info(metadata->subject_public_key_info),
      wrapped_key(metadata->wrapped_key),
      algorithm(metadata->algorithm),
      key_tag(base::OptionalToExpected(metadata->key_tag,
                                       ServiceError::kOperationNotSupported)),
      creation_time(
          base::OptionalToExpected(metadata->creation_time,
                                   ServiceError::kOperationNotSupported)) {}

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
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  remote_->GenerateSigningKey(
      base::ToVector(acceptable_algorithms), priority,
      // remote_ will not call any pending callbacks after it is destroyed.
      // Since we own remote_, it is guaranteed that this will be alive when a
      // callback is called.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UnexportableKeyServiceProxied::OnSigningKeyGenerated,
                         base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnSigningKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewSigningKeyDataPtr> result) {
  if (!result.has_value()) {
    std::move(original_callback).Run(base::unexpected(result.error()));
    return;
  }

  const mojom::NewSigningKeyDataPtr& new_key_data = result.value();
  UnexportableKeyId key_id(new_key_data->key_id);

  if (!key_cache_.try_emplace(key_id, new_key_data->metadata).second) {
    std::move(original_callback)
        .Run(base::unexpected(ServiceError::kKeyCollision));
    return;
  }

  std::move(original_callback).Run(UnexportableSigningKeyId(key_id));
}

void UnexportableKeyServiceProxied::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  remote_->FromWrappedSigningKey(
      base::ToVector(wrapped_key), priority,
      // remote_ will not call any pending callbacks after it is destroyed.
      // Since we own remote_, it is guaranteed that this will be alive when a
      // callback is called.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UnexportableKeyServiceProxied::OnSigningKeyLoaded,
                         base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnSigningKeyLoaded(
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewSigningKeyDataPtr> result) {
  if (!result.has_value()) {
    std::move(original_callback).Run(base::unexpected(result.error()));
    return;
  }

  const mojom::NewSigningKeyDataPtr& new_key_data = result.value();
  UnexportableKeyId key_id(new_key_data->key_id);

  key_cache_.try_emplace(key_id, new_key_data->metadata);
  std::move(original_callback).Run(UnexportableSigningKeyId(key_id));
}

void UnexportableKeyServiceProxied::GenerateAttestationKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // TODO(crbug.com/501306852): Implement this.
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}

void UnexportableKeyServiceProxied::FromWrappedAttestationKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // TODO(crbug.com/501306852): Implement this.
  std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
}

void UnexportableKeyServiceProxied::SignSlowlyAsync(
    UnexportableSigningKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  remote_->Sign(key_id, base::ToVector(data), priority,
                mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                    std::move(callback),
                    base::unexpected(ServiceError::kOperationCancelled)));
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

ServiceErrorOr<std::string> UnexportableKeyServiceProxied::GetKeyTag(
    UnexportableKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.key_tag;
}

ServiceErrorOr<base::Time> UnexportableKeyServiceProxied::GetCreationTime(
    UnexportableKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.creation_time;
}

void UnexportableKeyServiceProxied::DeleteKeysSlowlyAsync(
    base::span<const UnexportableKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  auto to_delete = base::ToVector(key_ids);
  std::erase_if(to_delete, [&](UnexportableKeyId key_id) {
    return key_cache_.erase(key_id) == 0;
  });

  if (to_delete.empty()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  remote_->DeleteKeys(
      to_delete, priority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&AdaptSizeType).Then(std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::DeleteAllKeysSlowlyAsync(
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  key_cache_.clear();

  remote_->DeleteAllKeys(mojo::WrapCallbackWithDefaultInvokeIfNotRun(
      base::BindOnce(&AdaptSizeType).Then(std::move(callback)),
      base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::GetAllKeysForGarbageCollectionSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
        callback) {
  // remote_ will not call any pending callbacks after it is destroyed.
  // Since we own remote_, it is guaranteed that this will be alive when a
  // callback is called.
  remote_->GetAllKeysForGarbageCollection(
      priority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &UnexportableKeyServiceProxied::OnGetAllKeysForGarbageCollection,
              base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnGetAllKeysForGarbageCollection(
    base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
        original_callback,
    ServiceErrorOr<std::vector<mojom::NewKeyDataPtr>> result) {
  ASSIGN_OR_RETURN(std::vector<mojom::NewKeyDataPtr> key_data,
                   std::move(result), [&](ServiceError error) {
                     std::move(original_callback).Run(base::unexpected(error));
                   });

  std::vector<UnexportableKeyId> key_ids;
  key_ids.reserve(key_data.size());
  for (mojom::NewKeyDataPtr& new_key_data : key_data) {
    UnexportableKeyId key_id = new_key_data->key_id;
    key_cache_.try_emplace(key_id, new_key_data->metadata);
    key_ids.push_back(key_id);
  }

  std::move(original_callback).Run(std::move(key_ids));
}

}  // namespace unexportable_keys
