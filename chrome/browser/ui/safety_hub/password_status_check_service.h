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
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordStatusCheckService
    : public KeyedService,
      public password_manager::InsecureCredentialsManager::Observer,
      public password_manager::BulkLeakCheckServiceInterface::Observer {
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

  // Register a delayed task running the password check.
  void StartRepeatedUpdates();

  // Bring cached credential issues up to date with data from Password Manager.
  void UpdateInsecureCredentialCountAsync();

  // Triggers Password Manager's password check to discover new credential
  // issues.
  //
  // TODO(crbug.com/1443466) Make private once there is a way for the password
  // check to be publicly triggered.
  void RunPasswordCheckAsync();

  // Testing functions.
  bool IsObservingInsecureCredentialsManagerForTesting() const {
    return insecure_credentials_manager_observation_.IsObserving();
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
  // InsecureCredentialsManager::Observer implementation.
  // Getting notified about this indicates that the presenter is initialized and
  // that weak and reuse checks have concluded.
  void OnInsecureCredentialsChanged() override;

  // BulkLeakCheckService::Observer implementation.
  // This is observed to get notified of the progress of the password check.
  void OnStateChanged(
      password_manager::BulkLeakCheckService::State state) override;
  void OnCredentialDone(const password_manager::LeakCheckCredential& credential,
                        password_manager::IsLeaked is_leaked) override;

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

  // A scoped observer for `InsecureCredentialsManager`. This is used for
  // detecting when password issues are available through
  // `OnInsecureCredentialsChanged`.
  base::ScopedObservation<
      password_manager::InsecureCredentialsManager,
      password_manager::InsecureCredentialsManager::Observer>
      insecure_credentials_manager_observation_{this};

  // A scoped observer for `BulkLeakCheckService` which is used by
  // `PasswordCheckDelegate`. This is used for detecting when password check is
  // complete through `OnStateChanged`.
  base::ScopedObservation<
      password_manager::BulkLeakCheckServiceInterface,
      password_manager::BulkLeakCheckServiceInterface::Observer>
      bulk_leak_check_observation_{this};

  // Cached results of the password check.
  size_t compromised_credential_count_ = 0;
  size_t weak_credential_count_ = 0;
  size_t reused_credential_count_ = 0;

  // Flags to indicate which async operations are currently ongoing. Memory
  // intensive objects will be reset after all have finished.
  bool is_update_credential_count_pending_ = false;
  bool is_password_check_running_ = false;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
