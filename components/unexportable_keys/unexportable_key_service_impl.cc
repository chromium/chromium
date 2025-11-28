// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include <algorithm>
#include <variant>

#include "base/containers/to_vector.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

// Class holding either an `UnexportableKeyId` or a list of callbacks waiting
// for the key creation.
class MaybePendingUnexportableKeyId {
 public:
  using CallbackType =
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>;
  using PendingCallbacks = std::vector<CallbackType>;
  using PendingCallbacksOrKeyId =
      std::variant<PendingCallbacks, UnexportableKeyId>;

  // Constructs an instance holding a list of callbacks.
  MaybePendingUnexportableKeyId() = default;

  // Constructs an instance holding `key_id`.
  explicit MaybePendingUnexportableKeyId(UnexportableKeyId key_id)
      : pending_callbacks_or_key_id_(key_id) {}

  // Returns true if a key has been assigned to this instance. Otherwise,
  // returns false which means that this instance holds a list of callbacks.
  bool HasKeyId() const {
    return std::holds_alternative<UnexportableKeyId>(
        pending_callbacks_or_key_id_);
  }

  // This method should be called only if `HasKeyId()` is true.
  UnexportableKeyId GetKeyId() const {
    CHECK(HasKeyId());
    return std::get<UnexportableKeyId>(pending_callbacks_or_key_id_);
  }

  // These methods should be called only if `HasKeyId()` is false.

  // Adds `callback` to the list of callbacks and returns size of the list.
  size_t AddCallback(CallbackType callback) {
    CHECK(!HasKeyId());
    GetCallbacks().push_back(std::move(callback));
    return GetCallbacks().size();
  }

  void SetKeyIdAndRunCallbacks(UnexportableKeyId key_id) {
    CHECK(!HasKeyId());
    PendingCallbacksOrKeyId pending_callbacks =
        std::exchange(pending_callbacks_or_key_id_, key_id);
    for (auto& callback : std::get<PendingCallbacks>(pending_callbacks)) {
      std::move(callback).Run(key_id);
    }
  }

  void RunCallbacksWithFailure(ServiceError error) {
    CHECK(!HasKeyId());
    for (auto& callback : std::exchange(GetCallbacks(), PendingCallbacks())) {
      std::move(callback).Run(base::unexpected(error));
    }
  }

 private:
  PendingCallbacks& GetCallbacks() {
    CHECK(!HasKeyId());
    return std::get<PendingCallbacks>(pending_callbacks_or_key_id_);
  }

  // Holds the value of its first alternative type by default.
  PendingCallbacksOrKeyId pending_callbacks_or_key_id_;
};

UnexportableKeyServiceImpl::UnexportableKeyServiceImpl(
    UnexportableKeyTaskManager& task_manager,
    crypto::UnexportableKeyProvider::Config config)
    : task_manager_(task_manager), config_(config) {}

UnexportableKeyServiceImpl::~UnexportableKeyServiceImpl() = default;

// static
bool UnexportableKeyServiceImpl::IsUnexportableKeyProviderSupported(
    crypto::UnexportableKeyProvider::Config config) {
  return UnexportableKeyTaskManager::GetUnexportableKeyProvider(
             std::move(config)) != nullptr;
}

void UnexportableKeyServiceImpl::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  task_manager_->GenerateSigningKeySlowlyAsync(
      config_, acceptable_algorithms, priority,
      base::BindOnce(&UnexportableKeyServiceImpl::OnKeyGenerated,
                     generate_key_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void UnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  auto& [wrapped_key_vec, maybe_pending_key_id] =
      *key_id_by_wrapped_key_.lazy_emplace(wrapped_key, [&](const auto& ctor) {
        ctor(base::ToVector(wrapped_key), MaybePendingUnexportableKeyId());
      });

  if (maybe_pending_key_id.HasKeyId()) {
    std::move(callback).Run(maybe_pending_key_id.GetKeyId());
    return;
  }

  size_t n_callbacks = maybe_pending_key_id.AddCallback(std::move(callback));
  if (n_callbacks == 1) {
    // `callback` is the first one waiting for the wrapped key. Schedule the
    // task to create a key from the wrapped key.
    task_manager_->FromWrappedSigningKeySlowlyAsync(
        config_, wrapped_key, priority,
        base::BindOnce(&UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKey,
                       from_wrapped_key_weak_ptr_factory_.GetWeakPtr(),
                       wrapped_key_vec));
  }
}

void UnexportableKeyServiceImpl::
    GetAllSigningKeysForGarbageCollectionSlowlyAsync(
        BackgroundTaskPriority priority,
        base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
            callback) {
  task_manager_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      config_, priority,
      base::BindOnce(&UnexportableKeyServiceImpl::
                         OnGetAllSigningKeysForGarbageCollectionSlowly,
                     get_all_keys_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void UnexportableKeyServiceImpl::SignSlowlyAsync(
    UnexportableKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  // The type expected by the callback
  using ArgType = ServiceErrorOr<std::vector<uint8_t>>;
  task_manager_->SignSlowlyAsync(
      it->second, data, priority,
      base::BindOnce(&UnexportableKeyServiceImpl::RunCallbackIfAlive<ArgType>,
                     service_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteKeySlowlyAsync(
    UnexportableKeyId key_id,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<void>)> callback) {
  auto key_id_it = key_by_key_id_.find(key_id);
  if (key_id_it == key_by_key_id_.end()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  std::vector<uint8_t> wrapped_key = key_id_it->second->key().GetWrappedKey();
  auto wrapped_key_it = key_id_by_wrapped_key_.find(wrapped_key);
  CHECK(wrapped_key_it != key_id_by_wrapped_key_.end());
  CHECK(wrapped_key_it->second.HasKeyId());
  CHECK_EQ(wrapped_key_it->second.GetKeyId(), key_id);

  key_by_key_id_.erase(key_id_it);
  key_id_by_wrapped_key_.erase(wrapped_key_it);

  // The type expected by the callback
  using ArgType = ServiceErrorOr<void>;
  task_manager_->DeleteSigningKeySlowlyAsync(
      config_, std::move(wrapped_key), priority,
      base::BindOnce(&UnexportableKeyServiceImpl::RunCallbackIfAlive<ArgType>,
                     service_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

void UnexportableKeyServiceImpl::CopyKeyFromOtherService(
    const UnexportableKeyService& other_service,
    UnexportableKeyId key_id_from_other_service,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  ServiceErrorOr<std::vector<uint8_t>> wrapped_key =
      other_service.GetWrappedKey(key_id_from_other_service);
  if (!wrapped_key.has_value()) {
    std::move(callback).Run(base::unexpected(wrapped_key.error()));
    return;
  }

  // TODO: crbug.com/455538141 - Implement key copy in the task manager.
  FromWrappedSigningKeySlowlyAsync(*wrapped_key, priority, std::move(callback));
}

void UnexportableKeyServiceImpl::DeleteAllKeysSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  key_by_key_id_.clear();

  // Clear the in-memory cache of pending key IDs by moving it to a local
  // variable and run pending callbacks with a failure.
  for (auto& [_, maybe_pending_key_id] :
       std::exchange(key_id_by_wrapped_key_, {})) {
    if (!maybe_pending_key_id.HasKeyId()) {
      maybe_pending_key_id.RunCallbacksWithFailure(ServiceError::kKeyNotFound);
    }
  }

  // Invalidate weak pointers to cancel pending key lookup requests.
  get_all_keys_weak_ptr_factory_.InvalidateWeakPtrs();
  from_wrapped_key_weak_ptr_factory_.InvalidateWeakPtrs();

  // The type expected by the callback
  using ArgType = ServiceErrorOr<size_t>;
  task_manager_->DeleteAllSigningKeysSlowlyAsync(
      config_, priority,
      base::BindOnce(&UnexportableKeyServiceImpl::RunCallbackIfAlive<ArgType>,
                     service_weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)));
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceImpl::GetSubjectPublicKeyInfo(
    UnexportableKeyId key_id) const {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second->key().GetSubjectPublicKeyInfo();
}

ServiceErrorOr<std::vector<uint8_t>> UnexportableKeyServiceImpl::GetWrappedKey(
    UnexportableKeyId key_id) const {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second->key().GetWrappedKey();
}

ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
UnexportableKeyServiceImpl::GetAlgorithm(UnexportableKeyId key_id) const {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  return it->second->key().Algorithm();
}

void UnexportableKeyServiceImpl::OnGetAllSigningKeysForGarbageCollectionSlowly(
    base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
        client_callback,
    ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
        keys_or_error) {
  std::move(client_callback)
      .Run(OnGetAllSigningKeysForGarbageCollectionSlowlyImpl(
          std::move(keys_or_error)));
}

ServiceErrorOr<std::vector<UnexportableKeyId>>
UnexportableKeyServiceImpl::OnGetAllSigningKeysForGarbageCollectionSlowlyImpl(
    ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
        keys_or_error) {
  ASSIGN_OR_RETURN(
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>> keys,
      std::move(keys_or_error));

  std::vector<UnexportableKeyId> key_ids;
  key_ids.reserve(keys.size());
  for (scoped_refptr<RefCountedUnexportableSigningKey>& key : keys) {
    CHECK(key);
    UnexportableKeyId key_id = key->id();
    auto [it, inserted] =
        key_id_by_wrapped_key_.try_emplace(key->key().GetWrappedKey(), key_id);

    if (!inserted) {
      // If insertion failed, it means that there were pending callbacks
      // waiting for the key to be created from the wrapped key.
      MaybePendingUnexportableKeyId& maybe_pending_key_id = it->second;

      if (!maybe_pending_key_id.HasKeyId()) {
        // If there is no key ID yet, it means there are still
        // `FromWrappedKey` requests in flight. In this case, we need set
        // the key ID and run callbacks.
        maybe_pending_key_id.SetKeyIdAndRunCallbacks(key_id);
      } else {
        // Otherwise, this wrapped key has already been assigned to a key
        // ID, and we need to use the existing key ID.
        key_id = maybe_pending_key_id.GetKeyId();
      }
    }

    if (key_id == key->id()) {
      // A newly generated key ID must be unique.
      CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
    }

    key_ids.push_back(key_id);
  }

  return key_ids;
}

void UnexportableKeyServiceImpl::OnKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> client_callback,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  return std::move(client_callback)
      .Run(OnKeyGeneratedImpl(std::move(key_or_error)));
}

ServiceErrorOr<UnexportableKeyId>
UnexportableKeyServiceImpl::OnKeyGeneratedImpl(
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  ASSIGN_OR_RETURN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                   std::move(key_or_error));
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  UnexportableKeyId key_id = key->id();
  if (!key_id_by_wrapped_key_.try_emplace(key->key().GetWrappedKey(), key_id)
           .second) {
    // Drop a newly generated key in the case of a key collision. This should
    // be extremely rare.
    DVLOG(1) << "Collision between an existing and a newly generated key "
                "detected.";
    return base::unexpected(ServiceError::kKeyCollision);
  }
  // A newly generated key ID must be unique.
  CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  return key_id;
}

void UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKey(
    std::vector<uint8_t> wrapped_key,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  auto it = key_id_by_wrapped_key_.find(wrapped_key);
  if (it == key_id_by_wrapped_key_.end()) {
    DVLOG(1) << "`wrapped_key` is unknown, did the key get deleted?";
    return;
  }

  MaybePendingUnexportableKeyId& maybe_pending_callbacks = it->second;
  if (maybe_pending_callbacks.HasKeyId()) {
    // If there is already a key ID for this wrapped key, it means that the key
    // id has been resolved in the meantime, for example through
    // `GetAllSigningKeys...`. In this case, there is nothing to do and we can
    // return immediately.
    return;
  }

  ASSIGN_OR_RETURN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                   std::move(key_or_error), [&](ServiceError error) {
                     auto node = key_id_by_wrapped_key_.extract(it);
                     node.mapped().RunCallbacksWithFailure(error);
                   });
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  DCHECK(wrapped_key == key->key().GetWrappedKey());

  UnexportableKeyId key_id = key->id();
  // A newly created key ID must be unique.
  CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  maybe_pending_callbacks.SetKeyIdAndRunCallbacks(key_id);
}

}  // namespace unexportable_keys
