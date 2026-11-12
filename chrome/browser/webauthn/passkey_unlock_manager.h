// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_
#define CHROME_BROWSER_WEBAUTHN_PASSKEY_UNLOCK_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "chrome/browser/trusted_vault/trusted_vault_encryption_keys_tab_helper.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_interface.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/service/sync_service_observer.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "components/webauthn/core/browser/passkey_model.h"
#include "components/webauthn/core/browser/passkey_model_change.h"
#include "google_apis/gaia/gaia_id.h"

class BrowserWindowInterface;

namespace syncer {
class SyncService;
}  // namespace syncer

namespace webauthn {

// Manages the unlock state for Google Password Manager (GPM) passkeys.
// Observers and startup checks update cached inputs, while a linear `Stage`
// state machine waits for `EnclaveManager` to load, downloads the security
// domain state once to check GPM PIN availability, and on macOS attempts
// opportunistic iCloud Keychain recovery before computing the error UI and
// reporting locked passkeys to Sync in `Stage::kReady`.
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
    // Notifies the observer that the passkey error UI state has changed.
    virtual void OnPasskeyErrorUiStateChanged() = 0;

    // Notifies the observer that the passkey unlock manager is shutting down.
    virtual void OnPasskeyUnlockManagerShuttingDown() = 0;

    // Notifies the observer when the passkey unlock manager becomes ready.
    virtual void OnPasskeyUnlockManagerIsReady() = 0;
  };

  PasskeyUnlockManager(EnclaveManagerInterface* enclave_manager,
                       PasskeyModel* passkey_model,
                       syncer::SyncService* sync_service);
  ~PasskeyUnlockManager() override;
  PasskeyUnlockManager(const PasskeyUnlockManager&) = delete;
  PasskeyUnlockManager(const PasskeyUnlockManager&&) = delete;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Synchronously tells whether the passkey error UI should be displayed.
  // Returns the value cached in `should_display_error_ui_`.
  virtual bool ShouldDisplayErrorUi() const;

  // Opens a browser tab with a challenge for unlocking passkeys.
  static void OpenTabWithPasskeyUnlockChallenge(
      BrowserWindowInterface* browser,
      trusted_vault::TrustedVaultUserActionTriggerForUMA trigger);

  // Returns true if the passkey unlock error UI is enabled, depending on the
  // feature flags.
  static bool IsPasskeyUnlockErrorUiEnabled();

  // Used in tests to notify observers.
  void NotifyObserversForTesting();

  static void RecordErrorUIEventType(ErrorUIEventType event_type);

 private:
  using AccountState =
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult;

  // Linear startup stages. The error UI and Sync's passkeys-locked state are
  // only computed and reported once `kReady` is reached.
  enum class Stage {
    // Waiting for `EnclaveManager` to finish loading (at construction, via an
    // early load by another caller, or via `LoadAfterDelay()`).
    kWaitingForEnclaveLoad,
    // Downloading the security domain state once; GPM PIN availability is
    // checked when it arrives.
    kDownloadingAccountState,
#if BUILDFLAG(IS_MAC)
    // Waiting for passkeys to become locked so iCloud Keychain recovery can run
    // once; the error UI and Sync's passkeys-locked state remain false.
    kAwaitingICloudRecovery,
    // Opportunistic iCloud Keychain recovery is in flight.
    kRecoveringWithICloudKeychain,
#endif  // BUILDFLAG(IS_MAC)
    // Steady state; computes `should_display_error_ui_` and
    // `passkeys_locked_for_sync_` and notifies observers and Sync.
    kReady,
  };

  // Returns the PasskeyModel provided at construction.
  PasskeyModel* passkey_model();

  // Returns the EnclaveManager provided at construction.
  EnclaveManagerInterface* enclave_manager();

  // Returns the SyncService provided at construction.
  syncer::SyncService* sync_service();

  // Updates the cached value of `has_passkeys_`.
  void UpdateHasPasskeys();

  // Updates the cached values of `syncing_webauthn_enabled_` and
  // `no_sync_errors_`.
  void UpdateSyncState();

  // Advances `stage_` through the startup sequence (`kWaitingForEnclaveLoad`
  // -> `kDownloadingAccountState` -> macOS iCloud Keychain recovery ->
  // `kReady`), or recomputes the error UI and Sync state once in `kReady`.
  void ActAccordingToPasskeyUnlockState();

  // Recomputes `should_display_error_ui_` and `passkeys_locked_for_sync_`, and
  // notifies observers / Sync if their values changed. Only called in
  // `Stage::kReady`.
  void ComputeShouldDisplayErrorUiAndNotifyObservers();

  // Used for notifying observers.
  void NotifyObservers();

  // Callback for `EnclaveManagerInterface::LoadAfterDelay()`. Advances startup
  // if `EnclaveManager` was not already loaded earlier.
  void OnEnclaveManagerLoaded();
  // Enters `Stage::kDownloadingAccountState` and starts the single security
  // domain state download.
  void StartAccountStateDownload();
  // Caches `has_gpm_pin_`, schedules `WebAuthentication.GpmPinStatus`, and
  // advances to iCloud Keychain recovery (on macOS when enabled) or
  // `Stage::kReady`.
  void OnAccountStateDownloaded(AccountState account_state,
                                GaiaId account_state_gaia_id);
  // Caches `has_system_uv_`.
  void AsynchronouslyCheckSystemUVAvailability();
  // Callback for `AsynchronouslyCheckSystemUVAvailability()`.
  void OnHaveSystemUVAvailability(bool has_system_uv);
#if BUILDFLAG(IS_MAC)
  // Enters `Stage::kRecoveringWithICloudKeychain` and starts opportunistic
  // iCloud Keychain recovery. `account_state` and `account_state_gaia_id` are
  // provided when recovery starts directly from `OnAccountStateDownloaded()`
  // after a successful download, and `std::nullopt` otherwise so
  // `EnclaveManager` downloads fresh state.
  void StartICloudRecovery(std::optional<AccountState> account_state,
                           std::optional<GaiaId> account_state_gaia_id);
  // Updates `enclave_ready_`, enters `Stage::kReady`, and recomputes the error
  // UI and Sync state.
  void OnICloudRecoveryComplete();
#endif  // BUILDFLAG(IS_MAC)

  // Checks whether passkeys are in the locked state; when
  // `kPasskeyUnlockIdentityErrorUi` is disabled, also requires Sync to have no
  // actionable error (`no_sync_errors_`). Returns `std::nullopt` if this
  // information can't be computed yet.
  std::optional<bool> ArePasskeysLocked() const;
  // A helper for `ComputeShouldDisplayErrorUiAndNotifyObservers()`. Checks
  // whether passkeys could be unlocked. If this information can't be computed
  // returns `std::nullopt`.
  std::optional<bool> ArePasskeysUnlockable() const;

  void Shutdown() override;

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

  // TODO(crbug.com/540854648): Decouple the metrics publishing logic.
  // Schedules recording the `WebAuthentication.PasskeyCount` histogram
  // if it hasn't been recorded yet.
  void MaybeRecordDelayedPasskeyCountHistogram();
  // Records the `WebAuthentication.PasskeyCount` histogram.
  void RecordPasskeyCountHistogram();
  // TODO(crbug.com/540854648): Decouple the metrics publishing logic.
  // Schedules recording the `WebAuthentication.PasskeyReadiness` histogram.
  void MaybeRecordDelayedPasskeyReadinessHistogram();
  // Records the `WebAuthentication.PasskeyReadiness` histogram.
  void RecordPasskeyReadinessHistogram();
  // TODO(crbug.com/540854648): Decouple the metrics publishing logic.
  // Schedules recording the `WebAuthentication.GpmPinStatus` histogram.
  void MaybeRecordDelayedGpmPinStatusHistogram(
      EnclaveManager::GpmPinAvailability gpm_pin_availability);
  // Records the `WebAuthentication.GpmPinStatus` histogram.
  void RecordGpmPinStatusHistogram(
      EnclaveManager::GpmPinAvailability gpm_pin_availability);
  // TODO(crbug.com/540854648): Decouple the metrics publishing logic.
  // Schedules recording the `PasswordManager.TrustedVaultPasswordReadiness`
  // histogram.
  void MaybeRecordDelayedPasswordReadinessHistogram();
  // Records the `PasswordManager.TrustedVaultPasswordReadiness` histogram.
  void RecordPasswordReadinessHistogram(bool password_readiness);

  Stage stage_ = Stage::kWaitingForEnclaveLoad;
  std::optional<bool> has_passkeys_;
  std::optional<bool> enclave_ready_;
  std::optional<bool> has_gpm_pin_;
  std::optional<bool> has_system_uv_;
  bool syncing_webauthn_enabled_ = false;
  bool no_sync_errors_ = false;
  bool should_display_error_ui_ = false;
  bool passkeys_locked_for_sync_ = false;

  base::ObserverList<Observer> observer_list_;

  // Used for UMA to determine whether `WebAuthentication.PasskeyCount`
  // histogram needs to recorded. Set to true iff histogram was already
  // recorded.
  bool passkey_count_recorded_on_startup_ = false;
  // Used for UMA to determine whether `WebAuthentication.PasskeyReadiness`
  // histogram needs to recorded. Set to true iff histogram was already
  // recorded.
  bool passkey_readiness_recorded_on_startup_ = false;
  // Used for UMA to determine whether `WebAuthentication.GpmPinStatus`
  // histogram needs to be recorded. Set to true iff histogram was already
  // recorded.
  bool gpm_pin_status_recorded_on_startup_ = false;
  // Used for UMA to determine whether
  // `PasswordManager.TrustedVaultPasswordReadiness` histogram needs to be
  // recorded. Set to true iff histogram was already recorded.
  bool password_readiness_recorded_on_startup_ = false;

  std::unique_ptr<trusted_vault::TrustedVaultConnection::Request>
      download_account_state_request_;

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
