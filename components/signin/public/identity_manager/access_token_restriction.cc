// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_restriction.h"

#include "base/containers/fixed_flat_set.h"
#include "build/build_config.h"
#include "components/signin/public/base/signin_switches.h"
#include "google_apis/gaia/gaia_constants.h"

#if !BUILDFLAG(IS_FUCHSIA)
#include "pdf/buildflags.h"  // nogncheck
#endif                       // !BUILDFLAG(IS_FUCHSIA)

namespace signin {

namespace {

// Returns true if `scope` is a Google OAuth2 API scope that do not require user
// to be signed in to the browser.
bool IsUnrestrictedOAuth2Scopes(const std::string& scope) {
#if !BUILDFLAG(IS_ANDROID)
  // Check kill switch for Device Management Service OAuth scope.
  if (scope == GaiaConstants::kDeviceManagementServiceOAuth) {
    return !base::FeatureList::IsEnabled(
        switches::kRestrictDeviceManagementServiceOAuthScope);
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  static constexpr auto kScopes = base::MakeFixedFlatSet<std::string_view>({
      GaiaConstants::kGoogleUserInfoEmail,
      GaiaConstants::kGoogleUserInfoProfile,

      // Required to fetch the ManagedAccounsSigninRestriction policy during
      // sign in.
      GaiaConstants::kSecureConnectOAuth2Scope,

#if !BUILDFLAG(IS_FUCHSIA)
#if BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
      GaiaConstants::kDriveOAuth2Scope,
#endif  // BUILDFLAG(ENABLE_PDF_SAVE_TO_DRIVE)
#endif  // !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_ANDROID)
      // Required by cloud policy.
      // On Android, cloud policies are fetched before sign in is completed.
      GaiaConstants::kDeviceManagementServiceOAuth,
#endif  // BUILDFLAG(IS_ANDROID)
  });

  return kScopes.contains(scope);
}

// Returns true if `scope` is a Google OAuth2 API scopes that require privileged
// access - these scopes are accessible by consumers listed in
// `GetPrivilegedOAuth2Consumers()`.
bool IsPrivilegedOAuth2Scopes(const std::string& scope) {
  return GaiaConstants::kAnyApiOAuth2Scope == scope;
}

}  // namespace

OAuth2ScopeRestriction GetOAuth2ScopeRestriction(const std::string& scope) {
  if (IsUnrestrictedOAuth2Scopes(scope)) {
    return OAuth2ScopeRestriction::kNoRestriction;
  }

  if (IsPrivilegedOAuth2Scopes(scope)) {
    return OAuth2ScopeRestriction::kPrivilegedOAuth2Consumer;
  }

  // By default, OAuth2 access token requires the user to be signed in.
  return OAuth2ScopeRestriction::kSignedIn;
}

bool IsPrivilegedOAuth2Consumer(OAuthConsumerId oauth_consumer_id) {
  return oauth_consumer_id == OAuthConsumerId::kExtensionsIdentityAPI;
}

bool IsConsumerAllowlistedForScope(OAuthConsumerId oauth_consumer_id,
                                   const std::string& scope) {
  static constexpr auto kAllowlist =
      base::MakeFixedFlatSet<std::pair<OAuthConsumerId, std::string_view>>({
          {OAuthConsumerId::kSyncDeviceStatisticsMetrics,
           GaiaConstants::kChromeSyncOAuth2Scope},
      });

  return kAllowlist.contains({oauth_consumer_id, scope});
}

}  // namespace signin
