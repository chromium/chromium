// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_

#include <string>

#include "components/prefs/pref_service.h"
#include "components/sync/service/sync_service.h"

namespace signin {
enum class ConsentLevel;
class IdentityManager;
}

namespace password_manager {

enum class SyncState;
struct PasswordForm;

namespace sync_util {

// Returns the sync username received from |identity_manager| (if not null).
// Moreover, using |sync_service| (if not null), this function also tries to
// return an empty string if the user isn't syncing passwords, but it is not
// always possible to determine since this code can be called during sync setup
// (http://crbug.com/393626).
std::string GetSyncUsernameIfSyncingPasswords(
    const syncer::SyncService* sync_service,
    const signin::IdentityManager* identity_manager);

// Returns true if |url| is google.com domain and |username| corresponds to the
// account specified by GetSyncUsernameIfSyncingPasswords. Returns false if
// GetSyncUsernameIfSyncingPasswords does not specify any account.
bool IsSyncAccountCredential(const GURL& url,
                             const std::u16string& username,
                             const syncer::SyncService* sync_service,
                             const signin::IdentityManager* identity_manager);

// If |username| matches the signed-in account.
bool IsSyncAccountEmail(const std::string& username,
                        const signin::IdentityManager* identity_manager,
                        signin::ConsentLevel consent_level);

// If |signon_realm| matches Gaia signon realm.
bool IsGaiaCredentialPage(const std::string& signon_realm);

// If |form|'s origin matches enterprise login URL or enterprise change password
// URL.
bool ShouldSaveEnterprisePasswordHash(const PasswordForm& form,
                                      const PrefService& prefs);

// If syncing passwords is enabled in settings.
bool IsPasswordSyncEnabled(const syncer::SyncService* sync_service);

// If passwords are actively syncing.
bool IsPasswordSyncActive(const syncer::SyncService* sync_service);

// Active syncing account if one exists. If password sync is disabled
// absl::nullopt will be returned.
absl::optional<std::string> GetSyncingAccount(
    const syncer::SyncService* sync_service);

// Returns the account where passwords are being saved, or nullopt if passwords
// are being saved only locally. In practice, this returns a non-empty
// value if the user is syncing or signed in and opted in to account storage.
absl::optional<std::string> GetAccountForSaving(
    const PrefService* pref_service,
    const syncer::SyncService* sync_service);

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING.
password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service);

}  // namespace sync_util

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_SYNC_UTIL_H_
