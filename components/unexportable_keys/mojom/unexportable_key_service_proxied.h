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
#include "components/unexportable_keys/ref_counted_unexportable_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/sign.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace unexportable_keys {

// Holds metadata about a key cached by `UnexportableKeyServiceProxied` in order
// to implement synchronous getter methods.
struct CachedKeyData {
  std::vector<uint8_t> subject_public_key_info;
  std::vector<uint8_t> wrapped_key;
  crypto::sign::SignatureKind algorithm = crypto::sign::ECDSA_SHA256;
  ServiceErrorOr<std::string> key_tag;
  ServiceErrorOr<base::Time> creation_time;
};

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
  void GetAllKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<
          void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback)
      override;
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
  void OnSigningKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          original_callback,
      ServiceErrorOr<mojom::NewSigningKeyDataPtr> result);

  void OnSigningKeyLoaded(
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          original_callback,
      ServiceErrorOr<mojom::NewSigningKeyDataPtr> result);

  void OnAttestationKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
          original_callback,
      ServiceErrorOr<mojom::NewAttestationKeyDataPtr> result);

  void OnAttestationKeyLoaded(
      base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
          original_callback,
      ServiceErrorOr<mojom::NewAttestationKeyDataPtr> result);

  void OnGetAllKeysForGarbageCollection(
      base::OnceCallback<
          void(ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)>
          original_callback,
      ServiceErrorOr<std::vector<mojom::NewSigningKeyDataPtr>> result);

  const mojo::Remote<mojom::UnexportableKeyService> remote_;
  absl::flat_hash_map<UnexportableSigningKeyId, CachedKeyData> key_cache_;
};
}  // namespace unexportable_keys
#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOJOM_UNEXPORTABLE_KEY_SERVICE_PROXIED_H_
