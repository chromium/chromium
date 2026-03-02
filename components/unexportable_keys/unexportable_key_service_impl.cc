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
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"

namespace unexportable_keys {

namespace {

// Returns the application tag from the config on Mac if the provider supports
// stateful unexportable keys. Otherwise, returns an empty string.
std::string_view GetApplicationTag(
    const crypto::UnexportableKeyProvider::Config& config) {
#if BUILDFLAG(IS_MAC)
  if (UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
          config)) {
    return config.application_tag;
  }

  return "";
#else
  return "";
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace

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
  MaybePendingUnexportableKeyId(MaybePendingUnexportableKeyId&&) = default;
  MaybePendingUnexportableKeyId& operator=(MaybePendingUnexportableKeyId&&) =
      default;

  // Constructs an instance holding `key_id`.
  explicit MaybePendingUnexportableKeyId(UnexportableKeyId key_id)
      : pending_callbacks_or_key_id_(key_id) {}

  ~MaybePendingUnexportableKeyId() {
    if (!HasKeyId()) {
      RunCallbacksWithFailure(ServiceError::kOperationCancelled);
    }
  }

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
    BackgroundTaskOrigin task_origin,
    crypto::UnexportableKeyProvider::Config config)
    : task_manager_(task_manager), task_origin_(task_origin), config_(config) {}

UnexportableKeyServiceImpl::~UnexportableKeyServiceImpl() = default;

// static
bool UnexportableKeyServiceImpl::IsUnexportableKeyProviderSupported(
    crypto::UnexportableKeyProvider::Config config) {
  return UnexportableKeyTaskManager::GetUnexportableKeyProvider(
             std::move(config)) != nullptr;
}

// static
bool UnexportableKeyServiceImpl::IsStatefulUnexportableKeyProviderSupported(
    crypto::UnexportableKeyProvider::Config config) {
  std::unique_ptr<crypto::UnexportableKeyProvider> provider =
      UnexportableKeyTaskManager::GetUnexportableKeyProvider(std::move(config));
  return provider != nullptr &&
         provider->AsStatefulUnexportableKeyProvider() != nullptr;
}

void UnexportableKeyServiceImpl::GenerateSigningKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  task_manager_->GenerateSigningKeySlowlyAsync(
      task_origin_, config_, acceptable_algorithms, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(callback),
          // SAFETY: `this` is guaranteed to be alive if the projection callback
          // is invoked.
          base::BindOnce(&UnexportableKeyServiceImpl::OnKeyGeneratedImpl,
                         base::Unretained(this))));
}

void UnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  // Construct a key_view from the wrapped key and application tag stored in the
  // config. Materialize it into the map only if needed.
  //
  // NOTE: In case the key does not exist in the map yet and we ask the backend
  // for the matching signing key, the application tag returned by the platform
  // must match the tag stored in the config. This invariant is CHECKed in
  // `OnKeyCreatedFromWrappedKeyAndTag`.
  const WrappedKeyAndTagView key_view(wrapped_key, GetApplicationTag(config_));
  auto& [wrapped_key_and_tag, maybe_pending_key_id] =
      *key_id_by_wrapped_key_and_tag_.lazy_emplace(
          key_view, [&](const auto& ctor) {
            ctor(Materialize(key_view), MaybePendingUnexportableKeyId());
          });

  if (maybe_pending_key_id.HasKeyId()) {
    std::move(callback).Run(maybe_pending_key_id.GetKeyId());
    return;
  }

  // NOTE: We don't wrap the callback in `WrapCallbackWithErrorIfCancelled`
  // here, but rather run the callbacks explicitly during the destruction of
  // `MaybePendingUnexportableKeyId`.
  size_t n_callbacks = maybe_pending_key_id.AddCallback(std::move(callback));
  if (n_callbacks == 1) {
    // `callback` is the first one waiting for the wrapped key. Schedule the
    // task to create a key from the wrapped key.
    task_manager_->FromWrappedSigningKeySlowlyAsync(
        task_origin_, config_, wrapped_key, priority,
        base::BindOnce(
            &UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKeyAndTag,
            weak_ptr_factory_.GetWeakPtr(), wrapped_key_and_tag));
  }
}

void UnexportableKeyServiceImpl::
    GetAllSigningKeysForGarbageCollectionSlowlyAsync(
        BackgroundTaskPriority priority,
        base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
            callback) {
  task_manager_->GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      task_origin_, config_, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(callback),
          // SAFETY: `this` is guaranteed to be alive if the projection callback
          // is invoked.
          base::BindOnce(&UnexportableKeyServiceImpl::
                             OnGetAllSigningKeysForGarbageCollectionSlowlyImpl,
                         base::Unretained(this))));
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

  task_manager_->SignSlowlyAsync(
      task_origin_, it->second, data, priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteKeysSlowlyAsync(
    base::span<const UnexportableKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  // Delete the keys from the in-memory maps.
  std::vector<ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>>
      keys_or_errors = base::ToVector(key_ids, [&](UnexportableKeyId key_id) {
        return ExtractKeyFromMaps(key_id);
      });

  // Collect the keys that were successfully deleted.
  std::erase_if(keys_or_errors, [](auto& k) { return !k.has_value(); });
  std::vector<scoped_refptr<RefCountedUnexportableSigningKey>> signing_keys =
      base::ToVector(keys_or_errors, [](auto& key) { return *std::move(key); });

  // If no keys were deleted, return an error.
  if (signing_keys.empty()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  task_manager_->DeleteSigningKeysSlowlyAsync(
      task_origin_, config_, std::move(signing_keys), priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteAllKeysSlowlyAsync(
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  key_by_key_id_.clear();
  key_id_by_wrapped_key_and_tag_.clear();

  // Invalidate weak pointers to cancel pending key lookup requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  task_manager_->DeleteAllSigningKeysSlowlyAsync(
      task_origin_, config_, BackgroundTaskPriority::kUserBlocking,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
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

ServiceErrorOr<std::string> UnexportableKeyServiceImpl::GetKeyTag(
    UnexportableKeyId key_id) const {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }

  crypto::StatefulUnexportableSigningKey* stateful_key =
      it->second->key().AsStatefulUnexportableSigningKey();
  if (!stateful_key) {
    return base::unexpected(ServiceError::kOperationNotSupported);
  }
  return stateful_key->GetKeyTag();
}

ServiceErrorOr<base::Time> UnexportableKeyServiceImpl::GetCreationTime(
    UnexportableKeyId key_id) const {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }

  crypto::StatefulUnexportableSigningKey* stateful_key =
      it->second->key().AsStatefulUnexportableSigningKey();
  if (!stateful_key) {
    return base::unexpected(ServiceError::kOperationNotSupported);
  }
  return stateful_key->GetCreationTime();
}

// static
UnexportableKeyServiceImpl::WrappedKeyAndTag
UnexportableKeyServiceImpl::GetWrappedKeyAndTag(
    const RefCountedUnexportableSigningKey& key) {
  std::string tag;
  if (crypto::StatefulUnexportableSigningKey* stateful_key =
          key.key().AsStatefulUnexportableSigningKey()) {
    tag = stateful_key->GetKeyTag();
  }

  return {key.key().GetWrappedKey(), std::move(tag)};
}

// static
UnexportableKeyServiceImpl::WrappedKeyAndTag
UnexportableKeyServiceImpl::Materialize(WrappedKeyAndTagView view) {
  auto [wrapped_key, tag] = view;
  return {base::ToVector(wrapped_key), std::string(tag)};
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
UnexportableKeyServiceImpl::ExtractKeyFromMaps(UnexportableKeyId key_id) {
  auto key_id_it = key_by_key_id_.find(key_id);
  if (key_id_it == key_by_key_id_.end()) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }

  scoped_refptr<RefCountedUnexportableSigningKey> key =
      key_by_key_id_.extract(key_id_it).mapped();

  auto wrapped_key_and_tag_handle =
      key_id_by_wrapped_key_and_tag_.extract(GetWrappedKeyAndTag(*key));

  CHECK(!wrapped_key_and_tag_handle.empty());
  MaybePendingUnexportableKeyId& mapped_key_id =
      wrapped_key_and_tag_handle.mapped();

  CHECK(mapped_key_id.HasKeyId());
  CHECK_EQ(mapped_key_id.GetKeyId(), key_id);
  return key;
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
    auto [it, inserted] = key_id_by_wrapped_key_and_tag_.try_emplace(
        GetWrappedKeyAndTag(*key), key_id);

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

ServiceErrorOr<UnexportableKeyId>
UnexportableKeyServiceImpl::OnKeyGeneratedImpl(
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  ASSIGN_OR_RETURN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                   std::move(key_or_error));
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  UnexportableKeyId key_id = key->id();
  if (!key_id_by_wrapped_key_and_tag_
           .try_emplace(GetWrappedKeyAndTag(*key), key_id)
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

void UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKeyAndTag(
    WrappedKeyAndTag wrapped_key_and_tag,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  auto it = key_id_by_wrapped_key_and_tag_.find(wrapped_key_and_tag);
  if (it == key_id_by_wrapped_key_and_tag_.end()) {
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
                     auto node = key_id_by_wrapped_key_and_tag_.extract(it);
                     node.mapped().RunCallbacksWithFailure(error);
                   });
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  CHECK(wrapped_key_and_tag == GetWrappedKeyAndTag(*key));

  UnexportableKeyId key_id = key->id();
  // A newly created key ID must be unique.
  CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  maybe_pending_callbacks.SetKeyIdAndRunCallbacks(key_id);
}

}  // namespace unexportable_keys
