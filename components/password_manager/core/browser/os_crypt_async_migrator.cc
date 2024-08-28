// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/os_crypt_async_migrator.h"

#include "base/time/time.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

OSCryptAsyncMigrator::OSCryptAsyncMigrator(
    scoped_refptr<PasswordStoreInterface> store,
    IsAccountStore is_account_store,
    PrefService* prefs)
    : store_(std::move(store)),
      store_pref_name_(is_account_store
                           ? prefs::kAccountStoreMigratedToOSCryptAsync
                           : prefs::kProfileStoreMigratedToOSCryptAsync),
      prefs_(prefs) {}

OSCryptAsyncMigrator::~OSCryptAsyncMigrator() = default;

bool OSCryptAsyncMigrator::NeedsCleaning() {
  // Phase 1 of OSCryptMigration has to be enabled.
  if (!base::FeatureList::IsEnabled(
          features::kUseAsyncOsCryptInLoginDatabase)) {
    return false;
  }
  // Phase 2 of OSCryptMigration has to be enabled.
  if (!base::FeatureList::IsEnabled(features::kUseNewEncryptionMethod)) {
    return false;
  }
  if (!base::FeatureList::IsEnabled(
          features::kEncryptAllPasswordsWithOSCryptAsync)) {
    return false;
  }
  return !prefs_->GetBoolean(store_pref_name_);
}

void OSCryptAsyncMigrator::StartCleaning(Observer* observer) {
  CHECK(NeedsCleaning());
  CHECK(observer);
  CHECK(!observer_);
  CHECK(base::FeatureList::IsEnabled(features::kUseNewEncryptionMethod));
  observer_ = observer;
  store_->GetAutofillableLogins(weak_ptr_factory_.GetWeakPtr());
}

void OSCryptAsyncMigrator::OnGetPasswordStoreResultsOrErrorFrom(
    PasswordStoreInterface* store,
    LoginsResultOrError results_or_error) {
  CHECK(store_ == store);
  if (absl::holds_alternative<PasswordStoreBackendError>(results_or_error)) {
    // Notify observer that cleaning is complete. Although don't mark it as such
    // to retry again in the future.
    observer_->CleaningCompleted();
    return;
  }

  LoginsResult logins = std::move(absl::get<LoginsResult>(results_or_error));

  if (logins.empty()) {
    MarkMigrationComplete();
    return;
  }

  store->UpdateLogins(
      logins, base::BindOnce(&OSCryptAsyncMigrator::MarkMigrationComplete,
                             weak_ptr_factory_.GetWeakPtr()));
}

void OSCryptAsyncMigrator::MarkMigrationComplete() {
  CHECK(observer_);
  CHECK(!prefs_->GetBoolean(store_pref_name_));

  prefs_->SetBoolean(store_pref_name_, true);
  observer_->CleaningCompleted();
}

}  // namespace password_manager
