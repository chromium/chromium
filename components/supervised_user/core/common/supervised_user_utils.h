// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
#define COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_

#include <string>

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

// Converts FilteringBehaviorReason enum to string format.
std::string FilteringBehaviorReasonToString(FilteringBehaviorReason reason);

// Strips user-specific tokens in a URL to generalize it.
GURL NormalizeUrl(const GURL& url);

// Check if web filtering prefs are set to default values.
bool AreWebFilterPrefsDefault(const PrefService& pref_service);

}  // namespace supervised_user

#endif  // COMPONENTS_SUPERVISED_USER_CORE_COMMON_SUPERVISED_USER_UTILS_H_
