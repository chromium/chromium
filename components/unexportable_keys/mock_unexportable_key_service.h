// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_SERVICE_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_SERVICE_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "crypto/signature_verifier.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

class MockUnexportableKeyService : public UnexportableKeyService {
 public:
  MockUnexportableKeyService();
  ~MockUnexportableKeyService() override;

  MOCK_METHOD(
      void,
      GenerateSigningKeySlowlyAsync,
      (base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
           acceptable_algorithms,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback),
      (override));
  MOCK_METHOD(
      void,
      FromWrappedSigningKeySlowlyAsync,
      (base::span<const uint8_t> wrapped_key,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback),
      (override));
  MOCK_METHOD(
      void,
      SignSlowlyAsync,
      (const UnexportableKeyId& key_id,
       base::span<const uint8_t> data,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback),
      (override));
  MOCK_METHOD(ServiceErrorOr<std::vector<uint8_t>>,
              GetSubjectPublicKeyInfo,
              (UnexportableKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<std::vector<uint8_t>>,
              GetWrappedKey,
              (UnexportableKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>,
              GetAlgorithm,
              (UnexportableKeyId key_id),
              (const, override));
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_SERVICE_H_
