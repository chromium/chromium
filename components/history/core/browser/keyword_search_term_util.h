// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_

#include "components/history/core/browser/history_types.h"

namespace base {
class Time;
class TimeDelta;
}  // namespace base

namespace history {

class KeywordSearchTermVisitEnumerator;

enum class SearchTermRankingPolicy {
  kRecency,  // From the most recent to the least recent.
  kFrecency  // By descending frecency score calculated by |GetFrecencyScore|.
};

// The time interval within which a duplicate query is considered invalid for
// autocomplete purposes.
// These invalid duplicates are extracted from search query URLs which are
// identical or nearly identical to the original search query URL and issued too
// closely to it, i.e., within this time interval. They are typically recorded
// as a result of back/forward navigations or user interactions in the search
// result page and are likely not newly initiated searches.
extern const base::TimeDelta kAutocompleteDuplicateVisitIntervalThreshold;

// Returns a score combining frequency and recency of the visit favoring ones
// that are more frequent and more recent (see go/local-zps-frecency-ranking).
double GetFrecencyScore(int visit_count, base::Time visit_time, base::Time now);

// Returns keyword search terms ordered by descending recency or frecency scores
// for use as prefix or zero-prefix suggestions in the omnibox respectively.
// |enumerator| enumerates keyword search term visits from the URLDatabase. It
// must return visits ordered first by |normalized_term| and then by
// |last_visit_time| in ascending order, i.e., from the oldest to the newest.
// |ignore_duplicate_visits| specifies whether duplicative visits to a search
// term should be ignored. A duplicative visit is defined as a visit to the
// same search term in an interval smaller than
// kAutocompleteDuplicateVisitIntervalThreshold. |ranking_policy| specifies
// how the returned keyword search terms should be ordered.
void GetAutocompleteSearchTermsFromEnumerator(
    KeywordSearchTermVisitEnumerator& enumerator,
    bool ignore_duplicate_visits,
    SearchTermRankingPolicy ranking_policy,
    KeywordSearchTermVisitList* search_terms);

// Returns keyword search terms ordered by descending frecency scores
// accumulated across days for use in the Most Visited tiles. |enumerator|
// enumerates keyword search term visits from the URLDatabase. It must return
// visits ordered first by |normalized_term| and then by |last_visit_time| in
// ascending order, i.e., from the oldest to the newest.
void GetMostRepeatedSearchTermsFromEnumerator(
    KeywordSearchTermVisitEnumerator& enumerator,
    KeywordSearchTermVisitList* search_terms);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_
