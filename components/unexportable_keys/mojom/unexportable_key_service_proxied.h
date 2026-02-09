// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXIED_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXIED_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/mojom/unexportable_key_service.mojom.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {

// `UnexportableKeyService` implementation forwarding most requests to a
// remote `UnexportableKeyService` instance via Mojo. It is supposed to be used
// in less-privileged processes that don't have direct access to platform's
// cryptographic primitives.
//
// `UnexportableKeyServiceProxied` caches some information about the keys
// locally in order to implement synchronous methods.
class UnexportableKeyServiceProxied : public UnexportableKeyService {
 public:
  explicit UnexportableKeyServiceProxied(
      mojo::PendingRemote<mojom::UnexportableKeyService> pending_remote);

  ~UnexportableKeyServiceProxied() override;

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
  void SignSlowlyAsync(
      const UnexportableKeyId key_id,
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
  void GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
          callback) override;
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
  const mojo::Remote<mojom::UnexportableKeyService> remote_;

  struct CachedKeyData {
    CachedKeyData();
    explicit CachedKeyData(const mojom::NewKeyDataPtr& new_key_data);

    CachedKeyData(const CachedKeyData& other);
    CachedKeyData& operator=(const CachedKeyData& other);
    CachedKeyData(CachedKeyData&& other) noexcept;
    CachedKeyData& operator=(CachedKeyData&& other);

    ~CachedKeyData();

    std::vector<uint8_t> subject_public_key_info;
    std::vector<uint8_t> wrapped_key;
    crypto::SignatureVerifier::SignatureAlgorithm algorithm;
    ServiceErrorOr<std::string> key_tag;
    ServiceErrorOr<base::Time> creation_time;
  };

  void OnKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
          original_callback,
      ServiceErrorOr<mojom::NewKeyDataPtr> result);

  void OnKeyLoaded(base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
                       original_callback,
                   ServiceErrorOr<mojom::NewKeyDataPtr> result);

  void OnGetAllSigningKeysForGarbageCollection(
      base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
          original_callback,
      ServiceErrorOr<std::vector<mojom::NewKeyDataPtr>> result);

  absl::flat_hash_map<UnexportableKeyId, CachedKeyData> key_cache_;
};
}  // namespace unexportable_keys
#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXIED_H_
