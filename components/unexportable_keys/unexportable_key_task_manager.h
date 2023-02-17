// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASK_MANAGER_H_
#define COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASK_MANAGER_H_

#include <map>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "components/unexportable_keys/background_long_task_scheduler.h"
#include "components/unexportable_keys/background_task_priority.h"
#include "components/unexportable_keys/ref_counted_unexportable_signing_key.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/signature_verifier.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace unexportable_keys {

// `UnexportableKeyTaskManager` helps efficiently schedule tasks to generate
// and use signing keys backed by specialized hardware.
//
// Basically, it provides an asynchronous interface for all slow tasks exposed
// by `crypto::UnexportableSigningKey` and `crypto::UnexportableKeyProvider`.
// These tasks may take up to several seconds to execute so they never should be
// run on the main thread.
//
// `UnexportableKeyTaskManager` reserves the right to deduplicate calls to
// `SignSlowlyAsync()` and cache recent results of this operation in order to
// reduce the number of operations scheduled on hardware.
//
// WARNING: This might break the assumption about the signature being
// non-deterministic for some algorithms (like ECDSA). Let the OWNERS know if
// you want to disable this feature for your use case.
//
// Read documentation to `BackgroundLongTaskScheduler` for details on how the
// tasks are getting scheduled.
class UnexportableKeyTaskManager {
 public:
  UnexportableKeyTaskManager();
  ~UnexportableKeyTaskManager();

  UnexportableKeyTaskManager(const UnexportableKeyTaskManager&) = delete;
  UnexportableKeyTaskManager& operator=(const UnexportableKeyTaskManager&) =
      delete;

  // Generates a new signing key asynchronously.
  // The first supported value of `acceptable_algorithms` determines the type of
  // the key.
  // Invokes `callback` with nullptr if no supported hardware exists, if no
  // value in `acceptable_algorithms` is supported, or if there was an error
  // creating the key.
  void GenerateSigningKeySlowlyAsync(
      base::span<const crypto::SignatureVerifier::SignatureAlgorithm>
          acceptable_algorithms,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
          callback);

  // Creates a new signing key from a `wrapped_key` asynchronously.
  // `wrapped_key` must have resulted from calling `GetWrappedKey()` on a
  // previous instance of `crypto::UnexportableSigningKey`.
  // `key_id` is a unique identifier that will be attached to the signing key.
  // The caller is responsible for avoiding collisions and not requesting
  // several keys under the same id.
  // Invokes `callback` with nullptr if `wrapped_key` cannot be imported.
  void FromWrappedSigningKeySlowlyAsync(
      base::span<const uint8_t> wrapped_key,
      const UnexportableKeyId& key_id,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(scoped_refptr<RefCountedUnexportableSigningKey>)>
          callback);

  // Schedules a new signing task or appends `callback` to an existing
  // task with `signing_key` and `data` arguments. Might return a cached result
  // if a task with the same combination of `signing_key` and `data` has been
  // completed recently.
  // Invokes `callback` with a signature of `data`, of `absl::nullopt` if an
  // error occurs during signing.
  void SignSlowlyAsync(
      scoped_refptr<RefCountedUnexportableSigningKey> signing_key,
      base::span<const uint8_t> data,
      BackgroundTaskPriority priority,
      base::OnceCallback<void(absl::optional<std::vector<uint8_t>>)> callback);

 private:
  // Scheduler to run long tasks in background.
  BackgroundLongTaskScheduler task_scheduler_;
};

}  // namespace unexportable_keys

#endif  // COMPONENTS_UNEXPORTABLE_KEYS_UNEXPORTABLE_KEY_TASK_MANAGER_H_
