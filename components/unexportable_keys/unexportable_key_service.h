// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_

#include <algorithm>
#include <map>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"

namespace unexportable_keys {

namespace {
class MaybePendingUnexportableKeyId;
}

class UnexportableKeyTaskManager;

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
class UnexportableKeyService : public KeyedService {
 public:
  // `task_manager` must outlive `UnexportableKeyService`.
  explicit UnexportableKeyService(UnexportableKeyTaskManager& task_manager);

  UnexportableKeyService(const UnexportableKeyService&) = delete;
  UnexportableKeyService& operator=(const UnexportableKeyService&) = delete;

  ~UnexportableKeyService() override;

  // Generates a new signing key asynchronously and returns an ID of this key.
  // Returned `UnexportableKeyId` can be used later to perform key operations on
  // this `UnexportableKeyService`.
  // The first supported value of `acceptable_algorithms` determines the type of
  // the key.
  // Invokes `callback` with a `ServiceError` if no supported hardware exists,
  // if no value in `acceptable_algorithms` is supported, or if there was an
  // error creating the key.
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback);

  // Creates a new signing key from a `wrapped_key` asynchronously and returns
  // an ID of this key.
  // Returned `UnexportableKeyId` can be used later to perform key operations on
  // this `UnexportableKeyService`.
  // `wrapped_key` can be read from disk but must have initially resulted from
  // calling `GetWrappedKey()` on a previous instance of `UnexportableKeyId`.
  // Invokes `callback` with a `ServiceError` if `wrapped_key` cannot be
  // imported.
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback);

  // Schedules a new asynchronous signing task.
  // Might return a cached result if a task with the same combination of
  // `signing_key` and `data` has been completed recently.
  // Invokes `callback` with a signature of `data`, or a `ServiceError` if
  // `key_id` is/ not found or an error occurs during signing.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  void SignSlowlyAsync(
      const UnexportableKeyId& key_id,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(ServiceErrorOr<std::vector<uint8_t>>)> callback);

  // Returns an SPKI that contains the public key of a key that `key_id` refers
  // to.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  ServiceErrorOr<std::vector<uint8_t>> GetSubjectPublicKeyInfo(
      UnexportableKeyId key_id) const;

  // Returns the encrypted private key of a key that `key_id` refers to. It is
  // encrypted to a key that is kept in hardware and the unencrypted private key
  // never exists in the CPU's memory.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  ServiceErrorOr<std::vector<uint8_t>> GetWrappedKey(
      UnexportableKeyId key_id) const;

  // Returns the algorithm of a key that `key_id` refers to.
  // Returns a `ServiceError` if `key_id` is not found.
  // `key_id` must have resulted from calling `GenerateSigningKeySlowlyAsync()`
  // or `FromWrappedSigningKeySlowlyAsync()`
  ServiceErrorOr<crypto::SignatureVerifier::SignatureAlgorithm> GetAlgorithm(
      UnexportableKeyId key_id) const;

 private:
  // Comparator object that allows comparing containers of different types that
  // are convertible to base::span<const uint8_t>.
  struct WrappedKeyCmp {
    using is_transparent = void;
    bool operator()(base::span<const uint8_t> lhs,
                    base::span<const uint8_t> rhs) const {
      return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(),
                                          rhs.end());
    }
  };

  using WrappedKeyMap = std::
      map<std::vector<uint8_t>, MaybePendingUnexportableKeyId, WrappedKeyCmp>;
  using KeyIdMap = std::map<UnexportableKeyId,
                            scoped_refptr<RefCountedUnexportableSigningKey>>;

  // Callback for `GenerateSigningKeySlowlyAsync()`.
  void OnKeyGenerated(
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>
          client_callback,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  // Callback for `FromWrappedSigningKeySlowlyAsync()`.
  void OnKeyCreatedFromWrappedKey(
      WrappedKeyMap::iterator pending_entry_it,
      ServiceErrorOr<scoped_refptr<RefCountedUnexportableSigningKey>>
          key_or_error);

  const raw_ref<UnexportableKeyTaskManager> task_manager_;

  // Helps mapping multiple `FromWrappedSigningKeySlowlyAsync()` requests with
  // the same wrapped key into the same key ID.
  WrappedKeyMap key_id_by_wrapped_key_;

  // Stores unexportable signing keys that were created during the current
  // session.
  KeyIdMap key_by_key_id_;

  base::WeakPtrFactory<UnexportableKeyService> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_SERVICE_H_
