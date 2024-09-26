// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_

#include "base/scoped_observation.h"
#include "base/timer/timer.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/safety_hub/safety_hub_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check_service_interface.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"

class PasswordStatusCheckResult;

namespace password_manager {
class BulkLeakCheckServiceAdapter;
}
// The service that runs password checks periodically to get unsecure credential
// information. The service also observes the changes in password store to get
// notified about the newly discovered password issues.
class PasswordStatusCheckService
    : public KeyedService,
      public password_manager::BulkLeakCheckServiceInterface::Observer,
      public password_manager::PasswordStoreInterface::Observer,
      public password_manager::InsecureCredentialsManager::Observer {
 public:
  explicit PasswordStatusCheckService(Profile* profile);

  PasswordStatusCheckService(const PasswordStatusCheckService&) = delete;
  PasswordStatusCheckService& operator=(const PasswordStatusCheckService&) =
      delete;

  ~PasswordStatusCheckService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Getters for different issues with credentials.
  size_t compromised_credential_count() const {
    return compromised_credential_count_;
  }
  size_t weak_credential_count() const { return weak_credential_count_; }
  size_t reused_credential_count() const { return reused_credential_count_; }

  bool is_password_check_running() const;

  bool no_passwords_saved() const { return saved_credential_count_ == 0; }

  // Returns the time at which the password check is currently scheduled to run.
  base::Time GetScheduledPasswordCheckTime() const;

  // Returns the interval that was used to schedule the current password check
  // time.
  base::TimeDelta GetScheduledPasswordCheckInterval() const;

  // Register a delayed task running the password check.
  void StartRepeatedUpdates();

  // Bring cached credential issues up to date with data from Password Manager.
  void UpdateInsecureCredentialCountAsync();

  // Helper function for displaying the current status in the UI.
  base::Value::Dict GetPasswordCardData(bool signed_in);

  // Returns the latest PasswordStatusCheckResult that is available in memory.
  // TODO(crbug.com/40267370): This will be a SafetyHubService implementation.
  std::optional<std::unique_ptr<SafetyHubService::Result>> GetCachedResult();

  // Returns if there is any ongoing password check or insecure credential
  // check. Returns true if is_update_credential_count_pending() or
  // is_password_check_running().
  bool IsUpdateRunning() const;

  bool is_running_weak_reused_check() const {
    return running_weak_reused_check_;
  }

  // Verifies that both `password_check_delegate_` and
  // `saved_passwords_presenter_` are initialized.
  bool IsInfrastructureReady() const;

  // Testing functions.
  bool IsObservingBulkLeakCheckForTesting() const {
    return bulk_leak_check_observation_.IsObserving();
  }

  // Public getters for testing.
  password_manager::SavedPasswordsPresenter*
  GetSavedPasswordsPresenterForTesting() {
    return saved_passwords_presenter_.get();
  }

  password_manager::InsecureCredentialsManager*
  GetInsecureCredentialsManagerForTesting() {
    return insecure_credentials_manager_.get();
  }

  password_manager::BulkLeakCheckServiceAdapter*
  GetBulkLeakCheckServiceAdapterForTesting() {
    return bulk_leak_check_service_adapter_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStatusCheckServiceBaseTest,
                           CheckTimeUpdatedAfterRunScheduledInThePast);
  FRIEND_TEST_ALL_PREFIXES(PasswordStatusCheckServiceBaseTest,
                           CheckTimeUpdatedAfterRunScheduledLongTimeInThePast);
  // The enum to hold 4 password check states.
  // As soon as the check is started, the state will be kStarting.
  // When the state is kStarting:
  // - if OnStateChanged is called, it will be kOnStateChangedCalled.
  // - if OnStartedPasswordCheck is called, it will be
  // kOnStartCallbackCompleted.
  // When the state is kOnStateChangedCalled:
  // - if OnStartedPasswordCheck is called, it will be kStopped.
  // When the state is OnStartedPasswordCheck:
  // - if OnStateChanged is called, it will be kStopped.
  //  Whenever the state is kStopped, then the infra will be killed.
  enum PasswordCheckState {
    kStarting,
    kOnStateChangedCalled,
    kOnStartCallbackCompleted,
    kStopped,
  };

  // Triggers Password Manager's password check to discover new credential
  // issues.
  void RunPasswordCheckAsync();

  // Runs weak/reused password check.
  void RunWeakReusedCheckAsync();

  // BulkLeakCheckService::Observer implementation.
  // This is observed to get notified of the progress of the password check.
  void OnStateChanged(
      password_manager::BulkLeakCheckServiceInterface::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;
  void OnBulkCheckServiceShutDown() override;

  // PasswordStoreInterface::Observer implementation.
  // Used to trigger an update of the password issue counts when passwords
  // change.
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // InsecureCredentialsManager::Observer implementation.
  void OnInsecureCredentialsChanged() override;

  // Called whenever password_check_delegate_->StartPasswordCheck is completed.
  // This is only used to prevent the infra being shutdown while there is an
  // ongoing check in the delegate.
  void OnStartedPasswordCheck(
      password_manager::BulkLeakCheckServiceInterface::State state);

  // This is called when weak and reuse checks are complete and
  // `InsecureCredentialsManager` is ready to be queried for credential issues.
  void OnWeakAndReuseChecksDone();

  // Initializes `saved_passwords_presenter_` and `password_check_delegate_`. If
  // there is no `password_store`, then the infra can not be initialized.
  void MaybeInitializePasswordCheckInfrastructure(base::OnceClosure completion);

  // Brings cached values for insecure credential counts up to date with
  // |saved_passwords_presenter_|.
  void UpdateInsecureCredentialCount();

  // Deletes `password_check_delegate_` and `saved_passwords_presenter_` if
  // async operations have concluded to keep memory footprint low.
  void MaybeResetInfrastructure();

  // Updates pref dict for scheduled password check.
  void SetPasswordCheckSchedulePrefsWithInterval(base::Time check_time);

  // Schedules the next password check time and calls StartRepeatedUpdates.
  void ScheduleCheckAndStartRepeatedUpdates();

  raw_ptr<Profile> profile_;

  // Required to obtain the list of saved passwords. Also is required for
  // construction of `PasswordCheckDelegate`. Because it is memory intensive,
  // only initialized when needed.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;

  // Used to obtain the list of insecure credentials.
  std::unique_ptr<password_manager::InsecureCredentialsManager>
      insecure_credentials_manager_;

  // Adapter used to start, monitor and stop a bulk leak check.
  std::unique_ptr<password_manager::BulkLeakCheckServiceAdapter>
      bulk_leak_check_service_adapter_;

  // A scoped observer for `BulkLeakCheckService` which is used by
  // `PasswordCheckDelegate`. This is used for detecting when password check is
  // complete through `OnStateChanged`.
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      bulk_leak_check_observation_{this};

  // Scoped observer for profile and account `PasswordStore`s. This is used
  // to trigger an update of the password issue counts when passwords have
  // changed. We're notified of this with `OnLoginsChanged`.
  base::ScopedObservation<password_manager::PasswordStoreInterface,
                          password_manager::PasswordStoreInterface::Observer>
      profile_password_store_observation_{this};
  base::ScopedObservation<password_manager::PasswordStoreInterface,
                          password_manager::PasswordStoreInterface::Observer>
      account_password_store_observation_{this};

  // A scoped observer for `insecure_credentials_manager_`. This is used for
  // detecting when insecure credentials are changed through
  // `OnInsecureCredentialsChanged`.
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      insecure_credentials_manager_observation_{this};

  // Cached results of the password check.
  size_t compromised_credential_count_ = 0;
  size_t weak_credential_count_ = 0;
  size_t reused_credential_count_ = 0;
  size_t saved_credential_count_ = 0;

  // Indicates whether weak reause check is currently running.
  bool running_weak_reused_check_ = false;

  // Timer to schedule the run of the password check after some time has passed.
  base::OneShotTimer password_check_timer_;

  // Timer to schedule the weak and reuse checks to run every day.
  base::RepeatingTimer weak_and_reuse_check_timer_;

  // The latest available result, which is initialized when the service is
  // started and whenever insecure credentials are changed.
  std::unique_ptr<PasswordStatusCheckResult> latest_result_;

  base::WeakPtrFactory<PasswordStatusCheckService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
