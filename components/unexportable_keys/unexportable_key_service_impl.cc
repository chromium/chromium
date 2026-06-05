// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include <algorithm>
#include <variant>

#include "base/containers/map_util.h"
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

// Convenience method to create a `WrappedKeyAndTag` from a
// `RefCountedUnexportableKey`.
std::pair<std::vector<uint8_t>, std::string> GetWrappedKeyAndTag(
    const RefCountedUnexportableKey& key) {
  std::string tag;
  if (const crypto::StatefulKey* stateful_key = key.key().AsStatefulKey()) {
    tag = stateful_key->GetKeyTag();
  }

  return {key.key().GetWrappedKey(), std::move(tag)};
}

// Helper function that extracts a key of type `KeyType` from `key_map` by
// `key_id`, and also extracts its matching entry from `wrapped_key_map`. This
// helper avoids code duplication between signing and attestation keys, which
// are stored in separate maps in `UnexportableKeyServiceImpl`.
template <typename KeyType, typename KeyIdType, typename WrappedKeyMap>
ServiceErrorOr<scoped_refptr<RefCountedUnexportableKey>> ExtractKeyFromMap(
    absl::flat_hash_map<KeyIdType, scoped_refptr<KeyType>>& key_map,
    WrappedKeyMap& wrapped_key_map,
    KeyIdType key_id) {
  auto key_handle = key_map.extract(key_id);
  if (!key_handle) {
    return base::unexpected(ServiceError::kKeyNotFound);
  }
  scoped_refptr<KeyType> key = std::move(key_handle.mapped());
  auto wrapped_key_and_tag_handle =
      wrapped_key_map.extract(GetWrappedKeyAndTag(*key));
  CHECK(wrapped_key_and_tag_handle);
  auto& mapped_key_id = wrapped_key_and_tag_handle.mapped();
  CHECK(mapped_key_id.HasKeyId());
  CHECK_EQ(mapped_key_id.GetKeyId(), key_id);
  return key;
}

}  // namespace

// Class holding either an `KeyIdType` or a list of callbacks waiting for the
// key creation.
template <typename KeyIdType>
class MaybePendingUnexportableKeyId {
 public:
  using CallbackType = base::OnceCallback<void(ServiceErrorOr<KeyIdType>)>;
  using PendingCallbacks = std::vector<CallbackType>;
  using PendingCallbacksOrKeyId = std::variant<PendingCallbacks, KeyIdType>;

  // Constructs an instance holding a list of callbacks.
  MaybePendingUnexportableKeyId() = default;
  MaybePendingUnexportableKeyId(MaybePendingUnexportableKeyId&&) = default;
  MaybePendingUnexportableKeyId& operator=(MaybePendingUnexportableKeyId&&) =
      default;

  // Constructs an instance holding `key_id`.
  explicit MaybePendingUnexportableKeyId(KeyIdType key_id)
      : pending_callbacks_or_key_id_(key_id) {}

  ~MaybePendingUnexportableKeyId() {
    if (!HasKeyId()) {
      RunCallbacksWithFailure(ServiceError::kOperationCancelled);
    }
  }

  // Returns true if a key has been assigned to this instance. Otherwise,
  // returns false which means that this instance holds a list of callbacks.
  bool HasKeyId() const {
    return std::holds_alternative<KeyIdType>(pending_callbacks_or_key_id_);
  }

  // This method should be called only if `HasKeyId()` is true.
  KeyIdType GetKeyId() const {
    CHECK(HasKeyId());
    return std::get<KeyIdType>(pending_callbacks_or_key_id_);
  }

  // These methods should be called only if `HasKeyId()` is false.

  // Adds `callback` to the list of callbacks and returns size of the list.
  size_t AddCallback(CallbackType callback) {
    CHECK(!HasKeyId());
    GetCallbacks().push_back(std::move(callback));
    return GetCallbacks().size();
  }

  void SetKeyIdAndRunCallbacks(KeyIdType key_id) {
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
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  task_manager_->GenerateSigningKeySlowlyAsync(
      task_origin_, config_, acceptable_algorithms, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(callback),
          // SAFETY: `this` is guaranteed to be alive if the projection callback
          // is invoked.
          base::BindOnce(&UnexportableKeyServiceImpl::OnSigningKeyGeneratedImpl,
                         base::Unretained(this))));
}

void UnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
        callback) {
  // Construct a key_view from the wrapped key and application tag stored in the
  // config. Materialize it into the map only if needed.
  //
  // NOTE: In case the key does not exist in the map yet and we ask the backend
  // for the matching signing key, the application tag returned by the platform
  // must match the tag stored in the config. This invariant is CHECKed in
  // `OnKeyCreatedFromWrappedKeyAndTag`.
  const WrappedKeyAndTagView key_view(wrapped_key, GetApplicationTag(config_));
  auto& [wrapped_key_and_tag, maybe_pending_key_id] =
      *signing_key_id_by_wrapped_key_and_tag_.lazy_emplace(
          key_view, [&](const auto& ctor) {
            ctor(Materialize(key_view), MaybePendingUnexportableSigningKeyId());
          });

  if (maybe_pending_key_id.HasKeyId()) {
    std::move(callback).Run(maybe_pending_key_id.GetKeyId());
    return;
  }

  // NOTE: We don't wrap the callback in `WrapCallbackWithErrorIfCancelled`
  // here, but rather run the callbacks explicitly during the destruction of
  // `MaybePendingUnexportableSigningKeyId`.
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

void UnexportableKeyServiceImpl::GenerateAttestationKeySlowlyAsync(
    base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
        acceptable_algorithms,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // TODO(crbug.com/501306852): Implement this.
  std::move(callback).Run(
      base::unexpected(ServiceError::kOperationNotSupported));
}

void UnexportableKeyServiceImpl::FromWrappedAttestationKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
        callback) {
  // TODO(crbug.com/501306852): Implement this.
  std::move(callback).Run(
      base::unexpected(ServiceError::kOperationNotSupported));
}

void UnexportableKeyServiceImpl::GetAllKeysForGarbageCollectionSlowlyAsync(
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
        callback) {
  task_manager_->GetAllKeysForGarbageCollectionSlowlyAsync(
      task_origin_, config_, priority,
      WrapCallbackWithErrorIfCancelled(
          std::move(callback),
          // SAFETY: `this` is guaranteed to be alive if the projection callback
          // is invoked.
          base::BindOnce(&UnexportableKeyServiceImpl::
                             OnGetAllKeysForGarbageCollectionSlowlyImpl,
                         base::Unretained(this))));
}

void UnexportableKeyServiceImpl::SignSlowlyAsync(
    UnexportableSigningKeyId key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  const auto* key = base::FindOrNull(signing_key_by_key_id_, key_id);
  if (!key) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  task_manager_->SignSlowlyAsync(
      task_origin_, *key, data, priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteKeysSlowlyAsync(
    base::span<const UnexportableKeyId> key_ids,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  // Delete the keys from the in-memory maps.
  std::vector<ServiceErrorOr<scoped_refptr<RefCountedUnexportableKey>>>
      keys_or_errors = base::ToVector(key_ids, [&](UnexportableKeyId key_id) {
        return ExtractKeyFromMaps(key_id);
      });

  // Collect the keys that were successfully deleted.
  std::erase_if(keys_or_errors, [](auto& k) { return !k.has_value(); });
  std::vector<scoped_refptr<RefCountedUnexportableKey>> keys_to_delete =
      base::ToVector(keys_or_errors, [](auto& key) { return *std::move(key); });

  // If no keys were deleted, return an error.
  if (keys_to_delete.empty()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }

  task_manager_->DeleteKeysSlowlyAsync(
      task_origin_, config_, std::move(keys_to_delete), priority,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

void UnexportableKeyServiceImpl::DeleteAllKeysSlowlyAsync(
    base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) {
  signing_key_by_key_id_.clear();
  attestation_key_by_key_id_.clear();
  all_gc_keys_by_key_id_.clear();
  signing_key_id_by_wrapped_key_and_tag_.clear();
  attestation_key_id_by_wrapped_key_and_tag_.clear();

  // Invalidate weak pointers to cancel pending key lookup requests.
  weak_ptr_factory_.InvalidateWeakPtrs();

  task_manager_->DeleteAllKeysSlowlyAsync(
      task_origin_, config_, BackgroundTaskPriority::kUserBlocking,
      WrapCallbackWithErrorIfCancelled(std::move(callback)));
}

ServiceErrorOr<std::vector<uint8_t>>
UnexportableKeyServiceImpl::GetSubjectPublicKeyInfo(
    UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableKey* key, GetKey(key_id));
  return key->GetSubjectPublicKeyInfo();
}

ServiceErrorOr<std::vector<uint8_t>> UnexportableKeyServiceImpl::GetWrappedKey(
    UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableKey* key, GetKey(key_id));
  return key->GetWrappedKey();
}

ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
UnexportableKeyServiceImpl::GetAlgorithm(UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableKey* key, GetKey(key_id));
  return key->Algorithm();
}

ServiceErrorOr<std::string> UnexportableKeyServiceImpl::GetKeyTag(
    UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::StatefulKey* stateful_key,
                   GetStatefulKey(key_id));
  return stateful_key->GetKeyTag();
}

ServiceErrorOr<base::Time> UnexportableKeyServiceImpl::GetCreationTime(
    UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::StatefulKey* stateful_key,
                   GetStatefulKey(key_id));
  return stateful_key->GetCreationTime();
}

// static
UnexportableKeyServiceImpl::WrappedKeyAndTag
UnexportableKeyServiceImpl::Materialize(WrappedKeyAndTagView view) {
  auto [wrapped_key, tag] = view;
  return {base::ToVector(wrapped_key), std::string(tag)};
}

ServiceErrorOr<const crypto::UnexportableKey*>
UnexportableKeyServiceImpl::GetKey(UnexportableKeyId key_id) const {
  if (const auto* key = base::FindOrNull(signing_key_by_key_id_,
                                         UnexportableSigningKeyId(key_id))) {
    return &(*key)->key();
  }
  if (const auto* key = base::FindOrNull(
          attestation_key_by_key_id_, UnexportableAttestationKeyId(key_id))) {
    return &(*key)->key();
  }
  if (const auto* key = base::FindOrNull(all_gc_keys_by_key_id_, key_id)) {
    return &(*key)->key();
  }
  return base::unexpected(ServiceError::kKeyNotFound);
}

ServiceErrorOr<const crypto::StatefulKey*>
UnexportableKeyServiceImpl::GetStatefulKey(UnexportableKeyId key_id) const {
  ASSIGN_OR_RETURN(const crypto::UnexportableKey* key, GetKey(key_id));
  if (const crypto::StatefulKey* stateful_key = key->AsStatefulKey()) {
    return stateful_key;
  }
  return base::unexpected(ServiceError::kOperationNotSupported);
}

ServiceErrorOr<scoped_refptr<RefCountedUnexportableKey>>
UnexportableKeyServiceImpl::ExtractKeyFromMaps(UnexportableKeyId key_id) {
  // Check the garbage collection map first. Ensure the `key_id` can't be
  // present in the other maps.
  if (auto gc_key_handle = all_gc_keys_by_key_id_.extract(key_id)) {
    CHECK(!signing_key_by_key_id_.contains(UnexportableSigningKeyId(key_id)));
    CHECK(!attestation_key_by_key_id_.contains(
        UnexportableAttestationKeyId(key_id)));
    return std::move(gc_key_handle.mapped());
  }

  if (auto res = ExtractKeyFromMap(signing_key_by_key_id_,
                                   signing_key_id_by_wrapped_key_and_tag_,
                                   UnexportableSigningKeyId(key_id));
      res.has_value()) {
    return res;
  }

  return ExtractKeyFromMap(attestation_key_by_key_id_,
                           attestation_key_id_by_wrapped_key_and_tag_,
                           UnexportableAttestationKeyId(key_id));
}

ServiceErrorOr<std::vector<UnexportableKeyId>>
UnexportableKeyServiceImpl::OnGetAllKeysForGarbageCollectionSlowlyImpl(
    ServiceErrorOr<std::vector<scoped_refptr<RefCountedUnexportableKey>>>
        keys_or_error) {
  ASSIGN_OR_RETURN(std::vector<scoped_refptr<RefCountedUnexportableKey>> keys,
                   std::move(keys_or_error));

  auto key_ids = base::ToVector(keys, [](auto& key) { return key->id(); });
  all_gc_keys_by_key_id_.clear();
  all_gc_keys_by_key_id_.reserve(keys.size());
  for (auto& key : keys) {
    all_gc_keys_by_key_id_.emplace(key->id(), std::move(key));
  }
  return key_ids;
}

ServiceErrorOr<UnexportableSigningKeyId>
UnexportableKeyServiceImpl::OnSigningKeyGeneratedImpl(
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  ASSIGN_OR_RETURN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                   std::move(key_or_error));
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  UnexportableSigningKeyId key_id(key->id());
  if (!signing_key_id_by_wrapped_key_and_tag_
           .try_emplace(GetWrappedKeyAndTag(*key), key_id)
           .second) {
    // Drop a newly generated key in the case of a key collision. This should
    // be extremely rare.
    DVLOG(1) << "Collision between an existing and a newly generated key "
                "detected.";
    return base::unexpected(ServiceError::kKeyCollision);
  }
  // A newly generated key ID must be unique.
  CHECK(signing_key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  return UnexportableSigningKeyId(key_id);
}

void UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKeyAndTag(
    WrappedKeyAndTag wrapped_key_and_tag,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  auto it = signing_key_id_by_wrapped_key_and_tag_.find(wrapped_key_and_tag);
  if (it == signing_key_id_by_wrapped_key_and_tag_.end()) {
    DVLOG(1) << "`wrapped_key` is unknown, did the key get deleted?";
    return;
  }

  MaybePendingUnexportableSigningKeyId& maybe_pending_callbacks = it->second;
  if (maybe_pending_callbacks.HasKeyId()) {
    // If there is already a key ID for this wrapped key, it means that the key
    // id has been resolved in the meantime. In this case, there is nothing to
    // do and we can return immediately.
    return;
  }

  ASSIGN_OR_RETURN(scoped_refptr<RefCountedUnexportableSigningKey> key,
                   std::move(key_or_error), [&](ServiceError error) {
                     auto node =
                         signing_key_id_by_wrapped_key_and_tag_.extract(it);
                     node.mapped().RunCallbacksWithFailure(error);
                   });
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  CHECK(wrapped_key_and_tag == GetWrappedKeyAndTag(*key));

  UnexportableSigningKeyId key_id(key->id());
  // A newly created key ID must be unique.
  CHECK(signing_key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  maybe_pending_callbacks.SetKeyIdAndRunCallbacks(key_id);
}

}  // namespace unexportable_keys
