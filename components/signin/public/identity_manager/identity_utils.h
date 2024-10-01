// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_

#include <string>

#include "base/containers/flat_set.h"

class PrefService;

namespace signin {

class AccountsInCookieJarInfo;
class IdentityManager;

// Returns true if the username is allowed based on a pattern registered
// |prefs::kGoogleServicesUsernamePattern| with the preferences service
// referenced by |prefs|.
bool IsUsernameAllowedByPatternFromPrefs(const PrefService* prefs,
                                         const std::string& username);

// Returns true:
// - if `switches::kExplicitBrowserSigninUIOnDesktop` feature is disabled.
// - The user is signed in to the browser implicitly by signing in on the
//   web.
// It will return false if the feature is enabled and the user is either signed
// out or signed in explicitly.
bool IsImplicitBrowserSigninOrExplicitDisabled(
    const IdentityManager* identity_manager,
    const PrefService* prefs);

// Returns true if the Google account cookies are automatically rebuilt after
// being cleared from settings, when the user is signed in.
// Note: this can return true even if the user is not signed in. This function
// reflects whether the cookie setting has this new behavior (as opposed to the
// old behavior where cookies were never rebuilt).
bool AreGoogleCookiesRebuiltAfterClearingWhenSignedIn(IdentityManager& manager,
                                                      PrefService& prefs);

// Returns all accounts for which Chrome should keep account-keyed preferences.
// These are the accounts in the cookie (signed in or signed out) plus the
// primary account.
// In particular, when the cookies are cleared while signed in, they may be
// rebuilt immediately, and in that case it is very important to not clear the
// preferences.
// `identity_manager` may be nullptr.
// `accounts_in_cookie_jar_info.AreAccountsFresh()` must be true.
base::flat_set<std::string> GetAllGaiaIdsForKeyedPreferences(
    const IdentityManager* identity_manager,
    const AccountsInCookieJarInfo& accounts_in_cookie_jar_info);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
