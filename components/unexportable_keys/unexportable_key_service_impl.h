// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

#include <algorithm>
#include <functional>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/hash_container_defaults.h"

namespace unexportable_keys {

class MaybePendingUnexportableKeyId;

class UnexportableKeyTaskManager;

class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) UnexportableKeyServiceImpl
    : public UnexportableKeyService {
 public:
  // `task_manager` must outlive `UnexportableKeyServiceImpl`.
  explicit UnexportableKeyServiceImpl(
      UnexportableKeyTaskManager& task_manager,
      BackgroundTaskOrigin task_origin,
      crypto::UnexportableKeyProvider::Config config);

  ~UnexportableKeyServiceImpl() override;

  // Returns whether the current platform has a support for unexportable signing
  // keys. If this returns false, all service methods will return
  // `ServiceError::kNoKeyProvider`.
  static bool IsUnexportableKeyProviderSupported(
      crypto::UnexportableKeyProvider::Config config);

  // Returns whether the current platform has a support for stateful
  // unexportable signing keys. If this returns false, the service methods
  // requiring stateful keys will be no-ops and will return one of the following
  // results:
  // - `ServiceError::kNoKeyProvider` if unexportable keys aren't supported
  //    on the platform in general,
  // - `ServiceError::kOperationNotSupported` if an operation cannot produce a
  //   meaningful result without stateful key support
  // - Empty result otherwise
  static bool IsStatefulUnexportableKeyProviderSupported(
      crypto::UnexportableKeyProvider::Config config);

  // UnexportableKeyService:
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
          callback) override;
  void SignSlowlyAsync(
      UnexportableKeyId key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  void DeleteKeysSlowlyAsync(
      base::span<const UnexportableKeyId> key_ids,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) override;
  void DeleteAllKeysSlowlyAsync(
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) override;
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<std::string> GetKeyTag(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<base::Time> GetCreationTime(
      UnexportableKeyId key_id) const override;

 private:
  using WrappedKeyAndTag = std::pair<std::vector<uint8_t>, std::string>;
  using WrappedKeyAndTagView =
      std::pair<base::span<const uint8_t>, std::string_view>;

  // Hasher object that allows lookups with `WrappedKeyAndTagView` using
  // `WrappedKeyAndTag` as a key.
  struct WrappedKeyAndTagViewHash
      : absl::DefaultHashContainerHash<WrappedKeyAndTagView> {
    using is_transparent = void;
  };

  using WrappedKeyAndTagMap = absl::flat_hash_map<WrappedKeyAndTag,
                                                  MaybePendingUnexportableKeyId,
                                                  WrappedKeyAndTagViewHash,
                                                  std::ranges::equal_to>;
  using KeyIdMap =
      absl::flat_hash_map<UnexportableKeyId,
                          scoped_refptr<RefCountedUnexportableSigningKey>>;

  // Convenience method to create a `WrappedKeyAndTag` from a
  // `RefCountedUnexportableSigningKey`.
  static WrappedKeyAndTag GetWrappedKeyAndTag(
      const RefCountedUnexportableSigningKey& key);

  // Convenience method to create a `WrappedKeyAndTag` from a
  // `WrappedKeyAndTagView`.
  static WrappedKeyAndTag Materialize(WrappedKeyAndTagView view);

  // Removes the key with `key_id` from the in-memory maps.
  // Returns the mapped signing key on success, or
  // `ServiceError::kKeyNotFound` if the key was not found.
  ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
  ExtractKeyFromMaps(UnexportableKeyId key_id);

  // Callback for `GetAllSigningKeysForGarbageCollectionSlowlyAsync()`.
  ServiceErrorOr<std::vector<UnexportableKeyId>>
  OnGetAllSigningKeysForGarbageCollectionSlowlyImpl(
      ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
          keys_or_error);

  // Callback for `GenerateSigningKeySlowlyAsync()`.
  ServiceErrorOr<UnexportableKeyId> OnKeyGeneratedImpl(
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Callback for `FromWrappedSigningKeySlowlyAsync()`.
  void OnKeyCreatedFromWrappedKeyAndTag(
      WrappedKeyAndTag wrapped_key_and_tag,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Generic trampoline that runs the callback only if the WeakPtr used to bind
  // this method is still valid. In case it is not, the callback is run with
  // `ServiceError::kOperationCancelled`.
  //
  // Supports an optional projection function that can be used to transform the
  // result before passing it to the callback.
  template <typename T, typename U = T>
  base::OnceCallback<void(ServiceErrorOr<U>)> WrapCallbackWithErrorIfCancelled(
      base::OnceCallback<void(ServiceErrorOr<T>)> callback,
      base::OnceCallback<ServiceErrorOr<T>(ServiceErrorOr<U>)> proj =
          base::BindOnce([](ServiceErrorOr<U> result) { return result; })) {
    return base::BindOnce(
        [](base::WeakPtr<UnexportableKeyServiceImpl> weak_ptr,
           base::OnceCallback<void(ServiceErrorOr<T>)> callback,
           base::OnceCallback<ServiceErrorOr<T>(ServiceErrorOr<U>)> proj,
           ServiceErrorOr<U> result) {
          std::move(callback).Run(
              weak_ptr ? std::move(proj).Run(std::move(result))
                       : base::unexpected(ServiceError::kOperationCancelled));
        },
        weak_ptr_factory_.GetWeakPtr(), std::move(callback), std::move(proj));
  }

  const raw_ref<UnexportableKeyTaskManager, DanglingUntriaged> task_manager_;
  const BackgroundTaskOrigin task_origin_;

  const crypto::UnexportableKeyProvider::Config config_;

  // Helps mapping multiple `FromWrappedSigningKeySlowlyAsync()` requests with
  // the same (wrapped key, tag) pair into the same key ID.
  WrappedKeyAndTagMap key_id_by_wrapped_key_and_tag_;

  // Stores unexportable signing keys that were created during the current
  // session.
  KeyIdMap key_by_key_id_;

  base::WeakPtrFactory<UnexportableKeyServiceImpl> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
