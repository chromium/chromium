// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store_factory_util.h"

#include <memory>
#include <utility>

#include "build/blink_buildflags.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/affiliation/affiliated_match_helper.h"
#include "components/password_manager/core/browser/credentials_cleaner.h"
#include "components/password_manager/core/browser/credentials_cleaner_runner.h"
#include "components/password_manager/core/browser/http_credentials_cleaner.h"
#include "components/password_manager/core/browser/old_google_credentials_cleaner.h"
#include "components/password_manager/core/browser/os_crypt_async_migrator.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store_backend.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"

namespace password_manager {

namespace {

LoginDatabase::DeletingUndecryptablePasswordsEnabled GetPolicyFromPrefs(
    PrefService* prefs) {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_IOS)
  return LoginDatabase::DeletingUndecryptablePasswordsEnabled(
      prefs->GetBoolean(prefs::kDeletingUndecryptablePasswordsEnabled));
#else
  return LoginDatabase::DeletingUndecryptablePasswordsEnabled(true);
#endif
}

}  // namespace

std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& db_directory,
    PrefService* prefs) {
  base::FilePath login_db_file_path =
      db_directory.Append(kLoginDataForProfileFileName);
  return std::make_unique<LoginDatabase>(
      login_db_file_path, IsAccountStore(false), GetPolicyFromPrefs(prefs));
}

std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& db_directory,
    PrefService* prefs) {
  base::FilePath login_db_file_path =
      db_directory.Append(kLoginDataForAccountFileName);
  return std::make_unique<LoginDatabase>(
      login_db_file_path, IsAccountStore(true), GetPolicyFromPrefs(prefs));
}

// TODO(http://crbug.com/890318): Add unitests to check cleaners are correctly
// created.
void SanitizeAndMigrateCredentials(
    password_manager::CredentialsCleanerRunner* cleaning_tasks_runner,
    scoped_refptr<password_manager::PasswordStoreInterface> store,
    password_manager::IsAccountStore is_account_store,
    PrefService* prefs,
    base::TimeDelta delay,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter) {
  DCHECK(cleaning_tasks_runner);

#if BUILDFLAG(USE_BLINK)
  // Can be null for some unittests.
  if (!network_context_getter.is_null()) {
    cleaning_tasks_runner->MaybeAddCleaningTask(
        std::make_unique<password_manager::HttpCredentialCleaner>(
            store, network_context_getter, prefs));
  }
#endif  // BUILDFLAG(USE_BLINK)

  // TODO(crbug.com/41153113): Remove this when enough number of clients switch
  // to the new version of Chrome.
  cleaning_tasks_runner->MaybeAddCleaningTask(
      std::make_unique<password_manager::OldGoogleCredentialCleaner>(store,
                                                                     prefs));

#if !BUILDFLAG(IS_ANDROID)
  cleaning_tasks_runner->MaybeAddCleaningTask(
      std::make_unique<password_manager::OSCryptAsyncMigrator>(
          store, is_account_store, prefs));
#endif

  if (cleaning_tasks_runner->HasPendingTasks()) {
    // The runner will delete itself once the clearing tasks are done, thus we
    // are releasing ownership here.
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &password_manager::CredentialsCleanerRunner::StartCleaning,
            cleaning_tasks_runner->GetWeakPtr()),
        delay);
  }
}

void IntermediateCallbackForSettingPrefs(
    base::WeakPtr<PasswordStoreBackend> backend,
    base::RepeatingCallback<void(LoginDatabase::LoginDatabaseEmptinessState)>
        set_prefs_callback,
    LoginDatabase::LoginDatabaseEmptinessState value) {
  // When a `PasswordStoreBackend` is shut down, the weak pointers are
  // invalidated.
  if (backend) {
    set_prefs_callback.Run(value);
  }
}

void SetEmptyStorePref(PrefService* prefs,
                       const std::string& pref,
                       LoginDatabase::LoginDatabaseEmptinessState value) {
  CHECK(prefs);
  prefs->SetBoolean(pref, value.no_login_found);
}

void SetAutofillableCredentialsStorePref(
    PrefService* prefs,
    const std::string& pref,
    LoginDatabase::LoginDatabaseEmptinessState value) {
  CHECK(prefs);
  prefs->SetBoolean(pref, value.autofillable_credentials_exist);
}

}  // namespace password_manager
