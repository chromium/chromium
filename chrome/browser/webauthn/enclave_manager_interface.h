// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "components/keyed_service/core/keyed_service.h"

class EnclaveManager;

class EnclaveManagerInterface : public KeyedService {
 public:
  // Many actions report results using a `Callback`. The boolean argument
  // is true if the operation is successful and false otherwise.
  // These callbacks never hairpin. (I.e. are never called before the function
  // that they were passed to returns.)
  using Callback = base::OnceCallback<void(bool)>;

  // An enum that expresses whether a GPM PIN is set on an account.
  enum class GpmPinAvailability {
    // The PIN is set but not usable (there are 0 remaining attempts for
    // entering the pin).
    kGpmPinSetButNotUsable,
    // The PIN is set and usable (there are > 0 remaining attempts for entering
    // the pin).
    kGpmPinSetAndUsable,
    // The PIN is unset.
    kGpmPinUnset,
  };

  using GpmPinAvailabilityCallback =
      base::OnceCallback<void(GpmPinAvailability)>;

  // An enum that expresses the outcome of the operation of storing the
  // opportunistically retrieved keys.
  enum class OutOfContextRecoveryOutcome {
    // The key has been successfully stored and the device has been added to
    // account.
    kStoreKeysFromOpportunisticFlowSucceeded,
    // There was a failure while adding the device to account.
    kStoreKeysFromOpportunisticFlowFailed,
    // The key has been ignored because the device has been already registered
    // with the enclave.
    kStoreKeysFromOpportunisticFlowIgnoredRedundant,
    // The key has been ignored because neither system UV nor GPM PIN is
    // available.
    kStoreKeysFromOpportunisticFlowIgnoredNoUV,
  };

  class Observer : public base::CheckedObserver {
   public:
    // OnKeyStores is called when MagicArch provides keys to the EnclaveManager
    // by calling `StoreKeys`.
    virtual void OnKeysStored() {}

    // `OnStateUpdated` is called from `EnclaveManager::Stopped()` - indicating
    // that the state machine reached its final state (so the state of the
    // enclave manager might be updated now, e.g. it might become ready).
    virtual void OnStateUpdated() {}

    // `OnOutOfContextRecoveryCompletion` informs observers about the outcome of
    // the operation of storing the opportunistically retrieved keys.
    virtual void OnOutOfContextRecoveryCompletion(
        OutOfContextRecoveryOutcome outcome) {}
  };

  EnclaveManagerInterface() = default;
  EnclaveManagerInterface(const EnclaveManagerInterface&) = delete;
  EnclaveManagerInterface& operator=(const EnclaveManagerInterface&) = delete;
  ~EnclaveManagerInterface() override = default;

  // Return the full `EnclaveManager` interface. This will crash the address
  // space if run on an `EnclaveManagerInterface` instance that is not backed
  // by a real `EnclaveManager`, i.e. when it's a mock.
  virtual EnclaveManager* GetEnclaveManager();

  // Returns true if the current user has been registered with the enclave.
  // TODO(crbug.com/462438488): Change the name to the PascalCase naming
  // convention because this function is virtual.
  virtual bool is_registered() const = 0;

  // Returns true if the persistent state has been loaded from the disk. (Or
  // else the loading failed and an empty state is being used.)
  // TODO(crbug.com/462438488): Change the name to the PascalCase naming
  // convention because this function is virtual.
  virtual bool is_loaded() const = 0;

  // Returns true if the current user has joined the security domain and has one
  // or more wrapped security domain secrets available. (This implies
  // `is_registered`.)
  // TODO(crbug.com/462438488): Change the name to the PascalCase naming
  // convention because this function is virtual.
  virtual bool is_ready() const = 0;

  // Send a request to the enclave to delete the registration for the current
  // user, erase local keys, and erase local state for the user. Safe to call in
  // any state and is a no-op if no registration exists.
  virtual void Unenroll(Callback callback) = 0;

  virtual void CheckGpmPinAvailability(GpmPinAvailabilityCallback callback) = 0;

  virtual void LoadAfterDelay(base::TimeDelta delay,
                              base::OnceClosure closure) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
