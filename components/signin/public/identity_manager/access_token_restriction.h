// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_
#define COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_

#include <string>

#include "components/signin/public/base/oauth_consumer_id.h"

// File that lists the Google cloud authenticated APIs (OAuth2 scopes) and
// the consent level required to access them.
namespace signin {

enum class OAuth2ScopeRestriction {
  // The scope can be used with no restriction.
  kNoRestriction,
  // The scope can be used when the user is signed in to Chrome.
  kSignedIn,
  // The scope can only be used by a OAuth2 privileged consumer.
  kPrivilegedOAuth2Consumer,
};

OAuth2ScopeRestriction GetOAuth2ScopeRestriction(const std::string& scope);

// Returns true for set of consumers that have privileged access to Google APIs.
// These consumers have access to all API scopes regardless of the user consent
// level.
bool IsPrivilegedOAuth2Consumer(OAuthConsumerId oauth_consumer_id);

// Returns true if the specific combination of consumer and scope is allowlisted
// to bypass any default restrictions (e.g. required sign-in level).
bool IsConsumerAllowlistedForScope(OAuthConsumerId oauth_consumer_id,
                                   const std::string& scope);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_IDENTITY_MANAGER_ACCESS_TOKEN_RESTRICTION_H_
