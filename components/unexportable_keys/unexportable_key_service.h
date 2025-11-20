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

  // Returns all signing keys currently stored by the OS that are have been
  // managed by this service.
  //
  // Invokes `callback` with a `ServiceError` if an error occurs during
  // retrieval, or the list of stored keys otherwise.
  //
  // Note: The intended use case for this method is garbage collecting obsolete
  // keys. That is, clients are expected to call this method, and then compare
  // the returned keys to the ones present in the client's storage. Keys that
  // are returned but no longer recognized by the client should be passed to
  // `DeleteKeySlowlyAsync()` soon after.
  //
  // Note: This method is meaningless on platforms that do not support storing
  // keys in the OS, and will return an empty list on those platforms. Here
  // clients are not expected to perform garbage collection.
  //
  // Example usage:
  //
  // void OnKeys(ServiceErrorOr<std::vector<UnexportableKeyId>> maybe_keys) {
  //   if (!maybe_keys.has_value()) {
  //     // Handle error.
  //     return;
  //   }
  //
  //   UnexportableKeyService& service = GetUnexportableKeyService();
  //   for (const auto& key : *maybe_keys) {
  //     if (!MyCodeRecognizesThisKey(key)) {
  //       // Perform garbage collection.
  //       service.DeleteKeySlowlyAsync(key, ...);
  //     }
  //   }
  // }
  //
  //
  // UnexportableKeyService& service = GetUnexportableKeyService();
  // service.GetAllSigningKeysForGarbageCollectionSlowlyAsync(
  //     kPriority,
  //     base::BindOnce(OnKeys));
  virtual void GetAllSigningKeysForGarbageCollectionSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<UnexportableKeyId>>)>
          callback) = 0;

// Copies a key from another service.
  //
  // Invokes `callback` with a `ServiceError` if `key_id_from_other_service` is
  // not found. Otherwise, returns a new key ID that can be used to refer to the
  // same key.
  virtual void CopyKeyFromOtherService(
      const UnexportableKeyService& other_service,
      UnexportableKeyId key_id_from_other_service,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback) = 0;

  // Schedules a new asynchronous signing task.
  // Might return a cached result if a task with the same combination of
  // `signing_key` and `data` has been completed recently.
  // Invokes `callback` with a signature of `data`, or a `ServiceError` if
  // `key_id` is not found or an error occurs during signing.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`.
  virtual void SignSlowlyAsync(
      UnexportableKeyId key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)>
          callback) = 0;

  // Deletes a key.
  //
  // Invokes `callback` with a `ServiceError` inline if `key_id` is not found.
  // Otherwise, removes the key from the in-memory cache synchronously, and
  // schedules an asynchronous deletion task.  This will invoke `callback` with
  // a `ServiceError` if an error occurs during deletion and `base::ok()`
  // otherwise.
  //
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`.
  //
  // Assuming `key_id` was found, it is invalidated immediately and should not
  // be used again.
  //
  // Note: On platforms like macOS this will delete the key from the OS, and
  // thus future calls to `FromWrappedSigningKeySlowlyAsync()` with the same
  // wrapped key will fail.
  virtual void DeleteKeySlowlyAsync(
      UnexportableKeyId key_id,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<void>)> callback) = 0;

  // Deletes all keys.
  //
  // This will remove all keys from the in-memory cache synchronously, reply
  // `kKeyNotFound` to pending `FromWrappedSigningKeySlowlyAsync()` requests,
  // and schedule an asynchronous deletion task. This will invoke `callback`
  // with a `ServiceError` if an error occurs during deletion and the number of
  // deleted keys otherwise. Pending `GenerateSigningKeySlowlyAsync()` requests
  // are not affected.
  //
  // Note: On platforms like macOS this will delete all keys from the OS, and
  // thus future calls to `FromWrappedSigningKeySlowlyAsync()` will fail.
  virtual void DeleteAllKeysSlowlyAsync(
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<size_t>)> callback) = 0;

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
