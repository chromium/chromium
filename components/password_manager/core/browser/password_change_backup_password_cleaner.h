// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_BACKUP_PASSWORD_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_BACKUP_PASSWORD_CLEANER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store/password_store_consumer.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

class PrefService;

namespace password_manager {

// Time that should pass since backup password creation until removal.
inline constexpr base::TimeDelta kBackupPasswordTTL = base::Days(90);

// Time that should pass since the last cleaning attempt to run another.
inline constexpr base::TimeDelta kBackupPasswordCleaningDelay = base::Days(30);

// Responsible for clearing backup passwords created in the password change flow
// that have reached their TTL.
class PasswordChangeBackupPasswordCleaner : public PasswordStoreConsumer,
                                            public CredentialsCleaner {
 public:
  PasswordChangeBackupPasswordCleaner(
      IsAccountStore is_account_store,
      scoped_refptr<PasswordStoreInterface> store,
      PrefService* prefs);
  PasswordChangeBackupPasswordCleaner(
      const PasswordChangeBackupPasswordCleaner&) = delete;
  PasswordChangeBackupPasswordCleaner& operator=(
      const PasswordChangeBackupPasswordCleaner&) = delete;
  ~PasswordChangeBackupPasswordCleaner() override;

  // CredentialsCleaner:
  bool NeedsCleaning() override;
  void StartCleaning(Observer* observer) override;

 private:
  // PasswordStoreConsumer:
  void OnGetPasswordStoreResultsOrErrorFrom(
      PasswordStoreInterface* store,
      LoginsResultOrError results_or_error) override;

  // Password store for which the cleaning is performed.
  scoped_refptr<PasswordStoreInterface> store_;

  // Used to query and record the last time cleaning happened.
  raw_ptr<PrefService> pref_service_;

  // Pref keeping track when the last cleaning happened for the `store_`.
  const std::string_view store_pref_name_;

  // Used to signal completion of the clean-up. Null until cleaning starts.
  raw_ptr<Observer> observer_ = nullptr;

  base::WeakPtrFactory<PasswordChangeBackupPasswordCleaner> weak_ptr_factory_{
      this};
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_CHANGE_BACKUP_PASSWORD_CLEANER_H_
