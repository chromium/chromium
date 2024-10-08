// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unexportable_keys/unexportable_key_service_impl.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace unexportable_keys {

namespace {

// Class holding either an `UnexportableKeyId` or a list of callbacks waiting
// for the key creation.
class MaybePendingUnexportableKeyId {
 public:
  using CallbackType =
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>;

  // Constructs an instance holding a list of callbacks.
  MaybePendingUnexportableKeyId();
  // Constructs an instance holding `key_id`.
  explicit MaybePendingUnexportableKeyId(UnexportableKeyId key_id);

  ~MaybePendingUnexportableKeyId();

  // Returns true if a key has been assigned to this instance. Otherwise,
  // returns false which means that this instance holds a list of callbacks.
  bool HasKeyId();

  // This method should be called only if `HasKeyId()` is true.
  UnexportableKeyId GetKeyId();

  // These methods should be called only if `HasKeyId()` is false.
  void AddCallback(CallbackType callback);
  void SetKeyIdAndRunCallbacks(UnexportableKeyId key_id);
  void RunCallbacksWithFailure(ServiceError error);

 private:
  std::vector<CallbackType>& GetCallbacks();

  // Holds the value of its first alternative type by default.
  absl::variant<std::vector<CallbackType>, UnexportableKeyId>
      key_id_or_pending_callbacks_;
};

MaybePendingUnexportableKeyId::MaybePendingUnexportableKeyId() = default;

MaybePendingUnexportableKeyId::MaybePendingUnexportableKeyId(
    UnexportableKeyId key_id)
    : key_id_or_pending_callbacks_(key_id) {}

MaybePendingUnexportableKeyId::~MaybePendingUnexportableKeyId() = default;

bool MaybePendingUnexportableKeyId::HasKeyId() {
  return absl::holds_alternative<UnexportableKeyId>(
      key_id_or_pending_callbacks_);
}

UnexportableKeyId MaybePendingUnexportableKeyId::GetKeyId() {
  CHECK(HasKeyId());
  return absl::get<UnexportableKeyId>(key_id_or_pending_callbacks_);
}

void MaybePendingUnexportableKeyId::AddCallback(CallbackType callback) {
  CHECK(!HasKeyId());
  GetCallbacks().push_back(std::move(callback));
}

void MaybePendingUnexportableKeyId::SetKeyIdAndRunCallbacks(
    UnexportableKeyId key_id) {
  CHECK(!HasKeyId());
  std::vector<CallbackType> callbacks;
  std::swap(callbacks, GetCallbacks());
  key_id_or_pending_callbacks_ = key_id;
  for (auto& callback : callbacks) {
    std::move(callback).Run(key_id);
  }
}

void MaybePendingUnexportableKeyId::RunCallbacksWithFailure(
    ServiceError error) {
  CHECK(!HasKeyId());
  std::vector<CallbackType> callbacks;
  std::swap(callbacks, GetCallbacks());
  for (auto& callback : callbacks) {
    std::move(callback).Run(base::unexpected(error));
  }
}

std::vector<MaybePendingUnexportableKeyId::CallbackType>&
MaybePendingUnexportableKeyId::GetCallbacks() {
  CHECK(!HasKeyId());
  return absl::get<std::vector<CallbackType>>(key_id_or_pending_callbacks_);
}

}  // namespace

UnexportableKeyServiceImpl::UnexportableKeyServiceImpl(
    UnexportableKeyTaskManager& task_manager)
    : task_manager_(task_manager) {}

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
      acceptable_algorithms, priority,
      base::BindOnce(&UnexportableKeyServiceImpl::OnKeyGenerated,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void UnexportableKeyServiceImpl::FromWrappedSigningKeySlowlyAsync(
    base::span<const uint8_t> wrapped_key,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) {
  auto it = key_id_by_wrapped_key_.find(wrapped_key);
  bool is_new = false;
  if (it == key_id_by_wrapped_key_.end()) {
    is_new = true;
    std::tie(it, std::ignore) = key_id_by_wrapped_key_.try_emplace(
        std::vector(wrapped_key.begin(), wrapped_key.end()));
  }

  if (it->second.HasKeyId()) {
    std::move(callback).Run(it->second.GetKeyId());
    return;
  }

  it->second.AddCallback(std::move(callback));

  if (is_new) {
    // As long as `this` is alive, `it` should only be invalidated by the call
    // below.
    task_manager_->FromWrappedSigningKeySlowlyAsync(
        wrapped_key, priority,
        base::BindOnce(&UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKey,
                       weak_ptr_factory_.GetWeakPtr(), it));
  }
}

void UnexportableKeyServiceImpl::SignSlowlyAsync(
    const UnexportableKeyId& key_id,
    base::span<const uint8_t> data,
    BackgroundTaskPriority priority,
    base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback) {
  auto it = key_by_key_id_.find(key_id);
  if (it == key_by_key_id_.end()) {
    std::move(callback).Run(base::unexpected(ServiceError::kKeyNotFound));
    return;
  }
  task_manager_->SignSlowlyAsync(it->second, data, priority,
                                 std::move(callback));
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

void UnexportableKeyServiceImpl::OnKeyGenerated(
    base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> client_callback,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  std::move(client_callback).Run([&]() -> ServiceErrorOr<UnexportableKeyId> {
    if (!key_or_error.has_value()) {
      return base::unexpected(key_or_error.error());
    }
    scoped_refptr<RefCountedUnexportableSigningKey>& key = key_or_error.value();
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
  }());
}

void UnexportableKeyServiceImpl::OnKeyCreatedFromWrappedKey(
    WrappedKeyMap::iterator pending_entry_it,
    ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
        key_or_error) {
  if (!key_or_error.has_value()) {
    auto node = key_id_by_wrapped_key_.extract(pending_entry_it);
    node.mapped().RunCallbacksWithFailure(key_or_error.error());
    return;
  }
  scoped_refptr<RefCountedUnexportableSigningKey>& key = key_or_error.value();
  // `key` must be non-null if `key_or_error` holds a value.
  CHECK(key);
  DCHECK(
      base::ranges::equal(pending_entry_it->first, key->key().GetWrappedKey()));

  UnexportableKeyId key_id = key->id();
  // A newly created key ID must be unique.
  CHECK(key_by_key_id_.try_emplace(key_id, std::move(key)).second);
  pending_entry_it->second.SetKeyIdAndRunCallbacks(key_id);
}

}  // namespace unexportable_keys
