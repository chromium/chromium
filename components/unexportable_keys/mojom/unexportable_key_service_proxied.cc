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
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
#include "mojo/public/cpp/bindings/enum_traits.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace unexportable_keys {
namespace {

ServiceErrorOr<size_t> AdaptSizeType(ServiceErrorOr<uint64_t> result) {
  return result.transform(
      [](uint64_t r) { return base::checked_cast<size_t>(r); });
}

CachedKeyData ToCachedKeyData(mojom::NewKeyMetadataPtr metadata) {
  return CachedKeyData{
      .subject_public_key_info = std::move(metadata->subject_public_key_info),
      .wrapped_key = std::move(metadata->wrapped_key),
      .algorithm = metadata->algorithm,
      .key_tag = base::OptionalToExpected(std::move(metadata->key_tag),
                                          ServiceError::kOperationNotSupported),
      .creation_time = base::OptionalToExpected(
          metadata->creation_time, ServiceError::kOperationNotSupported),
  };
}

template <
    typename NewKeyDataPtrType,
    typename KeyIdType = decltype(std::declval<NewKeyDataPtrType>()->key_id)>
ServiceErrorOr<KeyIdType> OnKeyGeneratedImpl(
    absl::flat_hash_map<UnexportableSigningKeyId, CachedKeyData>& key_cache,
    ServiceErrorOr<NewKeyDataPtrType> result) {
  ASSIGN_OR_RETURN(NewKeyDataPtrType new_key_data, std::move(result));
  KeyIdType key_id = new_key_data->key_id;
  if (!key_cache
           .try_emplace(key_id,
                        ToCachedKeyData(std::move(new_key_data->metadata)))
           .second) {
    return base::unexpected(ServiceError::kKeyCollision);
  }

  return key_id;
}

template <
    typename NewKeyDataPtrType,
    typename KeyIdType = decltype(std::declval<NewKeyDataPtrType>()->key_id)>
ServiceErrorOr<KeyIdType> OnKeyLoadedImpl(
    absl::flat_hash_map<UnexportableSigningKeyId, CachedKeyData>& key_cache,
    ServiceErrorOr<NewKeyDataPtrType> result) {
  ASSIGN_OR_RETURN(NewKeyDataPtrType new_key_data, std::move(result));
  KeyIdType key_id = new_key_data->key_id;
  key_cache.try_emplace(key_id,
                        ToCachedKeyData(std::move(new_key_data->metadata)));
  return key_id;
}

}  // namespace


UnexportableKeyServiceProxied::UnexportableKeyServiceProxied(
    mojo::PendingRemote<mojom::UnexportableKeyService> pending_remote)
    : remote_(std::move(pending_remote)) {}

UnexportableKeyServiceProxied::~UnexportableKeyServiceProxied() = default;

void UnexportableKeyServiceProxied::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  remote_->GenerateSigningKey(
      base::ToVector(acceptable_algorithms), priority,
      // SAFETY: remote_ will not call any pending callbacks after it is
      // destroyed. Since we own remote_, it is guaranteed that this will be
      // alive when a callback is called.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UnexportableKeyServiceProxied::OnSigningKeyGenerated,
                         base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnSigningKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewSigningKeyDataPtr> result) {
  std::move(original_callback)
      .Run(OnKeyGeneratedImpl(key_cache_, std::move(result)));
}

void UnexportableKeyServiceProxied::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  remote_->FromWrappedSigningKey(
      base::ToVector(wrapped_key), priority,
      // SAFETY: remote_ will not call any pending callbacks after it is
      // destroyed. Since we own remote_, it is guaranteed that this will be
      // alive when a callback is called.
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UnexportableKeyServiceProxied::OnSigningKeyLoaded,
                         base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnSigningKeyLoaded(
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewSigningKeyDataPtr> result) {
  std::move(original_callback)
      .Run(OnKeyLoadedImpl(key_cache_, std::move(result)));
}

void UnexportableKeyServiceProxied::GenerateAttestationKeySlowlyAsync(
    base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // SAFETY: remote_ will not call any pending callbacks after it is destroyed.
  // Since we own remote_, it is guaranteed that this will be alive when a
  // callback is called.
  remote_->GenerateAttestationKey(
      base::ToVector(acceptable_algorithms), priority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(
              &UnexportableKeyServiceProxied::OnAttestationKeyGenerated,
              base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnAttestationKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewAttestationKeyDataPtr> result) {
  std::move(original_callback)
      .Run(OnKeyGeneratedImpl(key_cache_, std::move(result)));
}

void UnexportableKeyServiceProxied::FromWrappedAttestationKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // SAFETY: remote_ will not call any pending callbacks after it is destroyed.
  // Since we own remote_, it is guaranteed that this will be alive when a
  // callback is called.
  remote_->FromWrappedAttestationKey(
      base::ToVector(wrapped_key), priority,
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&UnexportableKeyServiceProxied::OnAttestationKeyLoaded,
                         base::Unretained(this), std::move(callback)),
          base::unexpected(ServiceError::kOperationCancelled)));
}

void UnexportableKeyServiceProxied::OnAttestationKeyLoaded(
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        original_callback,
    ServiceErrorOr<mojom::NewAttestationKeyDataPtr> result) {
  std::move(original_callback)
      .Run(OnKeyLoadedImpl(key_cache_, std::move(result)));
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

void UnexportableKeyServiceProxied::CertifySlowlyAsync(
    UnexportableAttestationKeyId attestation_key_id,
    UnexportableSigningKeyId signing_key_id,
    base::span<const uint8_t> challenge,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
        callback) {
  remote_->Certify(attestation_key_id, signing_key_id,
                   base::ToVector(challenge), priority,
                   mojo::WrapCallbackWithDefaultInvokeIfNotRun(
                       std::move(callback),
                       base::unexpected(ServiceError::kOperationCancelled)));
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceProxied::GetSubjectPublicKeyInfo(
    UnexportableSigningKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.subject_public_key_info;
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceProxied::GetWrappedKey(
    UnexportableSigningKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.wrapped_key;
}

ServiceErrorOr<crypto::sign::SignatureKind>
UnexportableKeyServiceProxied::GetAlgorithm(
    UnexportableSigningKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.algorithm;
}

ServiceErrorOr<std::string> UnexportableKeyServiceProxied::GetKeyTag(
    UnexportableSigningKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.key_tag;
}

ServiceErrorOr<base::Time> UnexportableKeyServiceProxied::GetCreationTime(
    UnexportableSigningKeyId key_id) const {
  auto it = key_cache_.find(key_id);
  if (it == key_cache_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second.creation_time;
}

void UnexportableKeyServiceProxied::DeleteKeysSlowlyAsync(
    base::span<const UnexportableSigningKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  auto to_delete = base::ToVector(key_ids);
  std::erase_if(to_delete, [&](UnexportableSigningKeyId key_id) {
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
    base::OnceCallback<
        void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback) {
  // SAFETY: remote_ will not call any pending callbacks after it is destroyed.
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
    base::OnceCallback<
        void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)>
        original_callback,
    ServiceErrorOr<std::vector<mojom::NewSigningKeyDataPtr>> result) {
  ASSIGN_OR_RETURN(std::vector<mojom::NewSigningKeyDataPtr> key_data,
                   std::move(result), [&](ServiceError error) {
                     std::move(original_callback).Run(base::unexpected(error));
                   });

  std::vector<UnexportableSigningKeyId> key_ids;
  key_ids.reserve(key_data.size());
  for (mojom::NewSigningKeyDataPtr& new_key_data : key_data) {
    UnexportableSigningKeyId key_id = new_key_data->key_id;
    key_cache_.try_emplace(key_id,
                           ToCachedKeyData(std::move(new_key_data->metadata)));
    key_ids.push_back(key_id);
  }

  std::move(original_callback).Run(std::move(key_ids));
}

}  // namespace unexportable_keys
