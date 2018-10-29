// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INVALID_REALM_CREDENTIAL_CLEANER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INVALID_REALM_CREDENTIAL_CLEANER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/sequence_checker.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/password_store_consumer.h"

namespace autofill {
struct PasswordForm;
}  // namespace autofill

class PrefService;

namespace password_manager {

class PasswordStore;

// This class removes HTTPS credentials with invalid signon_realm created by
// faulty HTTP->HTTPS migration. See https://crbug.com/881731 for details.
// TODO(https://crbug.com/880090): Remove the code once majority of the users
// executed it.
class InvalidRealmCredentialCleaner : public PasswordStoreConsumer,
                                      public CredentialsCleaner {
 public:
  // The cleaning will be made for credentials from |store|.
  // A preference from |prefs| is used to set the cleaning state as finished.
  InvalidRealmCredentialCleaner(scoped_refptr<PasswordStore> store,
                                PrefService* prefs);

  ~InvalidRealmCredentialCleaner() override;

  // CredentialsCleaner:
  void StartCleaning(Observer* observer) override;

  // PasswordStoreConsumer:
  void OnGetPasswordStoreResults(
      std::vector<std::unique_ptr<autofill::PasswordForm>> results) override;

 private:
  // After the clean-up is done, this function will inform the |observer_| about
  // clean-up completion and set a preference in |prefs_| to true to avoid
  // calling clean-up multiple times.
  void CleaningFinished();

  // Clean-up is performed on |store_|.
  scoped_refptr<PasswordStore> store_;

  // |prefs_| is not an owning pointer. It is used to record that the clean-up
  // happened and thus does not have to happen again.
  PrefService* prefs_;

  // Used to signal completion of the clean-up. It is null until
  // StartCleaning is called.
  Observer* observer_ = nullptr;

  // Indicates the number of responses from PasswordStore remaining to be
  // processed. The number will be set in StartCleaning method.
  int remaining_cleaning_tasks_ = 0;

  // This guard checks that all operations performed on
  // |remaining_cleaning_tasks_| run on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(InvalidRealmCredentialCleaner);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_INVALID_REALM_CREDENTIAL_CLEANER_H_