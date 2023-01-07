// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_CONSTANTS_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_CONSTANTS_H_

#include <set>
#include <string>

// File that lists the Google cloud authenticated APIs (OAuth scopes) and
// the consent level required to access them.
namespace signin {

// Set of Google OAuth2 API scopes that do not require user consent for their
// usage - these scopes are accessible at ConsentLevel::kSignin.
const std::set<std::string> GetUnconsentedOAuth2Scopes();

// Set of Google OAuth2 API scopes that require privileged access - these scopes
// are accessible by consumers listed in GetPrivilegedOAuth2Consumers().
const std::set<std::string> GetPrivilegedOAuth2Scopes();

// Set of consumers that have privileged access to Google APIs. These consumers
// have access to all API scopes regardless of the user consent level.
const std::set<std::string> GetPrivilegedOAuth2Consumers();

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_CONSTANTS_H_
