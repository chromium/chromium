// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_

#include <algorithm>
#include <concepts>
#include <functional>

#include "base/containers/enum_set.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

namespace unexportable_keys {

// Concept defining the valid wrapper classes that can be stored and managed
// within the spare key pool.
template <typename T>
concept SparePoolKeyType =
    std::same_as<T, RefCountedUnexportableSigningKey> ||
    std::same_as<T, RefCountedUnexportableAttestationKey>;

// LINT.IfChange(SpareKeyPoolRetrievalResult)
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SpareKeyPoolRetrievalResult {
  kHit = 0,
  // We haven't initialized spare keys yet.
  kMissNotInitialized = 1,
  // We tried to initialize a spare key but creation failed.
  kMissFailedToCreateSpareKey = 2,
  // We decided to not create a key for this algorithm.
  kMissNoKeyForAlgorithm = 3,
  // A key got requested before we finished replenishing the pool.
  kMissDidNotReplenishFromLastUse = 4,
  // The hardware doesn't support the requested algorithm.
  kAlgorithmNotSupported = 5,
  kMaxValue = kAlgorithmNotSupported,
};
// LINT.ThenChange(/tools/metrics/histograms/metadata/net/enums.xml:SpareKeyPoolRetrievalResult)

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
      base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          callback) override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          callback) override;
  void GenerateAttestationKeySlowlyAsync(
      base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
          callback) override;
  void FromWrappedAttestationKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
          callback) override;
  void GetAllKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<
          void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback)
      override;
  void SignSlowlyAsync(
      UnexportableSigningKeyId key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  void CertifySlowlyAsync(
      UnexportableAttestationKeyId attestation_key_id,
      UnexportableSigningKeyId signing_key_id,
      base::span<const uint8_t> challenge,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
          callback) override;
  void DeleteKeysSlowlyAsync(
      base::span<const UnexportableSigningKeyId> key_ids,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) override;
  void DeleteAllKeysSlowlyAsync(
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) override;
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<crypto::sign::SignatureKind> GetAlgorithm(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<std::string> GetKeyTag(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<base::Time> GetCreationTime(
      UnexportableSigningKeyId key_id) const override;

 private:
  using AllKeysForGarbageCollectionMap =
      absl::flat_hash_map<UnexportableSigningKeyId,
                          scoped_refptr<RefCountedUnexportableSigningKey>>;

  // Repositories storing and managing the lifetime of loaded unexportable
  // signing and attestation keys, respectively.
  template <typename RefCountedKeyType>
  class KeyRepository;

  using SigningKeyRepository = KeyRepository<RefCountedUnexportableSigningKey>;
  using AttestationKeyRepository =
      KeyRepository<RefCountedUnexportableAttestationKey>;

  // A class template that maintains a background-replenished pool
  // of idle pre-generated keys (of a specific key type) to mitigate the
  // significant latency (~1s) of on-demand Windows TPM key generation.
  template <typename KeyType>
    requires SparePoolKeyType<KeyType>
  class SpareKeyPool;

  using SpareSigningKeyPool = SpareKeyPool<RefCountedUnexportableSigningKey>;
  using SpareAttestationKeyPool =
      SpareKeyPool<RefCountedUnexportableAttestationKey>;

  // Identifies which internal repository or map holds the unexportable key.
  enum class KeyMap {
    // Look up key in `signing_keys_`.
    kSigning = 0,
    // Look up key in `attestation_keys_`.
    kAttestation = 1,
    // Look up key in `all_gc_keys_by_key_id_`.
    kGarbageCollection = 2,
  };

  using KeyMapSet =
      base::EnumSet<KeyMap, KeyMap::kSigning, KeyMap::kGarbageCollection>;

  // Convenient set to search all non-GC keys.
  static constexpr KeyMapSet kSigningAndAttestationKeyMaps = {
      KeyMap::kSigning,
      KeyMap::kAttestation,
  };

  // Returns the ref-counted key wrapper corresponding to `key_id`. Performs
  // lookup in the key maps specified in `key_map_set`. Returns nullptr if the
  // key is not found in any set.
  RefCountedUnexportableSigningKey* GetKey(
      KeyMapSet key_map_set,
      UnexportableSigningKeyId key_id) const;

  // Removes the key with `key_id` from the in-memory maps.
  // Returns the mapped key on success, or `ServiceError::kKeyNotFound` if the
  // key was not found.
  ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
  ExtractKeyFromMaps(UnexportableSigningKeyId key_id);

  // Callback for `GetAllKeysForGarbageCollectionSlowlyAsync()`.
  ServiceErrorOr<std::vector<UnexportableSigningKeyId>>
  OnGetAllKeysForGarbageCollectionSlowlyImpl(
      ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>
          keys_or_error);

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

  // Use the Pimpl (Pointer to IMPLementation) pattern to hide helper
  // KeyRepository template declaration details from the header file.
  const std::unique_ptr<SigningKeyRepository> signing_keys_;
  const std::unique_ptr<AttestationKeyRepository> attestation_keys_;

  // Stores all unexportable keys for garbage collection purposes. This map is
  // disjoint from maps inside `signing_keys_` and `attestation_keys_`, and will
  // be overwritten on each call to `GetAllKeysForGarbageCollection`.
  AllKeysForGarbageCollectionMap all_gc_keys_by_key_id_;

  // Spare key pools for preemptively generating and caching
  // hardware-backed keys in the background to mitigate the significant
  // latency (~1s) of on-demand Windows TPM key generation.
  std::unique_ptr<SpareSigningKeyPool> spare_signing_key_pool_;
  std::unique_ptr<SpareAttestationKeyPool> spare_attestation_key_pool_;

  base::WeakPtrFactory<UnexportableKeyServiceImpl> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_IMPL_H_
