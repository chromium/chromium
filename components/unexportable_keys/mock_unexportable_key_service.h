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
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace unexportable_keys {

class MockUnexportableKeyService : public UnexportableKeyService {
 public:
  MockUnexportableKeyService();
  ~MockUnexportableKeyService() override;

  MOCK_METHOD(
      void,
      GenerateSigningKeySlowlyAsync,
      (base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      FromWrappedSigningKeySlowlyAsync,
      (base::span<const uint8_t> wrapped_key,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableSigningKeyId>)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      GenerateAttestationKeySlowlyAsync,
      (base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      FromWrappedAttestationKeySlowlyAsync,
      (base::span<const uint8_t> wrapped_key,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<UnexportableAttestationKeyId>)>
           callback),
      (override));
  MOCK_METHOD(
      void,
      GetAllKeysForGarbageCollectionSlowlyAsync,
      (BackgroundTaskPriority priority,
       base::OnceCallback<void(
           ServiceErrorOr<std::vector<UnexportableSigningKeyId>>)> callback),
      (override));
  MOCK_METHOD(
      void,
      SignSlowlyAsync,
      (UnexportableSigningKeyId key_id,
       base::span<const uint8_t> data,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback),
      (override));
  MOCK_METHOD(
      void,
      CertifySlowlyAsync,
      (UnexportableAttestationKeyId attestation_key_id,
       UnexportableSigningKeyId signing_key_id,
       base::span<const uint8_t> challenge,
       BackgroundTaskPriority priority,
       base::OnceCallback<void(ServiceErrorOr<crypto::AttestationStatement>)>
           callback),
      (override));
  MOCK_METHOD(void,
              DeleteKeysSlowlyAsync,
              (base::span<const UnexportableSigningKeyId> key_ids,
               BackgroundTaskPriority priority,
               base::OnceCallback<void(ServiceErrorOr<size_t>)> callback),
              (override));
  MOCK_METHOD(void,
              DeleteAllKeysSlowlyAsync,
              (base::OnceCallback<void(ServiceErrorOr<size_t>)> callback),
              (override));
  MOCK_METHOD(ServiceErrorOr<std::vector<uint8_t>>,
              GetSubjectPublicKeyInfo,
              (UnexportableSigningKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<std::vector<uint8_t>>,
              GetWrappedKey,
              (UnexportableSigningKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<crypto::sign::SignatureKind>,
              GetAlgorithm,
              (UnexportableSigningKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<std::string>,
              GetKeyTag,
              (UnexportableSigningKeyId key_id),
              (const, override));
  MOCK_METHOD(ServiceErrorOr<base::Time>,
              GetCreationTime,
              (UnexportableSigningKeyId key_id),
              (const, override));

  // Delegates all unconfigured mock calls to the real `service`.
  // `service` must outlive this mock. Use `testing::Mock::VerifyAndClear()`
  // to clear the state and stop delegating.
  void DelegateToService(UnexportableKeyService& service);
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_MOCK_UNEXPORTABLE_KEY_SERVICE_H_
