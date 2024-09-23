// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_

#include <string>

#include "base/containers/flat_set.h"

// File that lists the Google cloud authenticated APIs (OAuth2 scopes) and
// the consent level required to access them.
namespace signin {

enum class OAuth2ScopeRestriction {
  kNoRestriction = 0,
  kSignedIn = 1,
  kExplicitConsent = 2,
  kPrivilegedOAuth2Consumer = 3,
};

OAuth2ScopeRestriction GetOAuth2ScopeRestriction(const std::string& scope);

// Returns true for set of consumers that have privileged access to Google APIs.
// These consumers have access to all API scopes regardless of the user consent
// level.
bool IsPrivilegedOAuth2Consumer(const std::string& consumer_name);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_
