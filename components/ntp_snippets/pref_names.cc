// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ntp_snippets/pref_names.h"

namespace ntp_snippets {
namespace prefs {

const char kEnableSnippets[] = "ntp_snippets.enable";

const char kArticlesListVisible[] = "ntp_snippets.list_visible";

const char kRemoteSuggestionCategories[] = "ntp_snippets.remote_categories";

const char kSnippetLastFetchAttemptTime[] = "ntp_snippets.last_fetch_attempt";

const char kSnippetLastSuccessfulFetchTime[] =
    "ntp_snippets.last_successful_fetch_time";

// For backwards compatibility, we do not rename the "fetching_" prefs (should
// be "persistent_fetching_").
const char kSnippetPersistentFetchingIntervalWifi[] =
    "ntp_snippets.fetching_interval_wifi";
const char kSnippetPersistentFetchingIntervalFallback[] =
    "ntp_snippets.fetching_interval_fallback";

const char kSnippetStartupFetchingIntervalWifi[] =
    "ntp_snippets.startup_fetching_interval_wifi";
const char kSnippetStartupFetchingIntervalFallback[] =
    "ntp_snippets.startup_fetching_interval_fallback";

// For backwards compatibility, we do not rename the "soft_fetching_" prefs
// (should be "shown_fetching_").
const char kSnippetShownFetchingIntervalWifi[] =
    "ntp_snippets.soft_fetching_interval_wifi";
const char kSnippetShownFetchingIntervalFallback[] =
    "ntp_snippets.soft_fetching_interval_fallback";

const char kSnippetFetcherRequestCount[] =
    "ntp.request_throttler.suggestion_fetcher.count";
const char kSnippetFetcherInteractiveRequestCount[] =
    "ntp.request_throttler.suggestion_fetcher.interactive_count";
const char kSnippetFetcherRequestsDay[] =
    "ntp.request_throttler.suggestion_fetcher.day";

const char kSnippetThumbnailsRequestCount[] =
    "ntp.request_throttler.suggestion_thumbnails.count";
const char kSnippetThumbnailsInteractiveRequestCount[] =
    "ntp.request_throttler.suggestion_thumbnails.interactive_count";
const char kSnippetThumbnailsRequestsDay[] =
    "ntp.request_throttler.suggestion_thumbnails.day";

const char kDismissedCategories[] = "ntp_suggestions.dismissed_categories";

const char kLastSuccessfulBackgroundFetchTime[] =
    "ntp_suggestions.remote.last_successful_background_fetch_time";

const char kUserClassifierAverageNTPOpenedPerHour[] =
    "ntp_suggestions.user_classifier.average_ntp_opened_per_hour";
const char kUserClassifierAverageSuggestionsShownPerHour[] =
    "ntp_suggestions.user_classifier.average_suggestions_shown_per_hour";
const char kUserClassifierAverageSuggestionsUsedPerHour[] =
    "ntp_suggestions.user_classifier.average_suggestions_used_per_hour";

const char kUserClassifierLastTimeToOpenNTP[] =
    "ntp_suggestions.user_classifier.last_time_to_open_ntp";
const char kUserClassifierLastTimeToShowSuggestions[] =
    "ntp_suggestions.user_classifier.last_time_to_show_suggestions";
const char kUserClassifierLastTimeToUseSuggestions[] =
    "ntp_suggestions.user_classifier.last_time_to_use_suggestions";

const char kClickBasedCategoryRankerOrderWithClicks[] =
    "ntp_suggestions.click_based_category_ranker.category_order_with_clicks";
const char kClickBasedCategoryRankerLastDecayTime[] =
    "ntp_suggestions.click_based_category_ranker.last_decay_time";

}  // namespace prefs
}  // namespace ntp_snippets
