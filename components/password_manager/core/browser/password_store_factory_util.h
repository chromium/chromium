// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

namespace network::mojom {
class NetworkContext;
}  // namespace network::mojom

class PrefService;

namespace password_manager {

class CredentialsCleanerRunner;
class LoginDatabase;
class PasswordStoreInterface;

// Creates a LoginDatabase. Looks in |db_directory| for the database file.
// Does not call LoginDatabase::Init() -- to avoid UI jank, that needs to be
// called by PasswordStore::Init() on the background thread.
// If a non-null `is_empty_cb` is passed, it's called to signal whether the
// database is empty, i.e. without any logins *or* blocklists. The call
// happens when initializing the database and when adding/removing entries,
// regardless of success.
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForProfileStorage(
    const base::FilePath& db_directory,
    const base::RepeatingCallback<void(bool)>& is_empty_cb);
std::unique_ptr<LoginDatabase> CreateLoginDatabaseForAccountStorage(
    const base::FilePath& db_directory);

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
void RemoveUselessCredentials(
    CredentialsCleanerRunner* cleaning_tasks_runner,
    scoped_refptr<PasswordStoreInterface> store,
    PrefService* prefs,
    base::TimeDelta delay,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter);

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_FACTORY_UTIL_H_
