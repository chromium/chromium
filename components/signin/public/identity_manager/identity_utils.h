// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_

#include <string>

class PrefService;

namespace signin {

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
    IdentityManager* identity_manager,
    PrefService* prefs);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
