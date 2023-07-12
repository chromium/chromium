// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/access_token_constants.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "google_apis/gaia/gaia_constants.h"

namespace signin {

namespace {

// Client name for Chrome extensions that require access to Identity APIs.
const char* const kExtensionsIdentityAPIOAuthConsumerName =
    "extensions_identity_api";

}  // namespace

const std::set<std::string> GetUnconsentedOAuth2Scopes() {
  // clang-format off
  return {
      // Used to fetch account information.
      GaiaConstants::kGoogleUserInfoEmail,
      GaiaConstants::kGoogleUserInfoProfile,

      // The "ChromeSync" scope is used by Sync-the-transport, which does
      // not require consent. Instead, features built on top of it (e.g., tab
      // sharing, account-scoped passwords, or Sync-the-feature) have their own
      // in-feature consent.
      GaiaConstants::kChromeSyncOAuth2Scope,
      GaiaConstants::kFCMOAuthScope,

      // Google Pay is accessible as it has its own consent dialogs.
      GaiaConstants::kPaymentsOAuth2Scope,

      // Required for password leak detection.
      GaiaConstants::kPasswordsLeakCheckOAuth2Scope,

      // Required by Zuul.
      GaiaConstants::kCryptAuthOAuth2Scope,

      // Required by safe browsing.
      GaiaConstants::kChromeSafeBrowsingOAuth2Scope,

      // Required by cloud policy.
      GaiaConstants::kDeviceManagementServiceOAuth,

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

      // Required to fetch the ManagedAccounsSigninRestriction policy.
      GaiaConstants::kSecureConnectOAuth2Scope,

      // Required for requesting Discover feed with personalization without
      // sync consent. Sync consent isn't required for personalization but can
      // improve suggestions.
      GaiaConstants::kFeedOAuth2Scope,

      // Required by k-Anonymity Server (FLEDGE)
      GaiaConstants::kKAnonymityServiceOAuth2Scope,

    // Required by ChromeOS only.
#if BUILDFLAG(IS_CHROMEOS_ASH)
      GaiaConstants::kAccountsReauthOAuth2Scope,
      GaiaConstants::kAssistantOAuth2Scope,
      GaiaConstants::kAuditRecordingOAuth2Scope,
      GaiaConstants::kCalendarReadOnlyOAuth2Scope,
      GaiaConstants::kCastBackdropOAuth2Scope,
      GaiaConstants::kClearCutOAuth2Scope,
      GaiaConstants::kCloudTranslationOAuth2Scope,
      GaiaConstants::kDriveOAuth2Scope,
      GaiaConstants::kDriveReadOnlyOAuth2Scope,
      GaiaConstants::kExperimentsAndConfigsOAuth2Scope,
      GaiaConstants::kGCMGroupServerOAuth2Scope,
      GaiaConstants::kCloudPlatformProjectsOAuth2Scope,
      GaiaConstants::kNearbyShareOAuth2Scope,
      GaiaConstants::kNearbyPresenceOAuth2Scope,
      GaiaConstants::kOAuth1LoginScope,
      GaiaConstants::kPeopleApiReadOnlyOAuth2Scope,
      GaiaConstants::kPhotosOAuth2Scope,
      GaiaConstants::kTachyonOAuthScope,
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  };
// clang-format on
}

const std::set<std::string> GetPrivilegedOAuth2Scopes() {
  return {
      GaiaConstants::kAnyApiOAuth2Scope,
  };
}

const std::set<std::string> GetPrivilegedOAuth2Consumers() {
  return {
      kExtensionsIdentityAPIOAuthConsumerName,
  };
}

}  // namespace signin
