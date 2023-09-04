// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_

#include "base/scoped_observation.h"
#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordStatusCheckService
    : public KeyedService,
      public password_manager::SavedPasswordsPresenter::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer,
      public password_manager::PasswordStoreInterface::Observer {
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

  bool is_update_credential_count_pending() const {
    return is_update_credential_count_pending_;
  }

  bool is_password_check_running() const { return is_password_check_running_; }

  // Returns the time at which the password check is currently scheduled to run.
  base::Time GetScheduledPasswordCheckTime() const;

  // Returns the interval that was used to schedule the current password check
  // time.
  base::TimeDelta GetScheduledPasswordCheckInterval() const;

  // Register a delayed task running the password check.
  void StartRepeatedUpdates();

  // Bring cached credential issues up to date with data from Password Manager.
  void UpdateInsecureCredentialCountAsync();

  // Testing functions.
  bool IsObservingSavedPasswordsPresenterForTesting() const {
    return saved_passwords_presenter_observation_.IsObserving();
  }

  bool IsObservingBulkLeakCheckForTesting() const {
    return bulk_leak_check_observation_.IsObserving();
  }

  // Public getters for testing.
  password_manager::SavedPasswordsPresenter*
  GetSavedPasswordsPresenterForTesting() {
    return saved_passwords_presenter_.get();
  }

  extensions::PasswordCheckDelegate* GetPasswordCheckDelegateForTesting() {
    return password_check_delegate_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(PasswordStatusCheckServiceBaseTest,
                           CheckTimeUpdatedAfterRunScheduledInThePast);

  // Triggers Password Manager's password check to discover new credential
  // issues.
  void RunPasswordCheckAsync();

  // SavedPasswordsPresenter::Observer implementation.
  // Getting notified about this indicates that the presenter is initialized.
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // BulkLeakCheckService::Observer implementation.
  // This is observed to get notified of the progress of the password check.
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

  // PasswordStoreInterface::Observer implementation.
  // Used to trigger an update of the password issue counts when passwords
  // change.
  void OnLoginsChanged(
      password_manager::PasswordStoreInterface* store,
      const password_manager::PasswordStoreChangeList& changes) override;
  void OnLoginsRetained(password_manager::PasswordStoreInterface* store,
                        const std::vector<password_manager::PasswordForm>&
                            retained_passwords) override;

  // This is called when weak and reuse checks are complete and
  // `InsecureCredentialsManager` is ready to be queried for credential issues.
  void OnWeakAndReuseChecksDone();

  // Initializes |saved_passwords_presenter_| and |password_check_delegate_|.
  void InitializePasswordCheckInfrastructure();

  // Brings cached values for insecure credential counts up to date with
  // |saved_passwords_presenter_|.
  void UpdateInsecureCredentialCount();

  // Posts a task to delete `password_check_delegate_` and
  // `saved_passwords_presenter_` if async operations have concluded to keep
  // memory footprint low.
  void MaybeResetInfrastructureAsync();

  // Verifies that both `password_check_delegate_` and
  // `saved_passwords_presenter_` are initialized.
  bool IsInfrastructureReady() const;

  // Updates pref dict for scheduled password check.
  void SetPasswordCheckSchedulePrefsWithInterval(base::Time check_time);

  raw_ptr<Profile> profile_;

  // Required for `password_check_delegate_`. Because it is memory intensive,
  // only initialized when needed.
  std::unique_ptr<extensions::IdGenerator> credential_id_generator_;

  // Required to obtain the list of saved passwords. Also is required for
  // construction of `PasswordCheckDelegate`. Because it is memory intensive,
  // only initialized when needed.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;

  // Required to run the password check. Because it is memory intensive, only
  // initialized when needed.
  std::unique_ptr<extensions::PasswordCheckDelegate> password_check_delegate_;

  // A scoped observer for `saved_passwords_presenter_`. This is used for
  // detecting when `saved_passwords_presenter_` is initialized through
  // `OnSavedPasswordsChanged`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      saved_passwords_presenter_observation_{this};

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

  // Cached results of the password check.
  size_t compromised_credential_count_ = 0;
  size_t weak_credential_count_ = 0;
  size_t reused_credential_count_ = 0;

  // Flags to indicate which async operations are currently ongoing. Memory
  // intensive objects will be reset after all have finished.
  bool is_update_credential_count_pending_ = false;
  bool is_password_check_running_ = false;

  // Timer to schedule the run of the password check after some time has passed.
  base::OneShotTimer password_check_timer_;

  base::WeakPtrFactory<PasswordStatusCheckService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
