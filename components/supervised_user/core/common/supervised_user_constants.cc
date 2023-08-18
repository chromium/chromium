// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/common/supervised_user_constants.h"

#include "base/notreached.h"
#include "components/supervised_user/core/common/pref_names.h"

namespace supervised_user {

const int kHistogramFilteringBehaviorSpacing = 100;
const int kSupervisedUserURLFilteringResultHistogramMax = 800;

namespace {

GURL KidsManagementBaseURL() {
  return GURL("https://kidsmanagement-pa.googleapis.com/kidsmanagement/v1/");
}

const char kGetFamilyProfileURL[] = "families/mine?alt=json";
const char kGetFamilyMembersURL[] = "families/mine/members?alt=json";
const char kPermissionRequestsURL[] = "people/me/permissionRequests";
const char kClassifyURLRequestURL[] = "people/me:classifyUrl";

const int kHistogramPageTransitionMaxKnownValue =
    static_cast<int>(ui::PAGE_TRANSITION_KEYWORD_GENERATED);
const int kHistogramPageTransitionFallbackValue =
    kHistogramFilteringBehaviorSpacing - 1;

}  // namespace

static_assert(kHistogramPageTransitionMaxKnownValue <
                  kHistogramPageTransitionFallbackValue,
              "HistogramPageTransition MaxKnownValue must be < FallbackValue");
static_assert(FILTERING_BEHAVIOR_MAX * kHistogramFilteringBehaviorSpacing +
                      kHistogramPageTransitionFallbackValue <
                  kSupervisedUserURLFilteringResultHistogramMax,
              "Invalid kSupervisedUserURLFilteringResultHistogramMax value");

int GetHistogramValueForTransitionType(ui::PageTransition transition_type) {
  int value =
      static_cast<int>(ui::PageTransitionStripQualifier(transition_type));
  if (0 <= value && value <= kHistogramPageTransitionMaxKnownValue) {
    return value;
  }
  NOTREACHED();
  return kHistogramPageTransitionFallbackValue;
}

const char kAuthorizationHeader[] = "Bearer";
const char kCameraMicDisabled[] = "CameraMicDisabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kCookiesAlwaysAllowed[] = "CookiesAlwaysAllowed";
const char kForceSafeSearch[] = "ForceSafeSearch";
const char kGeolocationDisabled[] = "GeolocationDisabled";
const char kSafeSitesEnabled[] = "SafeSites";
const char kSigninAllowed[] = "SigninAllowed";
const char kSigninAllowedOnNextStartup[] = "kSigninAllowedOnNextStartup";

const char kChildAccountSUID[] = "ChildAccountSUID";

const char kChromeAvatarIndex[] = "chrome-avatar-index";
const char kChromeOSAvatarIndex[] = "chromeos-avatar-index";

const char kChromeOSPasswordData[] = "chromeos-password-data";

const char* const kCustodianInfoPrefs[] = {
    prefs::kSupervisedUserCustodianName,
    prefs::kSupervisedUserCustodianEmail,
    prefs::kSupervisedUserCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserCustodianProfileURL,
    prefs::kSupervisedUserCustodianProfileImageURL,
    prefs::kSupervisedUserSecondCustodianName,
    prefs::kSupervisedUserSecondCustodianEmail,
    prefs::kSupervisedUserSecondCustodianObfuscatedGaiaId,
    prefs::kSupervisedUserSecondCustodianProfileURL,
    prefs::kSupervisedUserSecondCustodianProfileImageURL,
};

const base::FilePath::CharType kSupervisedUserSettingsFilename[] =
    FILE_PATH_LITERAL("Managed Mode Settings");

const char kSyncGoogleDashboardURL[] =
    "https://www.google.com/settings/chrome/sync";

GURL KidsManagementGetFamilyProfileURL() {
  return KidsManagementBaseURL().Resolve(kGetFamilyProfileURL);
}

GURL KidsManagementGetFamilyMembersURL() {
  return KidsManagementBaseURL().Resolve(kGetFamilyMembersURL);
}

GURL KidsManagementPermissionRequestsURL() {
  return KidsManagementBaseURL().Resolve(kPermissionRequestsURL);
}

GURL KidsManagementClassifyURLRequestURL() {
  return KidsManagementBaseURL().Resolve(kClassifyURLRequestURL);
}

const char kFamilyLinkUserLogSegmentHistogramName[] =
    "FamilyLinkUser.LogSegment";

const char kSupervisedUserURLFilteringResultHistogramName[] =
    "ManagedUsers.FilteringResult";

}  // namespace supervised_user
