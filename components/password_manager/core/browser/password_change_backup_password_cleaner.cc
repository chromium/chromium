// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change_backup_password_cleaner.h"

#include <algorithm>
#include <ranges>

#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store/password_form_converters.h"
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
  for (auto& cred : std::get<LoginsResult>(std::move(results_or_error))) {
    auto note_itr = std::ranges::find(
        cred.notes, PasswordNote::kPasswordChangeBackupNoteName,
        &PasswordNote::unique_display_name);
    if (note_itr != cred.notes.end() && !note_itr->value.empty() &&
        cleaning_time - note_itr->date_created >= kBackupPasswordTTL) {
      cred.notes.erase(note_itr);
      store_->UpdateLogin(std::move(cred));
    }
  }

  pref_service_->SetTime(store_pref_name_, cleaning_time);
  observer_->CleaningCompleted();
}

}  // namespace password_manager
