// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_

#include <memory>
#include <vector>

namespace history {

class KeywordSearchTermVisitEnumerator;
struct KeywordSearchTermVisit;

// Returns keyword search terms ordered by descending frecency scores
// accumulated across days for use in the Most Visited tiles. |enumerator|
// enumerates keyword search term visits from the URLDatabase. It must return
// visits ordered first by |normalized_term| and then by |last_visit_time| in
// ascending order, i.e., from the oldest to the newest.
void GetMostRepeatedSearchTermsFromEnumerator(
    KeywordSearchTermVisitEnumerator& enumerator,
    std::vector<std::unique_ptr<KeywordSearchTermVisit>>* search_terms);

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_UTIL_H_
