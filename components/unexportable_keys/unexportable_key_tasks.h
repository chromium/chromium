// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_

#include <cstdint>
#include <memory>
#include <optional>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/background_task_impl.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "crypto/signature_verifier.h"
#include "crypto/unexportable_key.h"

namespace crypto {
class UnexportableKeyProvider;
}  // namespace crypto

namespace unexportable_keys {

class RefCountedUnexportableKey;
class RefCountedUnexportableSigningKey;
class RefCountedUnexportableAttestationKey;

// A `BackgroundTask` to retrieve all `crypto::UnexportableKey`s from the
// key provider.
class GetAllKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableKey>>>> {
 public:
  GetAllKeysTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(GetAllKeysTask::ReturnType, size_t)> callback);
};

// A `BackgroundTask` to generate a new `crypto::UnexportableSigningKey`.
class GenerateKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>> {
 public:
  GenerateKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(GenerateKeyTask::ReturnType, size_t)> callback);
};

// A `BackgroundTask` to create a `crypto::UnexportableSigningKey` from a
// wrapped key.
class FromWrappedKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>> {
 public:
  FromWrappedKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(FromWrappedKeyTask::ReturnType, size_t)>
          callback);
};

// A `BackgroundTask` to sign data with `crypto::UnexportableSigningKey`.
class SignTask : public internal::BackgroundTaskImpl<
                     ServiceErrorOr<std::vector<uint8_t>>> {
 public:
  SignTask(scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
           base::span<const uint8_t> data,
           BackgroundTaskPriority priority,
           size_t max_retries,
           base::OnceCallback<void(SignTask::ReturnType, size_t)> callback);

 protected:
  bool ShouldRetryBasedOnResult(
      const ServiceErrorOr<std::vector<uint8_t>>& result) const override;
};

// A `BackgroundTask` to delete a collection of `crypto::UnexportableKey`.
class DeleteKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<size_t>> {
 public:
  DeleteKeysTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      std::vector<scoped_refptr<RefCountedUnexportableKey>> keys,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(DeleteKeysTask::ReturnType, size_t)> callback);
};

// A `BackgroundTask` to delete all `crypto::UnexportableKey`s matching the key
// provider config.
class DeleteAllKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<size_t>> {
 public:
  DeleteAllKeysTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(DeleteAllKeysTask::ReturnType, size_t)> callback);
};

// A `BackgroundTask` to generate a new `crypto::UnexportableAttestationKey`.
class GenerateAttestationKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>> {
 public:
  GenerateAttestationKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(GenerateAttestationKeyTask::ReturnType, size_t)>
          callback);
};

// A `BackgroundTask` to create a `crypto::UnexportableAttestationKey` from a
// wrapped key.
class FromWrappedAttestationKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>> {
 public:
  FromWrappedAttestationKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(FromWrappedAttestationKeyTask::ReturnType,
                              size_t)> callback);
};

// A `BackgroundTask` to certify a signing key using an attestation key.
class CertifyTask : public internal::BackgroundTaskImpl<
                        ServiceErrorOr<crypto::AttestationStatement>> {
 public:
  CertifyTask(
      scoped_refptr<RefCountedUnexportableAttestationKey> attestation_key,
      scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
      base::span<const uint8_t> challenge,
      BackgroundTaskPriority priority,
      size_t max_retries,
      base::OnceCallback<void(CertifyTask::ReturnType, size_t)> callback);

 protected:
  bool ShouldRetryBasedOnResult(
      const ServiceErrorOr<crypto::AttestationStatement>& result)
      const override;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
