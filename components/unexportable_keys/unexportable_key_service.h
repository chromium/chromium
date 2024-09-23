// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"

namespace unexportable_keys {

// Service providing access to `UnexportableSigningKey`s.
//
// The service doesn't give clients direct access to the keys. Instead,
// `UnexportableKeyService` returns a key handle, `UnexportableKeyId`, that can
// be passed back to the service to perform operations with the key.
//
// To use the same key across several sessions, a client should perform the
// following steps:
//
// 1. Generate a new `UnexportableSigningKey` and obtain its key ID:
//
//  UnexportableKeyService& service = GetUnexportableKeyService();
//  ServiceErrorOr<UnexportableKeyId> key_id;
//  service.GenerateSigningKeySlowlyAsync(
//      kAlgorithm, kPriority, [&key_id](auto result) { key_id = result; });
//
// 2. Get a wrapped key for this key and save it to disk:
//
//  std::vector<uint8_t> wrapped_key = service.GetWrappedKey(*key_id);
//  SaveToDisk(wrapped_key);
//
// 3. After the process restart, restore the same `UnexportableSigningKey` from
//    the wrapped key:
//
//  UnexportableKeyService& service = GetUnexportableKeyService();
//  ServiceErrorOr<UnexportableKeyId> key_id;
//  std::vector<uint8_t> wrapped_key = ReadFromDisk();
//  service.FromWrappedSigningKeySlowlyAsync(
//    wrapped_key, kPriority, [&key_id](auto result) { key_id = result; });
//
// 4. Use obtained key ID to sign data:
//
//  service.SignSlowlyAsync(*key_id, kData, kPriority, std::move(callback));
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) UnexportableKeyService {
 public:
  UnexportableKeyService() = default;

  UnexportableKeyService(const UnexportableKeyService&) = delete;
  UnexportableKeyService& operator=(const UnexportableKeyService&) = delete;

  virtual ~UnexportableKeyService() = default;

  // Generates a new signing key asynchronously and returns an ID of this key.
  // Returned `UnexportableKeyId` can be used later to perform key operations on
  // this `UnexportableKeyService`.
  // The first supported value of `acceptable_algorithms` determines the type of
  // the key.
  // Invokes `callback` with a `ServiceError` if no supported hardware exists,
  // if no value in `acceptable_algorithms` is supported, or if there was an
  // error creating the key.
  virtual void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) = 0;

  // Creates a new signing key from a `wrapped_key` asynchronously and returns
  // an ID of this key.
  // Returned `UnexportableKeyId` can be used later to perform key operations on
  // this `UnexportableKeyService`.
  // `wrapped_key` can be read from disk but must have initially resulted from
  // calling `GetWrappedKey()` on a previous instance of `UnexportableKeyId`.
  // Invokes `callback` with a `ServiceError` if `wrapped_key` cannot be
  // imported.
  virtual void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) = 0;

  // Schedules a new asynchronous signing task.
  // Might return a cached result if a task with the same combination of
  // `signing_key` and `data` has been completed recently.
  // Invokes `callback` with a signature of `data`, or a `ServiceError` if
  // `key_id` is/ not found or an error occurs during signing.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  virtual void SignSlowlyAsync(
      const UnexportableKeyId& key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)>
          callback) = 0;

  // Returns an SPKI that contains the public key of a key that `key_id` refers
  // to.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  virtual ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const = 0;

  // Returns the encrypted private key of a key that `key_id` refers to. It is
  // encrypted to a key that is kept in hardware and the unencrypted private key
  // never exists in the CPU's memory.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  virtual ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const = 0;

  // Returns the algorithm of a key that `key_id` refers to.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  virtual ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm>
  GetAlgorithm(UnexportableKeyId key_id) const = 0;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_
