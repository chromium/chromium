// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_restriction.h"

#include "base/containers/contains.h"
#include "base/containers/flat_set.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/plus_addresses/features.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin {

namespace {

// Client name for Chrome extensions that require access to Identity APIs.
const char* const kExtensionsIdentityAPIOAuthConsumerName =
    "extensions_identity_api";

// Returns true if `scope` is a Google OAuth2 API scope that do not require user
// to be signed in to the browser.
bool IsUnrestrictedOAuth2Scopes(const std::string& scope) {
  // clang-format off

  static const base::NoDestructor<base::flat_set<std::string_view>> scopes(
    {
      GaiaConstants::kGoogleUserInfoEmail,
      GaiaConstants::kGoogleUserInfoProfile,

      // Required to fetch the ManagedAccounsSigninRestriction policy during
      //sign in.
      GaiaConstants::kSecureConnectOAuth2Scope,

      // TODO(b/321900823): Fix tests and move below scopes to require the
      // browser to be signed in.

      // Required by cloud policy.
      GaiaConstants::kDeviceManagementServiceOAuth,
  });
  // clang-format on

  return scopes->contains(scope);
}

// Returns true if `scope` is a Google OAuth2 API scopes that requires the user
// to be signed in with ConsentLevel::kSignin. Sync or explicit consent is not
// required.
bool IsUnconsentedSignedInOAuth2Scopes(const std::string& scope) {
  // clang-format off
  static const base::NoDestructor<base::flat_set<std::string_view>> scopes (
    {
      GaiaConstants::kFCMOAuthScope,

      // Google Pay is accessible as it has its own consent dialogs.
      GaiaConstants::kPaymentsOAuth2Scope,

      // Required for password leak detection.
      GaiaConstants::kPasswordsLeakCheckOAuth2Scope,

      // Required by Zuul.
      GaiaConstants::kCryptAuthOAuth2Scope,

      // Required by safe browsing.
      GaiaConstants::kChromeSafeBrowsingOAuth2Scope,

      // The "ChromeSync" scope is used by Sync-the-transport, which does
      // not require consent. Instead, features built on top of it (e.g., tab
      // sharing, account-scoped passwords, or Sync-the-feature) have their own
      // in-feature consent.
      GaiaConstants::kChromeSyncOAuth2Scope,

      // Required by Permission Request Creator.
      GaiaConstants::kClassifyUrlKidPermissionOAuth2Scope,

      // Required for IP protection proxy authentication.
      GaiaConstants::kIpProtectionAuthScope,

      // Required by the feedback uploader.
      GaiaConstants::kSupportContentOAuth2Scope,

      // Required by the Google Photos NTP module.
      GaiaConstants::kPhotosModuleOAuth2Scope,
      GaiaConstants::kPhotosModuleImageOAuth2Scope,

      // Required for displaying information about parents on supervised child
      // devices.  Consent is obtained outside Chrome within Family Link flows.
      GaiaConstants::kKidFamilyReadonlyOAuth2Scope,

      // Required for requesting Discover feed with personalization without
      // sync consent. Sync consent isn't required for personalization but can
      // improve suggestions.
      GaiaConstants::kFeedOAuth2Scope,

      // Required by k-Anonymity Server (FLEDGE)
      GaiaConstants::kKAnonymityServiceOAuth2Scope,

      // Required by supervision features that verify parent password.
      GaiaConstants::kAccountsReauthOAuth2Scope,

      // Used by desktop Chrome to talk to passkey enclaves when using Google
      // Password Manager.
      GaiaConstants::kPasskeysEnclaveOAuth2Scope,

      // Required by Optimization Guide.
      GaiaConstants::kOptimizationGuideServiceGetHintsOAuth2Scope,
      GaiaConstants::kOptimizationGuideServiceModelExecutionOAuth2Scope,

      // Required by Lens.
      GaiaConstants::kLensOAuth2Scope,

      // Required by Omnibox / DocumentSuggestionsService.
      GaiaConstants::kCloudSearchQueryOAuth2Scope,

      // Used by AdvancedProtectionStatusManager, as well as internally by the
      // identity system.
      GaiaConstants::kOAuth1LoginScope,

      // Required by the Google Calendar NTP module and ChromeOS.
      GaiaConstants::kCalendarReadOnlyOAuth2Scope,

      // Used by DevTools GenAI features
      GaiaConstants::kAidaOAuth2Scope,

    // Required by ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS_ASH)
      GaiaConstants::kAssistantOAuth2Scope,
      GaiaConstants::kAuditRecordingOAuth2Scope,
      GaiaConstants::kCastBackdropOAuth2Scope,
      GaiaConstants::kClearCutOAuth2Scope,
      GaiaConstants::kDriveOAuth2Scope,
      GaiaConstants::kDriveReadOnlyOAuth2Scope,
      GaiaConstants::kExperimentsAndConfigsOAuth2Scope,
      GaiaConstants::kGCMGroupServerOAuth2Scope,
      GaiaConstants::kNearbyDevicesOAuth2Scope,
      GaiaConstants::kNearbyShareOAuth2Scope,
      GaiaConstants::kNearbyPresenceOAuth2Scope,
      GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
      GaiaConstants::kPhotosOAuth2Scope,
      GaiaConstants::kTachyonOAuthScope,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
      // clang-format on
  });

  std::string plus_address_scope =
      plus_addresses::features::kEnterprisePlusAddressOAuthScope.Get();
  return scopes->contains(scope) ||
         (!plus_address_scope.empty() && plus_address_scope == scope);
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

  if (IsUnconsentedSignedInOAuth2Scopes(scope)) {
    return OAuth2ScopeRestriction::kSignedIn;
  }

  if (IsPrivilegedOAuth2Scopes(scope)) {
    return OAuth2ScopeRestriction::kPrivilegedOAuth2Consumer;
  }

  // By default, OAuth2 access token requires explicit consent.
  return OAuth2ScopeRestriction::kExplicitConsent;
}

bool IsPrivilegedOAuth2Consumer(const std::string& consumer_name) {
  return consumer_name == kExtensionsIdentityAPIOAuthConsumerName;
}

}  // namespace signin
