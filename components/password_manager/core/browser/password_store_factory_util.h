// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_store/login_database.h"
#include "components/password_manager/core/browser/password_store/password_store_interface.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

class PrefService;

namespace password_manager {

class CredentialsCleanerRunner;
class PasswordStoreBackend;

// Creates a LoginDatabase. Looks in |db_directory| for the database file.
// Does not call LoginDatabase::Init() -- to avoid UI jank, that needs to be
// called by PasswordStore::Init() on the background thread.
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& db_directory,
    PrefService* prefs);
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& db_directory,
    PrefService* prefs);

// This function handles the following clean-ups of credentials:
// (1) Removing blocklisted duplicates: if two blocklisted credentials have the
// same signon_realm, they are duplicates of each other. Deleting all but one
// sharing the signon_realm does not affect Chrome's behaviour and hence
// duplicates can be removed. Having duplicates makes un-blocklisting not work,
// hence blocklisted duplicates need to be removed.
// (2) Removing or fixing of HTTPS credentials with wrong signon_realm. See
// https://crbug.com/881731 for details.
// (3) Report metrics about HTTP to HTTPS migration process and remove obsolete
// HTTP credentials. This feature is not available on iOS platform because the
// HSTS query is not supported. |network_context_getter| is always null for iOS
// and it can also be null for some unittests.
// (4) Indirectly re-encrypts all passwords by reading them form the |store| and
// invoking UpdateLogins.
void SanitizeAndMigrateCredentials(
    CredentialsCleanerRunner* cleaning_tasks_runner,
    scoped_refptr<PasswordStoreInterface> store,
    password_manager::IsAccountStore is_account_store,
    PrefService* prefs,
    base::TimeDelta delay,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter);

// Checks that the backend was not yet shut down (i.e. the weak pointer to the
// backend was not yet invalidated) before calling `set_prefs_callback`.
//
// Example of usage:
//
// base::BindRepeating(
//     &password_manager::IntermediateCallbackForSettingPrefs,
//     backend->AsWeakPtr(), base::BindRepeating(
//         &password_manager::SetEmptyStorePref, pref_service,
//        password_manager::prefs::
//            kEmptyProfileStoreLoginDatabase))
void IntermediateCallbackForSettingPrefs(
    base::WeakPtr<PasswordStoreBackend> backend,
    base::RepeatingCallback<void(LoginDatabase::LoginDatabaseEmptinessState)>
        set_prefs_callback,
    LoginDatabase::LoginDatabaseEmptinessState value);

// Extracts `value.no_login_found` and uses it as a value for the pref.
// Important! Always wrap this method in
// `IntermediateCallbackForSettingPrefs()`. No prefs should be set after
// `PasswordStoreBackend::Shutdown()` was called, because it will lead to
// use-after-free.
// If this method didn't rely on `IntermediateCallbackForSettingPrefs()` to
// check whether the backend has been shut down, and instead did this check
// itself, a dangling pointer error would occur in `base::Unretained()` (because
// `prefs` would be dangling). The error occurs in spite of the fact that
// `prefs` is never dereferenced after the backend was shut down.
void SetEmptyStorePref(PrefService* prefs,
                       const std::string& pref,
                       LoginDatabase::LoginDatabaseEmptinessState value);

// Same as `SetEmptyStorePref()`, with the only difference that it extracts
// `value.autofillable_credentials_exist` and uses it as a value for the pref.
// Important! Always wrap this method in
// `IntermediateCallbackForSettingPrefs()`.
void SetAutofillableCredentialsStorePref(
    PrefService* prefs,
    const std::string& pref,
    LoginDatabase::LoginDatabaseEmptinessState value);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
