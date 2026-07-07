// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_FAKE_UNEXPORTABLE_KEY_SERVICE_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_FAKE_UNEXPORTABLE_KEY_SERVICE_H_

#include "components/unexportable_keys/unexportable_key_service.h"

namespace unexportable_keys {

// Fake implementation of `UnexportableKeyService` that returns error to all
// requests.
class FakeUnexportableKeyService : public UnexportableKeyService {
 public:
  // UnexportableKeyService:
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          callback) override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
          callback) override;
  void GenerateAttestationKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
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
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<std::string> GetKeyTag(
      UnexportableSigningKeyId key_id) const override;
  ServiceErrorOr<base::Time> GetCreationTime(
      UnexportableSigningKeyId key_id) const override;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_FAKE_UNEXPORTABLE_KEY_SERVICE_H_
