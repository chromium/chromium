// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
#define CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_

#include <optional>

#include "base/functional/callback_forward.h"
#include "base/observer_list.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/gpm_pin_availability.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "google_apis/gaia/gaia_id.h"

class EnclaveManager;

class EnclaveManagerInterface : public KeyedService {
 public:
  // Many actions report results using a `Callback`. The boolean argument
  // is true if the operation is successful and false otherwise.
  // These callbacks never hairpin. (I.e. are never called before the function
  // that they were passed to returns.)
  using Callback = base::OnceCallback<void(bool)>;

  using GpmPinAvailability = webauthn::GpmPinAvailability;
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
  };

  class Observer : public base::CheckedObserver {
   public:
    // `OnKeysStored` is called when MagicArch provides keys to the
    // EnclaveManager by calling `StoreKeys`. `gaia_id` is the account
    // identifier for the keys that were stored. Clients should verify this
    // matches the account they expect.
    virtual void OnKeysStored(const GaiaId& gaia_id) {}

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

  // Returns true if the persistent state has been loaded and the current user
  // has been registered with the enclave.
  virtual bool IsRegistered() const = 0;

  // Loads the persistent state from disk if needed and invokes `callback` with
  // whether the current user is registered with the enclave.
  virtual void LoadAndCheckIsRegistered(Callback callback) = 0;

  // Returns true if the persistent state has been loaded from the disk. (Or
  // else the loading failed and an empty state is being used.)
  virtual bool IsLoaded() const = 0;

  // Returns true if the current user has joined the security domain and has one
  // or more wrapped security domain secrets available. (This implies
  // `IsRegistered`.)
  virtual bool IsReady() const = 0;

  // Send a request to the enclave to delete the registration for the current
  // user, erase local keys, and erase local state for the user. Safe to call in
  // any state and is a no-op if no registration exists.
  virtual void Unenroll(Callback callback) = 0;

  // Asynchronously checks if the current user has a GPM PIN. The caller must
  // keep the returned Request object alive until the callback is run.
  // Destroying the Request object will cancel the operation.
  [[nodiscard]] virtual std::unique_ptr<
      trusted_vault::TrustedVaultConnection::Request>
  CheckGpmPinAvailability(GpmPinAvailabilityCallback callback) = 0;

#if BUILDFLAG(IS_MAC)
  // Attempts to recover the security domain secret using local iCloud Keychain
  // recovery keys, storing the recovered key in `EnclaveManager` on success and
  // reporting whether recovery succeeded via `callback`. `account_state` is the
  // security domain state of the account with `account_state_gaia_id` if the
  // caller has already downloaded it (both or neither must be set). It is used
  // if `account_state_gaia_id` still matches the primary account when recovery
  // runs; otherwise a fresh state is downloaded.
  virtual void RecoverWithICloudKeychain(
      std::optional<
          trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult>
          account_state,
      std::optional<GaiaId> account_state_gaia_id,
      Callback callback) = 0;
#endif  // BUILDFLAG(IS_MAC)

  virtual void LoadAfterDelay(base::TimeDelta delay,
                              base::OnceClosure closure) = 0;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

#endif  // CHROME_BROWSER_WEBAUTHN_ENCLAVE_MANAGER_INTERFACE_H_
