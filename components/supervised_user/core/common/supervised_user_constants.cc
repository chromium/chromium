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

std::string WebFilterTypeToDisplayString(WebFilterType web_filter_type) {
  switch (web_filter_type) {
    case WebFilterType::kAllowAllSites:
      return "allow_all_sites";
    case WebFilterType::kCertainSites:
      return "allow_certain_sites";
    case WebFilterType::kTryToBlockMatureSites:
      return "block_mature_sites";
    case WebFilterType::kMixed:
      NOTREACHED();
  }
}

int GetHistogramValueForTransitionType(ui::PageTransition transition_type) {
  int value =
      static_cast<int>(ui::PageTransitionStripQualifier(transition_type));
  if (0 <= value && value <= kHistogramPageTransitionMaxKnownValue) {
    return value;
  }
  NOTREACHED_IN_MIGRATION();
  return kHistogramPageTransitionFallbackValue;
}

const char kAuthorizationHeader[] = "Bearer";
const char kCameraMicDisabled[] = "CameraMicDisabled";
const char kContentPackDefaultFilteringBehavior[] =
    "ContentPackDefaultFilteringBehavior";
const char kContentPackManualBehaviorHosts[] = "ContentPackManualBehaviorHosts";
const char kContentPackManualBehaviorURLs[] = "ContentPackManualBehaviorURLs";
const char kCookiesAlwaysAllowed[] = "CookiesAlwaysAllowed";
const char kGeolocationDisabled[] = "GeolocationDisabled";
const char kSafeSitesEnabled[] = "SafeSites";
const char kSigninAllowed[] = "SigninAllowed";
const char kSigninAllowedOnNextStartup[] = "kSigninAllowedOnNextStartup";
const char kSkipParentApprovalToInstallExtensions[] =
    "SkipParentApprovalToInstallExtensions";

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

const char kFamilyLinkUserLogSegmentHistogramName[] =
    "FamilyLinkUser.LogSegment";

const char kFamilyLinkUserLogSegmentWebFilterHistogramName[] =
    "FamilyUser.WebFilterType.PerRecord";

extern const char kSitesMayRequestCameraMicLocationHistogramName[] =
    "SupervisedUsers.SitesMayRequestCameraMicLocation.PerRecord";

const char kSkipParentApprovalToInstallExtensionsHistogramName[] =
    "SupervisedUsers.SkipParentApprovalToInstallExtensions.PerRecord";

const char kSupervisedUserURLFilteringResultHistogramName[] =
    "ManagedUsers.FilteringResult";

const char kSupervisedUserTopLevelURLFilteringResultHistogramName[] =
    "ManagedUsers.TopLevelFilteringResult";

const char kManagedByParentUiMoreInfoUrl[] =
    "https://familylink.google.com/setting/resource/94";

const char kDefaultEmptyFamilyMemberRole[] = "not_in_family";

// LINT.IfChange
const char kFamilyMemberRoleFeedbackTag[] = "Family_Member_Role";
// LINT.ThenChange(//chrome/browser/feedback/android/java/src/org/chromium/chrome/browser/feedback/FamilyInfoFeedbackSource.java)

const char kClassifiedEarlierThanContentResponseHistogramName[] =
    "SupervisedUsers.ClassifyUrlThrottle.EarlierThanContentResponse";
const char kClassifiedLaterThanContentResponseHistogramName[] =
    "SupervisedUsers.ClassifyUrlThrottle.LaterThanContentResponse";
extern const char kClassifyUrlThrottleStatusHistogramName[] =
    "SupervisedUsers.ClassifyUrlThrottle.Status";

}  // namespace supervised_user
