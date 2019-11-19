// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_PREF_NAMES_H_
#define COMPONENTS_FEED_CORE_PREF_NAMES_H_

class PrefRegistrySimple;

namespace feed {

namespace prefs {

// The pref name for whether suggested articles is allowed at all. When false,
// all Feed Java objects will be destroyed/nulled. Typically set by policy.
extern const char kEnableSnippets[];

// The pref name for whether the suggested articles section is expanded or
// collapsed. Only when it is expanded are the articles themselves visible.
extern const char kArticlesListVisible[];

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

}  // namespace prefs

void RegisterProfilePrefs(PrefRegistrySimple* registry);

}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_PREF_NAMES_H_
