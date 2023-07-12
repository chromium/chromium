// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
#define CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_

#include "base/scoped_observation.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"

class PasswordStatusCheckService
    : public KeyedService,
      public password_manager::SavedPasswordsPresenter::Observer {
 public:
  explicit PasswordStatusCheckService(Profile* profile);

  PasswordStatusCheckService(const PasswordStatusCheckService&) = delete;
  PasswordStatusCheckService& operator=(const PasswordStatusCheckService&) =
      delete;

  ~PasswordStatusCheckService() override;

  // KeyedService implementation.
  void Shutdown() override;

  // Getters for different issues with credentials.
  size_t compromised_credential_count() {
    return compromised_credential_count_;
  }
  size_t weak_credential_count() { return weak_credential_count_; }
  size_t reused_credential_count() { return reused_credential_count_; }

  // Register a delayed task running the password check.
  void StartRepeatedUpdates();

  // Triggers an update to cached credential issues. Will start initialization
  // of `saved_passwords_presenter_` and observes `OnSavedPasswordsChanged`.
  void UpdateInsecureCredentialCountAsync();

  password_manager::SavedPasswordsPresenter*
  GetSavedPasswordsPresenterForTesting() {
    return saved_passwords_presenter_.get();
  }

  bool IsObservingSavedPasswordsPresenterForTesting() {
    return saved_passwords_presenter_observation_.IsObserving();
  }

  void SetTestingCallback(base::RepeatingClosure callback) {
    on_passwords_changed_finished_callback_for_test_ = std::move(callback);
  }

 private:
  // SavedPasswordsPresenter::Observer implementation.
  // Brings cached values for insecure credential counts up to date with
  // `saved_passwords_presenter_`. Getting notified about this indicates that
  // the presenter is initialized. When update is complete
  // `saved_passwords_presenter_` is reset to save memory.
  void OnSavedPasswordsChanged(
      const password_manager::PasswordStoreChangeList& changes) override;

  // This function is called at regular intervals and triggers the password
  // check, which will retrieve and store credential issues. As a result,
  // reasonably up-to-date information is made available for SafetyHub.
  void RunPasswordCheck();

  raw_ptr<Profile> profile_;

  // Required to obtain the list of saved passwords and run the password check.
  // Because it is memory-intensive, only initialized when needed.
  std::unique_ptr<password_manager::SavedPasswordsPresenter>
      saved_passwords_presenter_;

  // A scoped observer for `saved_passwords_presenter_`.
  base::ScopedObservation<password_manager::SavedPasswordsPresenter,
                          password_manager::SavedPasswordsPresenter::Observer>
      saved_passwords_presenter_observation_{this};

  // Cached results of the password check.
  size_t compromised_credential_count_ = 0;
  size_t weak_credential_count_ = 0;
  size_t reused_credential_count_ = 0;

  // If bound, will be invoked at the end of the scope of
  // `OnSavedPasswordsChanged()`.
  base::RepeatingClosure on_passwords_changed_finished_callback_for_test_;
};

#endif  // CHROME_BROWSER_UI_SAFETY_HUB_PASSWORD_STATUS_CHECK_SERVICE_H_
