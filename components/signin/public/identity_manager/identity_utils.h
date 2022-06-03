// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_

#include <string>

class PrefService;

namespace signin {

// Returns true if the username is allowed based on a pattern registered
// |prefs::kGoogleServicesUsernamePattern| with the preferences service
// referenced by |prefs|.
bool IsUsernameAllowedByPatternFromPrefs(const PrefService* prefs,
                                         const std::string& username);
}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_IDENTITY_UTILS_H_
