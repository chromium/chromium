// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

#include <algorithm>
#include <functional>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/container/hash_container_defaults.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {

class MaybePendingUnexportableKeyId;

class UnexportableKeyTaskManager;

class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) UnexportableKeyServiceImpl
    : public UnexportableKeyService {
 public:
  // `task_manager` must outlive `UnexportableKeyServiceImpl`.
  explicit UnexportableKeyServiceImpl(
      UnexportableKeyTaskManager& task_manager,
      crypto::UnexportableKeyProvider::Config config);

  ~UnexportableKeyServiceImpl() override;

  // Returns whether the current platform has a support for unexportable signing
  // keys. If this returns false, all service methods will return
  // `ServiceError::kNoKeyProvider`.
  static bool IsUnexportableKeyProviderSupported(
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
  void CopyKeyFromOtherService(
      const UnexportableKeyService& other_service,
      UnexportableKeyId key_id_from_other_service,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void SignSlowlyAsync(
      UnexportableKeyId key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  void DeleteKeySlowlyAsync(
      UnexportableKeyId key_id,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<void>)> callback) override;
  void DeleteAllKeysSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) override;
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableKeyId key_id) const override;

 private:
  // Hasher object that allows comparing containers of different types that
  // are convertible to base::span<const uint8_t>.
  struct WrappedKeyHash
      : absl::DefaultHashContainerHash<base::span<const uint8_t>> {
    using is_transparent = void;
  };

  using WrappedKeyMap = absl::flat_hash_map<std::vector<uint8_t>,
                                            MaybePendingUnexportableKeyId,
                                            WrappedKeyHash,
                                            std::ranges::equal_to>;
  using KeyIdMap =
      absl::flat_hash_map<UnexportableKeyId,
                          scoped_refptr<RefCountedUnexportableSigningKey>>;

  // Callback for `GetAllSigningKeysForGarbageCollectionSlowlyAsync()`.
  void OnGetAllSigningKeysForGarbageCollectionSlowly(
      base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
          client_callback,
      ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
          keys_or_error);
  ServiceErrorOr<std::vector<UnexportableKeyId>>
  OnGetAllSigningKeysForGarbageCollectionSlowlyImpl(
      ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
          keys_or_error);

  // Callback for `GenerateSigningKeySlowlyAsync()`.
  void OnKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
          client_callback,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);
  ServiceErrorOr<UnexportableKeyId> OnKeyGeneratedImpl(
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Callback for `FromWrappedSigningKeySlowlyAsync()`.
  void OnKeyCreatedFromWrappedKey(
      std::vector<uint8_t> wrapped_key,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Generic trampoline that runs the callback only if the WeakPtr used to bind
  // this method is still valid.
  template <typename... Args>
  void RunCallbackIfAlive(base::OnceCallback<void(Args...)> callback,
                          Args... args) {
    std::move(callback).Run(std::forward<Args>(args)...);
  }

  const raw_ref<UnexportableKeyTaskManager, DanglingUntriaged> task_manager_;

  const crypto::UnexportableKeyProvider::Config config_;

  // Helps mapping multiple `FromWrappedSigningKeySlowlyAsync()` requests with
  // the same wrapped key into the same key ID.
  WrappedKeyMap key_id_by_wrapped_key_;

  // Stores unexportable signing keys that were created during the current
  // session.
  KeyIdMap key_by_key_id_;

  base::WeakPtrFactory<UnexportableKeyServiceImpl>
      get_all_keys_weak_ptr_factory_{this};
  base::WeakPtrFactory<UnexportableKeyServiceImpl>
      generate_key_weak_ptr_factory_{this};
  base::WeakPtrFactory<UnexportableKeyServiceImpl>
      from_wrapped_key_weak_ptr_factory_{this};
  base::WeakPtrFactory<UnexportableKeyServiceImpl> service_weak_ptr_factory_{
      this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
