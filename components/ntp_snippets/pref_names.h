// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
#define COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_

namespace ntp_snippets {
namespace prefs {

// If set to false, remote suggestions are completely disabled. This is set by
// an enterprise policy.
extern const char kEnableSnippets[];

// Whether the list of NTP snippets is visible in UI. This is set to false when
// the user toggles the list off.
extern const char kArticlesListVisible[];

// The pref name under which remote suggestion categories (including their ID
// and title) are stored.
extern const char kRemoteSuggestionCategories[];

// The pref name for the last time when a background fetch was attempted.
extern const char kSnippetLastFetchAttemptTime[];

// The pref name for the last time when a background fetch was successfull.
extern const char kSnippetLastSuccessfulFetchTime[];

// Pref names for minimal intervals between two successive background fetches.
//
// The prefs store *currently applied* minimal intervals. For each trigger type
// there are two intervals stored in prefs:
//  - "Wifi" for situations with Wifi / unmetered network connectivity, and
//  - "Fallback" for situations with only non-Wifi / metered network.
// We check "meteredness" of the current network only on platforms that support
// that, notably Android; we use WiFi as the proxy of being unmetered elsewhere.
//
// Intervals for trigger type 1: wake-up of the persistent scheduler.
extern const char kSnippetPersistentFetchingIntervalWifi[];
extern const char kSnippetPersistentFetchingIntervalFallback[];
// Intervals for trigger type 2: browser started-up (both cold and warm start).
extern const char kSnippetStartupFetchingIntervalWifi[];
extern const char kSnippetStartupFetchingIntervalFallback[];
// Intervals for trigger type 3: suggestions shown to the user.
extern const char kSnippetShownFetchingIntervalWifi[];
extern const char kSnippetShownFetchingIntervalFallback[];

// The pref name for today's count of RemoteSuggestionsFetcher requests, so far.
extern const char kSnippetFetcherRequestCount[];
// The pref name for today's count of RemoteSuggestionsFetcher interactive
// requests.
extern const char kSnippetFetcherInteractiveRequestCount[];
// The pref name for the current day for the counter of RemoteSuggestionsFetcher
// requests.
extern const char kSnippetFetcherRequestsDay[];

// The pref name for today's count of requests for article thumbnails, so far.
extern const char kSnippetThumbnailsRequestCount[];
// The pref name for today's count of interactive requests for article
// thumbnails, so far.
extern const char kSnippetThumbnailsInteractiveRequestCount[];
// The pref name for the current day for the counter of requests for article
// thumbnails.
extern const char kSnippetThumbnailsRequestsDay[];

// The pref name for the time of the last successful background fetch.
extern const char kLastSuccessfulBackgroundFetchTime[];

extern const char kDismissedCategories[];

// The pref name for the discounted average number of browsing sessions per hour
// that involve opening a new NTP.
extern const char kUserClassifierAverageNTPOpenedPerHour[];
// The pref name for the discounted average number of browsing sessions per hour
// that involve opening showing the content suggestions.
extern const char kUserClassifierAverageSuggestionsShownPerHour[];
// The pref name for the discounted average number of browsing sessions per hour
// that involve using content suggestions (i.e. opening one or clicking on the
// "More" button).
extern const char kUserClassifierAverageSuggestionsUsedPerHour[];

// The pref name for the last time a new NTP was opened.
extern const char kUserClassifierLastTimeToOpenNTP[];
// The pref name for the last time content suggestions were shown to the user.
extern const char kUserClassifierLastTimeToShowSuggestions[];
// The pref name for the last time content suggestions were used by the user.
extern const char kUserClassifierLastTimeToUseSuggestions[];

// The pref name for the current order of categories and their clicks.
extern const char kClickBasedCategoryRankerOrderWithClicks[];
// The pref name for the time when last click decay has happened.
extern const char kClickBasedCategoryRankerLastDecayTime[];

}  // namespace prefs
}  // namespace ntp_snippets

#endif  // COMPONENTS_NTP_SNIPPETS_PREF_NAMES_H_
