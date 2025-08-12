// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_backup_password_cleaner.h"

#include "base/time/time.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

PasswordChangeBackupPasswordCleaner::PasswordChangeBackupPasswordCleaner(
    IsAccountStore is_account_store,
    scoped_refptr<PasswordStoreInterface> store,
    PrefService* pref_service)
    : store_(std::move(store)),
      pref_service_(pref_service),
      store_pref_name_(
          is_account_store
              ? prefs::kAccountStoreBackupPasswordCleaningLastTimestamp
              : prefs::kProfileStoreBackupPasswordCleaningLastTimestamp) {}

PasswordChangeBackupPasswordCleaner::~PasswordChangeBackupPasswordCleaner() =
    default;

bool PasswordChangeBackupPasswordCleaner::NeedsCleaning() {
  base::Time last_time = pref_service_->GetTime(store_pref_name_);
  return base::Time::Now() - last_time >= kBackupPasswordCleaningDelay;
}

void PasswordChangeBackupPasswordCleaner::StartCleaning(Observer* observer) {
  CHECK(observer);
  CHECK(!observer_);
  observer_ = observer;
  store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void PasswordChangeBackupPasswordCleaner::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  CHECK(store_ == store);
  if (std::holds_alternative<PasswordStoreBackendError>(results_or_error)) {
    // Notify observer that cleaning is complete, but don't set the timestamp in
    // the pref, so it can be retried again in the future.
    observer_->CleaningCompleted();
    return;
  }

  base::Time cleaning_time = base::Time::Now();
  for (const PasswordForm& form : std::get<LoginsResult>(results_or_error)) {
    std::optional<base::Time> note_created_time =
        form.GetPasswordBackupDateCreated();
    if (note_created_time.has_value() &&
        cleaning_time - *note_created_time >= kBackupPasswordTTL) {
      PasswordForm updated_form = form;
      updated_form.DeletePasswordBackupNote();
      store_->UpdateLogin(std::move(updated_form));
    }
  }

  pref_service_->SetTime(store_pref_name_, cleaning_time);
  observer_->CleaningCompleted();
}

}  // namespace password_manager
