// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_LOADER_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_LOADER_H_

#include <memory>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"

namespace unexportable_keys {

class UnexportableKeyService;

// This class facilitates creation of `UnexportableKeyId` and allows scheduling
// callbacks to be called once a key is loaded.
//
// This class is designed for a single use: it allows loading only one key.
// Create multiple instances of this class to load multiple keys.
class COMPONENT_EXPORT(UNEXPORTABLE_KEYS) UnexportableKeyLoader {
 public:
  enum class State {
    // A key hasn't been requested yet by this class.
    kNotStarted,
    // A key is being loaded either by creating it from a wrapped key or by
    // generating a brand new key.
    kLoading,
    // Terminal state of the loader. Either a key has been loaded successfully
    // or a key load terminated with an error.
    kReady
  };

  // Creates a new loader for a key that has previously been serialized into a
  // `wrapped_key`.
  static std::unique_ptr<UnexportableKeyLoader> CreateFromWrappedKey(
      UnexportableKeyService& unexportable_key_service,
      base::span<const uint8_t> wrapped_key,
      BackgroundTaskPriority priority);

  // Creates a new loader that will generate a brand new key.
  static std::unique_ptr<UnexportableKeyLoader> CreateWithNewKey(
      UnexportableKeyService& unexportable_key_service,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority);

  UnexportableKeyLoader(const UnexportableKeyLoader&) = delete;
  UnexportableKeyLoader& operator=(const UnexportableKeyLoader&) = delete;

  ~UnexportableKeyLoader();

  // Registers `callback` to be called when a key is loaded. Invokes `callback`
  // immediately if a key has already been loaded.
  void InvokeCallbackAfterKeyLoaded(
      base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)> callback);

  // If a key hasn't been loaded yet, returns ServiceError::kKeyNotReady.
  // Otherwise, returns a loaded key ID or a terminal error state.
  ServiceErrorOr<UnexportableKeyId> GetKeyIdOrError();

  // Returns the current state of the loader.
  // Public for testing.
  State GetStateForTesting();

 private:
  // Use one of the Create* static methods to create an object of this class.
  UnexportableKeyLoader();

  void LoadFromWrappedKey(UnexportableKeyService& unexportable_key_service,
                          base::span<const uint8_t> wrapped_key,
                          BackgroundTaskPriority priority);
  void GenerateNewKey(
      UnexportableKeyService& unexportable_key_service,
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority);

  void OnKeyLoaded(ServiceErrorOr<UnexportableKeyId> key_id_or_error);

  ServiceErrorOr<UnexportableKeyId> key_id_or_error_ =
      base::unexpected(ServiceError::kKeyNotReady);
  State state_ = State::kNotStarted;
  std::vector<base::OnceCallback<void(ServiceErrorOr<UnexportableKeyId>)>>
      on_load_callbacks_;

  base::WeakPtrFactory<UnexportableKeyLoader> weak_ptr_factory_{this};
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_LOADER_H_
