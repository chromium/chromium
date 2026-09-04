// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/background_task_impl.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "crypto/sign.h"
#include "crypto/unexportable_key.h"

namespace crypto {
class UnexportableKeyProvider;
}  // namespace crypto

namespace unexportable_keys {

class RefCountedUnexportableSigningKey;
class RefCountedUnexportableAttestationKey;

// A `BackgroundTask` to retrieve all `crypto::UnexportableSigningKey`s from the
// key provider.
class GetAllKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<
          std::vector<scoped_refptr<RefCountedUnexportableSigningKey>>>> {
 public:
  GetAllKeysTask(std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
                 BackgroundTaskPriority priority,
                 base::OnceCallback<void(ReturnType)> callback,
                 PreReplyCallback pre_reply);
};

// A `BackgroundTask` to generate a new `crypto::UnexportableSigningKey`.
class GenerateKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>> {
 public:
  GenerateKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
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
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
};

// A `BackgroundTask` to sign data with `crypto::UnexportableSigningKey` or
// `crypto::UnexportableAttestationKey`.
class SignTask : public internal::BackgroundTaskImpl<
                     ServiceErrorOr<std::vector<uint8_t>>> {
 public:
  SignTask(scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
           base::span<const uint8_t> data,
           BackgroundTaskPriority priority,
           BackgroundTaskType type,
           size_t max_retries,
           base::OnceCallback<void(ReturnType)> callback,
           PreReplyCallback pre_reply);

 protected:
  bool ShouldRetryBasedOnResult(
      const ServiceErrorOr<std::vector<uint8_t>>& result) const override;
};

// A `BackgroundTask` to delete a collection of
// `crypto::UnexportableSigningKey`.
class DeleteKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<size_t>> {
 public:
  DeleteKeysTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      std::vector<scoped_refptr<RefCountedUnexportableSigningKey>> keys,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
};

// A `BackgroundTask` to delete all `crypto::UnexportableSigningKey`s matching
// the key provider config.
class DeleteAllKeysTask
    : public internal::BackgroundTaskImpl<ServiceErrorOr<size_t>> {
 public:
  DeleteAllKeysTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
};

// A `BackgroundTask` to generate a new `crypto::UnexportableAttestationKey`.
class GenerateAttestationKeyTask
    : public internal::BackgroundTaskImpl<
          ServiceErrorOr<scoped_refptr<RefCountedUnexportableAttestationKey>>> {
 public:
  GenerateAttestationKeyTask(
      std::unique_ptr<crypto::UnexportableKeyProvider> key_provider,
      base::span<const crypto::sign::SignatureKind> acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
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
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);
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
      base::OnceCallback<void(ReturnType)> callback,
      PreReplyCallback pre_reply);

 protected:
  bool ShouldRetryBasedOnResult(
      const ServiceErrorOr<crypto::AttestationStatement>& result)
      const override;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASKS_H_
