// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_

#include <map>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/strings/string16.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "ui/gfx/native_widget_types.h"

namespace autofill {
struct PasswordForm;
class AutofillClient;
}

namespace network {
namespace mojom {
class NetworkContext;
}
}

namespace password_manager {
class PasswordManagerDriver;
class PasswordManagerClient;
class PasswordStore;
}

namespace syncer {
class SyncService;
}

class PrefService;

namespace password_manager_util {

// Update |credential| to reflect usage.
void UpdateMetadataForUsage(autofill::PasswordForm* credential);

// Reports whether and how passwords are currently synced. In particular, for a
// null |sync_service| returns NOT_SYNCING.
password_manager::SyncState GetPasswordSyncState(
    const syncer::SyncService* sync_service);

// Finds the forms with a duplicate sync tags in |forms|. The first one of
// the duplicated entries stays in |forms|, the others are moved to
// |duplicates|.
// |tag_groups| is optional. It will contain |forms| and |duplicates| grouped by
// the sync tag. The first element in each group is one from |forms|. It's
// followed by the duplicates.
void FindDuplicates(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* forms,
    std::vector<std::unique_ptr<autofill::PasswordForm>>* duplicates,
    std::vector<std::vector<autofill::PasswordForm*>>* tag_groups);

// Removes Android username-only credentials from |android_credentials|.
// Transforms federated credentials into non zero-click ones.
void TrimUsernameOnlyCredentials(
    std::vector<std::unique_ptr<autofill::PasswordForm>>* android_credentials);

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

// Opens Password Manager setting page and records the metrics.
void UserTriggeredShowAllSavedPasswordsFromContextMenu(
    autofill::AutofillClient* autofill_client);

// Triggers password generation flow and records the metrics.
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
// This assumes that the |form|'s origin is a substring of the signon_realm.
base::StringPiece GetSignonRealmWithProtocolExcluded(
    const autofill::PasswordForm& form);

// Given all non-blacklisted |matches|, finds and populates
// |best_matches_|, |preferred_match_| and |non_best_matches_| accordingly.
// For comparing credentials the following rule is used: non-psl match is better
// than psl match, preferred match is better than non-preferred match. In case
// of tie, an arbitrary credential from the tied ones is chosen for
// |best_matches| and preferred_match.
void FindBestMatches(
    std::vector<const autofill::PasswordForm*> matches,
    std::map<base::string16, const autofill::PasswordForm*>* best_matches,
    std::vector<const autofill::PasswordForm*>* not_best_matches,
    const autofill::PasswordForm** preferred_match);

}  // namespace password_manager_util

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_MANAGER_UTIL_H_
