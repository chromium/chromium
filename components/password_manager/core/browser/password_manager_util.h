// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_store.h"
#include "ui/gfx/native_widget_types.h"

namespace network {
namespace mojom {
class NetworkContext;
}
}  // namespace network

namespace password_manager {
class PasswordManagerDriver;
class PasswordManagerClient;
}  // namespace password_manager

namespace syncer {
class SyncService;
}

class PrefService;

namespace password_manager_util {

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(password_manager::PasswordForm* credential);

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING.
password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service);

// Reports whether passwords are synced with normal encryption, i.e. without a
// custom passphrase.
bool IsSyncingWithNormalEncryption(const syncer::SyncService* sync_service);

// Removes Android username-only credentials from |android_credentials|.
// Transforms federated credentials into non zero-click ones.
void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<password_manager::PasswordForm>>*
        android_credentials);

// A convenience function for testing that |client| has a non-null LogManager
// and that that LogManager returns true for IsLoggingActive. This function can
// be removed once PasswordManagerClient::GetLogManager is implemented on iOS
// and required to always return non-null.
bool IsLoggingActive(const password_manager::PasswordManagerClient* client);

// True iff the manual password generation is enabled for the current site.
bool ManualPasswordGenerationEnabled(
    password_manager::PasswordManagerDriver* driver);

// Returns true iff the "Show all saved passwords" option should be shown in
// Context Menu. Also records metric, that the Context Menu will have "Show all
// saved passwords" option.
bool ShowAllSavedPasswordsContextMenuEnabled(
    password_manager::PasswordManagerDriver* driver);

// Triggers password generation flow and records the metrics. If the user should
// be asked to opt in to account storage, will trigger a reauth flow first and
// generation will only happen on success.
void UserTriggeredManualGenerationFromContextMenu(
    password_manager::PasswordManagerClient* password_manager_client);

// This function handles the following clean-ups of credentials:
// (1) Removing blacklisted duplicates: if two blacklisted credentials have the
// same signon_realm, they are duplicates of each other. Deleting all but one
// sharing the signon_realm does not affect Chrome's behaviour and hence
// duplicates can be removed. Having duplicates makes un-blacklisting not work,
// hence blacklisted duplicates need to be removed.
// (2) Removing or fixing of HTTPS credentials with wrong signon_realm. See
// https://crbug.com/881731 for details.
// (3) Report metrics about HTTP to HTTPS migration process and remove obsolete
// HTTP credentials. This feature is not available on iOS platform because the
// HSTS query is not supported. |network_context_getter| is always null for iOS
// and it can also be null for some unittests.
void RemoveUselessCredentials(
    scoped_refptr<password_manager::PasswordStore> store,
    PrefService* prefs,
    int delay_in_seconds,
    base::RepeatingCallback<network::mojom::NetworkContext*()>
        network_context_getter);

// Excluding protocol from a signon_realm means to remove from the signon_realm
// what is before the web origin (with the protocol excluded as well). For
// example if the signon_realm is "https://www.google.com/", after
// excluding protocol it becomes "www.google.com/".
// This assumes that the |form|'s host is a substring of the signon_realm.
base::StringPiece GetSignonRealmWithProtocolExcluded(
    const password_manager::PasswordForm& form);

// Given all non-blacklisted |non_federated_matches|, finds and populates
// |non_federated_same_scheme|, |best_matches|, and |preferred_match|
// accordingly. For comparing credentials the following rule is used: non-psl
// match is better than psl match, most recently used match is better than other
// matches. In case of tie, an arbitrary credential from the tied ones is chosen
// for |best_matches| and |preferred_match|.
void FindBestMatches(
    const std::vector<const password_manager::PasswordForm*>&
        non_federated_matches,
    password_manager::PasswordForm::Scheme scheme,
    std::vector<const password_manager::PasswordForm*>*
        non_federated_same_scheme,
    std::vector<const password_manager::PasswordForm*>* best_matches,
    const password_manager::PasswordForm** preferred_match);

// Returns a form with the given |username_value| from |forms|, or nullptr if
// none exists. If multiple matches exist, returns the first one.
const password_manager::PasswordForm* FindFormByUsername(
    const std::vector<const password_manager::PasswordForm*>& forms,
    const base::string16& username_value);

// If the user submits a form, they may have used existing credentials, new
// credentials, or modified existing credentials that should be updated.
// The function returns a form from |credentials| that is the best candidate to
// use for an update. Returned value is NULL if |submitted_form| looks like a
// new credential for the site to be saved.
// |submitted_form| is the form being submitted.
// |credentials| are all the credentials relevant for the current site including
// PSL and Android matches.
const password_manager::PasswordForm* GetMatchForUpdating(
    const password_manager::PasswordForm& submitted_form,
    const std::vector<const password_manager::PasswordForm*>& credentials);

// This method creates a blacklisted form with |digests|'s scheme, signon_realm
// and origin. This is done to avoid storing PII and to have a normalized unique
// key. Furthermore it attempts to normalize the origin by stripping path
// components. In case this fails (e.g. for non-standard origins like Android
// credentials), the original origin is kept.
password_manager::PasswordForm MakeNormalizedBlacklistedForm(
    password_manager::PasswordStore::FormDigest digest);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
