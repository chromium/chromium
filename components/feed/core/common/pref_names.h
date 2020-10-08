// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_
#define COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_

class PrefRegistrySimple;

namespace feed {

namespace prefs {

// The pref name for the period of time between background refreshes.
extern const char kBackgroundRefreshPeriod[];

// The pref name for the last time when a background fetch was attempted.
extern const char kLastFetchAttemptTime[];

// The pref name for today's count of RefreshThrottler requests, so far.
extern const char kThrottlerRequestCount[];
// The pref name for the current day for the counter of RefreshThrottler's
// requests.
extern const char kThrottlerRequestsDay[];

// The pref name for the discounted average number of browsing sessions per hour
// that involve opening a new NTP.
extern const char kUserClassifierAverageSuggestionsViwedPerHour[];
// The pref name for the discounted average number of browsing sessions per hour
// that involve using content suggestions (i.e. opening one or clicking on the
// "More" button).
extern const char kUserClassifierAverageSuggestionsUsedPerHour[];

// The pref name for the last time a surface was shown that displayed
// suggestions to the user.
extern const char kUserClassifierLastTimeToViewSuggestions[];
// The pref name for the last time content suggestions were used by the user.
extern const char kUserClassifierLastTimeToUseSuggestions[];

// The pref name for the feed host override.
extern const char kHostOverrideHost[];
// The pref name for the feed host override auth token.
extern const char kHostOverrideBlessNonce[];

// The pref name for the bit that determines whether the conditions are reached
// to enable the upload of click and view actions in the feed with the notice
// card when using the feature kInterestFeedConditionalClickAndViewActionUpload.
// This is for when the privacy notice card is at the second position in the
// feed. Currently only used in V1.
extern const char kHasReachedClickAndViewActionsUploadConditions[];

// The pref name for the bit that determines whether the notice card was present
// in the feed in the last fetch of content. The notice card is considered as
// present by default to make sure that the upload of click and view actions
// doesn't take place when the notice card is present but has not yet been
// detected. Currently only used in V1.
extern const char kLastFetchHadNoticeCard[];

// The following prefs are used only by v2.

// The pref name for the request throttler counts.
extern const char kThrottlerRequestCountListPrefName[];
// The pref name for the request throttler's last request time.
extern const char kThrottlerLastRequestTime[];
// The pref name for storing |DebugStreamData|.
extern const char kDebugStreamData[];
// The pref name for storing the request schedule.
extern const char kRequestSchedule[];
// The pref name for storing the persistent metrics data.
extern const char kMetricsData[];
// The pref name for storing client instance id.
extern const char kClientInstanceId[];
// The pref name for the Actions endpoint override.
extern const char kActionsEndpointOverride[];

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_COMMON_PREF_NAMES_H_
