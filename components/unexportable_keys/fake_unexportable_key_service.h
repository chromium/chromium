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
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback)
      override;
  void SignSlowlyAsync(
      const UnexportableKeyId& key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback)
      override;
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const override;
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableKeyId key_id) const override;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_FAKE_UNEXPORTABLE_KEY_SERVICE_H_
