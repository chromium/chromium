// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_

#include <string>
#include <vector>

#include "components/signin/public/identity_manager/account_info.h"

class GURL;
class PrefService;

namespace supervised_user {

// Reason for applying the website filtering parental control.
enum class FilteringBehaviorReason {
  DEFAULT = 0,
  ASYNC_CHECKER = 1,
  // Deprecated, DENYLIST = 2,
  MANUAL = 3,
  ALLOWLIST = 4,
  NOT_SIGNED_IN = 5,
};

// This enum describes the state of the interstitial banner that is shown for
// when previous supervised users of desktop see the interstitial for the first
// time after desktop controls are enabled.
enum class FirstTimeInterstitialBannerState : int {
  // Supervised users should see banner the next time the interstitial is
  // triggered.
  kNeedToShow = 0,

  // Banner has been shown to supervised user if needed.
  kSetupComplete = 1,

  // Banner state has not been set.
  kUnknown = 2,
};

// These enum values represent the user's supervision type and how the
// supervision has been enabled.
// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
enum class LogSegment {
  // User is not supervised by FamilyLink.
  kUnsupervised = 0,
  // User that is required to be supervised by FamilyLink due to child account
  // policies (maps to Unicorn and Griffin accounts).
  kSupervisionEnabledByPolicy = 1,
  // User that has chosen to be supervised by FamilyLink (maps to Geller
  // accounts).
  kSupervisionEnabledByUser = 2,
  // Profile contains users with multiple different supervision status
  // used only when ExtendFamilyLinkUserLogSegmentToAllPlatforms flag is
  // enabled
  kMixedProfile = 3,
  // Add future entries above this comment, in sync with
  // "FamilyLinkUserLogSegment" in src/tools/metrics/histograms/enums.xml.
  // Update kMaxValue to the last value.
  kMaxValue = kMixedProfile
};

// Converts FilteringBehaviorReason enum to string format.
std::string FilteringBehaviorReasonToString(FilteringBehaviorReason reason);

// Strips user-specific tokens in a URL to generalize it.
GURL NormalizeUrl(const GURL& url);

// Check if web filtering prefs are set to default values.
bool AreWebFilterPrefsDefault(const PrefService& pref_service);

// Categorizes the account into a FamilyLink supervision type to segment the
// Chrome user population.
// `primary_accounts` should contain only primary accounts, possibly sourced
// from multiple Chrome profiles. In the case of multiple accounts, the function
// emits a single record to signal the multi-profile state.
// Returns true if one or more histograms were emitted.
bool EmitLogSegmentHistogram(const std::vector<AccountInfo>& primary_accounts);

// Returns true if the primary account is a child account subject to parental
// controls.
bool IsSubjectToParentalControls(const PrefService* pref_service);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
