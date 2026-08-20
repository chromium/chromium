// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/signin/public/identity_manager/account_info.h"

class GaiaId;
class PrefService;

namespace signin {

class AccountsInCookieJarInfo;
class IdentityManager;

// Returns true if the username matches the pattern.
// The pattern is a RE2 pattern. If the pattern is empty, all usernames are
// allowed. If the pattern is invalid, no usernames are allowed.
bool IsUsernameAllowedByPattern(std::string_view username,
                                std::string_view pattern);

// Returns true if the username is allowed based on a pattern registered
// |prefs::kGoogleServicesUsernamePattern| with the preferences service
// referenced by |prefs|.
bool IsUsernameAllowedByPatternFromPrefs(const PrefService* prefs,
                                         const std::string& username);

// Returns all accounts for which Chrome should keep account-keyed preferences.
// These are the accounts in the cookie (signed in or signed out) plus the
// primary account.
// In particular, when the cookies are cleared while signed in, they may be
// rebuilt immediately, and in that case it is very important to not clear the
// preferences.
// `identity_manager` may be nullptr.
// `accounts_in_cookie_jar_info.AreAccountsFresh()` must be true.
base::flat_set<GaiaId> GetAllGaiaIdsForKeyedPreferences(
    const IdentityManager* identity_manager,
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

// Returns the candidate accounts in priority order for display in sign-in
// promos or UI.
// - If signed in to Chrome, the primary account is always returned first.
// - On iOS: device accounts in system keychain order.
// - On Android: accounts with refresh tokens in device order.
// - On Desktop: accounts with refresh tokens in Gaia cookie jar order.
//
// If `local_state` is provided, accounts disallowed by enterprise pattern
// policies (`prefs::kGoogleServicesUsernamePattern`) are filtered out.
std::vector<AccountInfo> GetOrderedAccountsForDisplay(
    const IdentityManager* identity_manager,
    const PrefService* local_state = nullptr);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
