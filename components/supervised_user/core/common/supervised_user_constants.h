// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_

#include "base/files/file_path.h"
#include "ui/base/page_transition_types.h"

namespace supervised_user {

// The result of local web approval flow.
// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(LocalApprovalResult)
enum class LocalApprovalResult {
  // The parent has locally approved the website.
  kApproved = 0,
  // The parent has explicitly declined the approval.
  kDeclined = 1,
  // The local web approval is canceled without user intervention.
  kCanceled = 2,
  // The local web approval is interrupted due to an error, e.g. parsing error
  // or unexpected `result` from the server.
  kError = 3,
  // Deprecated kMalformedPacpResult = 4,
  kMaxValue = kError
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyLinkUserLocalWebApprovalResult)

// Used for metrics. These values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(ParentAccessWidgetError)
enum class ParentAccessWidgetError {
  kOAuthError = 0,
  kDelegateNotAvailable = 1,
  kDecodingError = 2,
  kParsingError = 3,
  kUnknownCallback = 4,
  kMaxValue = kUnknownCallback
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:FamilyLinkUserParentAccessWidgetError)

// Type of error that was encountered during a local web approval flow.
// Used for metrics. Those values are logged to UMA. Entries should not be
// renumbered and numeric values should never be reused.
// LINT.IfChange(LocalWebApprovalErrorType)
enum class LocalWebApprovalErrorType : int {
  kFailureToDecodePacpResponse = 0,
  kFailureToParsePacpResponse = 1,
  kUnexpectedPacpResponse = 2,
  kPacpTimeoutExceeded = 3,
  kPacpEmptyResponse = 4,
  kMaxValue = kPacpEmptyResponse
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:LocalWebApprovalErrorType)

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

  // Web filter is neutralized: it behaves as if there were no filtering.
  kDisabled = 4,

  // Used for UMA. Update kMaxValue to the last value. Add future entries
  // above this comment. Sync with enums.xml.
  kMaxValue = kDisabled,
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

// Indicates why the filtering was issued.
// LINT.IfChange(top_level_filtering_context)
enum class FilteringContext : int {
  // Default setting used if filtering context is not explicitly specified
  // (eg. for tools in chrome:// internal pages).
  kDefault = 0,
  // Use for filtering triggered by content::NavigationThrottle events.
  kNavigationThrottle = 1,
  // Use for filtering triggered by content::WebContentsObserver events.
  kNavigationObserver = 2,
  // Use for filtering triggered by changes to Family Link.
  kFamilyLinkSettingsUpdated = 3
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/histograms.xml:top_level_filtering_context)

// LINT.IfChange(top_level_filtering_result)
// This enum, together with `::FilteringContext`, constitutes value for the
// `ManagedUser.TopLevelFilteringResult` histogram: value = context * spacing +
// result (spacing is 100).
enum class SupervisedUserFilterTopLevelResult : int {
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
// LINT.ThenChange(//tools/metrics/histograms/metadata/families/enums.xml:top_level_filtering_result)

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

// Histogram name to log top level URL filtering results with reason for filter
extern const char kSupervisedUserTopLevelURLFilteringResultHistogramName[];

// Histogram name to log top level URL filtering results with reason for filter,
// for use in the navigation throttle context.
extern const char kSupervisedUserTopLevelURLFilteringResult2HistogramName[];

// Histogram name to log the result of a local url approval request.
extern const char kLocalWebApprovalResultHistogramName[];

// The URL which the "Managed by your parent" UI links to.
extern const char kManagedByParentUiMoreInfoUrl[];

// The url that displays a user's Family info.
// The navigations in the via PACP widget redirect to this url.
extern const char kFamilyManagementUrl[];

// The string used to denote an account that does not have a family member role.
extern const char kDefaultEmptyFamilyMemberRole[];

// Feedback source name for family member role in Family Link.
extern const char kFamilyMemberRoleFeedbackTag[];

// Histogram name to track throttle's headroom before its decision was required.
extern const char kClassifiedEarlierThanContentResponseHistogramName[];

// Histogram name to track how much throttle delayed the navigation.
extern const char kClassifiedLaterThanContentResponseHistogramName[];

// Histogram name to track intermediate throttle states.
extern const char kClassifyUrlThrottleStatusHistogramName[];

// Histogram name to track the final throttle verdict.
extern const char kClassifyUrlThrottleFinalStatusHistogramName[];

// Histogram name to track the reason for creating a throttle.
extern const char kClassifyUrlThrottleUseCaseHistogramName[];

// Histogram name to track the duration of successful local web approval flows,
// in milliseconds.
extern const char kLocalWebApprovalDurationMillisecondsHistogramName[];

// Histogram name to track the different error types that may occur during the
// local web approval flow.
extern const char kLocalWebApprovalErrorTypeHistogramName[];
}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_CONSTANTS_H_
