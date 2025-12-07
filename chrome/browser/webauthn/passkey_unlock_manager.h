// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"

class Browser;
class Profile;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace webauthn {

// This class manages the unlock state for Google Password Manager (GPM)
// passkeys. It asynchronously determines if passkeys are locked, but can be
// unlocked. Once the final state is known, it notifies observers.
class PasskeyUnlockManager : public KeyedService,
                             public PasskeyModel::Observer,
                             public EnclaveManager::Observer,
                             public syncer::SyncServiceObserver {
 public:
  // LINT.IfChange
  //
  // Represents the type of event related to the passkey unlock error UI, such
  // as displaying, hiding or interacting with the UI.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  enum class ErrorUIEventType {
    kAvatarUIDisplayed = 0,
    kAvatarUIHidden = 1,
    kAvatarButtonPressed = 2,
    kMaxValue = kAvatarButtonPressed,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

  class Observer : public base::CheckedObserver {
   public:
    // Notifies the observer that state has changed.
    // TODO(crbug.com/461806010): Rename this method. The more suitable name for
    // this method would be something like `OnPasskeyErrorUiStateChanged()`.
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
  // Returns the value cached in `should_display_error_ui_`.
  virtual bool ShouldDisplayErrorUi() const;

  // Opens a browser tab with a challenge for unlocking passkeys.
  static void OpenTabWithPasskeyUnlockChallenge(Browser* browser);

  // Methods providing the UI strings. Results depend on the experiment arms
  // configured by the feature parameter `kPasskeyUnlockErrorUiExperimentArm`.
  std::u16string GetPasskeyErrorProfilePillTitle() const;
  std::u16string GetPasskeyErrorProfileMenuDetails() const;
  std::u16string GetPasskeyErrorProfileMenuButtonLabel() const;

  // Returns true if the passkey unlock error UI is enabled, depending on the
  // feature flags.
  static bool IsPasskeyUnlockErrorUiEnabled();

  // Used in tests to notify observers.
  void NotifyObserversForTesting();

  static void RecordErrorUIEventType(ErrorUIEventType event_type);

 private:
  // Returns the PasskeyModel associated with the profile passed to the
  // constructor.
  PasskeyModel* passkey_model();

  // Returns the EnclaveManager associated with the profile passed to the
  // constructor.
  EnclaveManagerInterface* enclave_manager();

  // Returns the SyncService associated with the profile passed to the
  // constructor.
  syncer::SyncService* sync_service();

  // Updates the cached value of `has_passkeys_`.
  void UpdateHasPasskeys();

  // Updates the cached value of `sync_active_`. Checks the sync state and
  // user actionable errors.
  void UpdateSyncState();

  // Recomputes `should_display_error_ui_` and notifies observers if its value
  // changed.
  void ComputeShouldDisplayErrorUiAndNotifyObservers();

  // Used for notifying observers.
  void NotifyObservers();

  // Caches `has_gpm_pin_`.
  void AsynchronouslyCheckGpmPinAvailability();
  void OnHaveGpmPinAvailability(
      EnclaveManager::GpmPinAvailability gpm_pin_availability);
  // Caches `has_system_uv_`.
  void AsynchronouslyCheckSystemUVAvailability();
  // Callback for `AsynchronouslyCheckSystemUVAvailability`.
  void OnHaveSystemUVAvailability(bool has_system_uv);

  // A helper for `ComputeShouldDisplayErrorUi()`. Checks whether passkeys are
  // in the locked state. If this information can't be computed returns
  // `std::nullopt`.
  std::optional<bool> ArePasskeysLocked() const;
  // A helper for `ComputeShouldDisplayErrorUi()`. Checks whether passkeys could
  // be unlocked. If this information can't be computed returns `std::nullopt`.
  std::optional<bool> ArePasskeysUnlockable() const;

  void Shutdown() override;

  void OnEnclaveManagerLoaded();

  // EnclaveManager::Observer
  void OnStateUpdated() override;

  // webauthn::PasskeyModel::Observer
  // After getting notified - update the cached value of `has_passkeys_`
  void OnPasskeysChanged(
      const std::vector<PasskeyModelChange>& changes) override;
  void OnPasskeyModelShuttingDown() override;
  void OnPasskeyModelIsReady(bool is_ready) override;

  // syncer::SyncServiceObserver overrides:
  void OnStateChanged(syncer::SyncService* sync) override;
  void OnSyncShutdown(syncer::SyncService* sync) override;

  // Schedules recording the `WebAuthentication.PasskeyCount` histogram if it
  // hasn't been recorded yet.
  void MaybeRecordDelayedPasskeyCountHistogram();
  // Records the `WebAuthentication.PasskeyCount` histogram.
  void RecordPasskeyCountHistogram();
  // Schedules recording the `WebAuthentication.PasskeyReadiness` histogram.
  void MaybeRecordDelayedPasskeyReadinessHistogram();
  // Records the `WebAuthentication.PasskeyReadiness` histogram.
  void RecordPasskeyReadinessHistogram();

  std::optional<bool> has_passkeys_;
  std::optional<bool> enclave_ready_;
  std::optional<bool> has_gpm_pin_;
  std::optional<bool> has_system_uv_;
  bool sync_active_ = false;
  bool should_display_error_ui_ = false;

  base::ObserverList<Observer> observer_list_;

  // Used for UMA to determine whether `WebAuthentication.PasskeyCount`
  // histogram needs to recorded. Set to true iff histogram was already
  // recorded.
  bool passkey_count_recorded_on_startup_ = false;
  // Used for UMA to determine whether `WebAuthentication.PasskeyReadiness`
  // histogram needs to recorded. Set to true iff histogram was already
  // recorded.
  bool passkey_readiness_recorded_on_startup_ = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::ScopedObservation<EnclaveManagerInterface, EnclaveManager::Observer>
      enclave_manager_observation_{this};
  base::ScopedObservation<PasskeyModel, PasskeyModel::Observer>
      passkey_model_observation_{this};
  base::ScopedObservation<syncer::SyncService, syncer::SyncServiceObserver>
      sync_service_observation_{this};
  base::WeakPtrFactory<PasskeyUnlockManager> weak_ptr_factory_{this};

 protected:
  PasskeyUnlockManager();
};

}  // namespace webauthn

#endif  // CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_
