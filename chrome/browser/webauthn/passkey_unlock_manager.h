// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

class Browser;
class Profile;

namespace webauthn {

// This class manages the unlock state for Google Password Manager (GPM)
// passkeys. It asynchronously determines if passkeys are locked, but can be
// unlocked. Once the final state is known, it notifies observers.
class PasskeyUnlockManager : public KeyedService,
                             public PasskeyModel::Observer {
 public:
  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that state has changed.
    virtual void OnPasskeyUnlockManagerStateChanged() = 0;

    // Notifies the observer that the passkey unlock manager is shutting down.
    virtual void OnPasskeyUnlockManagerShuttingDown() = 0;

    // Notifies the observer when the passkey unlock manager becomes ready.
    virtual void OnPasskeyUnlockManagerIsReady() = 0;
  };

  explicit PasskeyUnlockManager(Profile* profile);
  ~PasskeyUnlockManager() override;
  PasskeyUnlockManager(const PasskeyUnlockManager&) = delete;
  PasskeyUnlockManager(const PasskeyUnlockManager&&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Synchronously tells whether the passkey error UI should be displayed.
  bool ShouldDisplayErrorUi();

  // Opens a browser tab with a challenge for unlocking passkeys.
  static void OpenTabWithPasskeyUnlockChallenge(Browser* browser);

 private:
  // Returns the PasskeyModel associated with the profile passed to the
  // constructor.
  PasskeyModel* passkey_model();

  // Updates the cached value of `has_passkeys_`.
  void UpdateHasPasskeys();

  // Used for notifying observers.
  void NotifyObservers();

  // Caches `has_gpm_pin_`.
  void AsynchronouslyCheckGpmPinAvailability();
  // Caches `has_system_uv_`.
  void AsynchronouslyCheckSystemUVAvailability();
  // Caches `enclave_ready_`.
  void AsynchronouslyLoadEnclaveManager();

  // TODO(crbug.com/449949272): Implement EnclaveManager::Observer.

  // webauthn::PasskeyModel::Observer
  // After getting notified - update the cached value of `has_passkeys_`
  void OnPasskeysChanged(
      const std::vector<webauthn::PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // TODO(crbug.com/449950177): Implement syncer::SyncServiceObserver.

  std::optional<bool> has_passkeys_;
  std::optional<bool> enclave_ready_;
  std::optional<bool> has_gpm_pin_;
  std::optional<bool> has_system_uv_;

  base::ObserverList<Observer> observer_list_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ScopedObservation<webauthn::PasskeyModel,
                          webauthn::PasskeyModel::Observer>
      passkey_model_observation_{this};
  base::WeakPtrFactory<PasskeyUnlockManager> weak_ptr_factory_{this};
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_
