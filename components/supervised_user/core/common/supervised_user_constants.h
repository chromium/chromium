// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "ui/base/page_transition_types.h"

namespace supervised_user {

// This enum describes the filter types of Chrome, which is
// set by Family Link App or at families.google.com/families. These values
// are logged to UMA. Entries should not be renumbered and numeric values
// should never be reused. Please keep in sync with "FamilyLinkWebFilterType"
// in src/tools/metrics/histograms/enums.xml.
enum class WebFilterType {
  // The web filter is set to "Allow all sites".
  kAllowAllSites = 0,

  // The web filter is set to "Try to block mature sites".
  kTryToBlockMatureSites = 1,

  // The web filter is set to "Only allow certain sites".
  kCertainSites = 2,

  // Used for UMA only. There are multiple web filters on the device.
  kMixed = 3,

  // Used for UMA. Update kMaxValue to the last value. Add future entries
  // above this comment. Sync with enums.xml.
  kMaxValue = kMixed,
};

// Returns the string equivalent of a Web Filter type. This is a user-visible
// string included in the user feedback log.
std::string WebFilterTypeToDisplayString(WebFilterType web_filter_type);

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(ToggleState)
enum class ToggleState {
  kDisabled = 0,
  kEnabled = 1,
  kMixed = 2,
  kMaxValue = kMixed,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:SupervisedUserToggleState)

// These values corresponds to SupervisedUserSafetyFilterResult in
// tools/metrics/histograms/enums.xml. If you change anything here, make
// sure to also update enums.xml accordingly.
enum SupervisedUserSafetyFilterResult {
  FILTERING_BEHAVIOR_ALLOW = 1,
  FILTERING_BEHAVIOR_ALLOW_UNCERTAIN = 2,
  FILTERING_BEHAVIOR_BLOCK_DENYLIST = 3,  // deprecated
  FILTERING_BEHAVIOR_BLOCK_SAFESITES = 4,
  FILTERING_BEHAVIOR_BLOCK_MANUAL = 5,
  FILTERING_BEHAVIOR_BLOCK_DEFAULT = 6,
  FILTERING_BEHAVIOR_ALLOW_ALLOWLIST = 7,  // deprecated
  FILTERING_BEHAVIOR_MAX = FILTERING_BEHAVIOR_ALLOW_ALLOWLIST
};

// These enum values describe the result of filtering and are logged to UMA.
// Please keep in sync with "SupervisedUserFilterTopLevelResult" in
// tools/metrics/histograms/enums.xml.
enum class SupervisedUserFilterTopLevelResult {
  // A parent has explicitly allowed the domain on the allowlist or all sites
  // are allowed through parental controls.
  kAllow = 0,
  // Site is blocked by the safe sites filter
  kBlockSafeSites = 1,
  // Sites that were blocked due to being on the blocklist
  kBlockManual = 2,
  // Sites are blocked by default when the "Only allow certain sites" setting is
  // enabled for the supervised user. Sites on the allowlist are not blocked.
  kBlockNotInAllowlist = 3,
};

// Constants used by SupervisedUserURLFilter::RecordFilterResultEvent.
extern const int kHistogramFilteringBehaviorSpacing;
extern const int kSupervisedUserURLFilteringResultHistogramMax;

int GetHistogramValueForTransitionType(ui::PageTransition transition_type);

// Keys for supervised user settings. These are configured remotely and mapped
// to preferences by the SupervisedUserPrefStore.
extern const char kAuthorizationHeader[];
extern const char kCameraMicDisabled[];
extern const char kContentPackDefaultFilteringBehavior[];
extern const char kContentPackManualBehaviorHosts[];
extern const char kContentPackManualBehaviorURLs[];
extern const char kCookiesAlwaysAllowed[];
extern const char kGeolocationDisabled[];
extern const char kSafeSitesEnabled[];
extern const char kSigninAllowed[];
extern const char kSigninAllowedOnNextStartup[];
extern const char kSkipParentApprovalToInstallExtensions[];

// A special supervised user ID used for child accounts.
extern const char kChildAccountSUID[];

// Keys for supervised user shared settings. These can be configured remotely or
// SupervisedUserPrefMappingService.
extern const char kChromeAvatarIndex[];
extern const char kChromeOSAvatarIndex[];
extern const char kChromeOSPasswordData[];

// A group of preferences of both primary and secondary custodians.
extern const char* const kCustodianInfoPrefs[10];

// Filenames.
extern const base::FilePath::CharType kSupervisedUserSettingsFilename[];

extern const char kSyncGoogleDashboardURL[];

// Histogram name to log FamilyLink user type segmentation.
extern const char kFamilyLinkUserLogSegmentHistogramName[];

// Histogram name to log Family Link user web filter type segmentation.
// This filter only applies to supervised user accounts.
extern const char kFamilyLinkUserLogSegmentWebFilterHistogramName[];

// Histogram name to log Family Link site permissions toggle state.
extern const char kSitesMayRequestCameraMicLocationHistogramName[];

// Histogram name to log Family Link extensions permissions toggle state.
extern const char kSkipParentApprovalToInstallExtensionsHistogramName[];

// Histogram name to log URL filtering results with reason for filter and page
// transition.
extern const char kSupervisedUserURLFilteringResultHistogramName[];

// Histogram name to log top level URL filtering results with reason for filter.
extern const char kSupervisedUserTopLevelURLFilteringResultHistogramName[];

// The URL which the "Managed by your parent" UI links to.
extern const char kManagedByParentUiMoreInfoUrl[];

// The string used to denote an account that does not have a family member role.
extern const char kDefaultEmptyFamilyMemberRole[];

// Feedback source name for family member role in Family Link.
extern const char kFamilyMemberRoleFeedbackTag[];

// Histogram name for the ::ClassifyUrlLoaderThrottle
extern const char kClassifiedEarlierThanContentResponseHistogramName[];
extern const char kClassifiedLaterThanContentResponseHistogramName[];
extern const char kClassifyUrlThrottleStatusHistogramName[];
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
