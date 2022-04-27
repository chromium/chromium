// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_

#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_row.h"

namespace history {

// KeywordSearchTermVisit is returned from GetMostRecentKeywordSearchTerms()
// and contains either the search term and the normalized search term. It also
// contains the visit count, and the last visit time for either a single keyword
// visit or a set of keyword visits, depending on the overloaded functions it is
// returned from.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit() = default;
  ~KeywordSearchTermVisit();

  // Returns the frecency score of the visit based on the following formula:
  //            (frequency ^ frequency_exponent) * recency_decay_unit_in_seconds
  // frecency = ————————————————————————————————————————————————————————————————
  //                   recency_in_seconds + recency_decay_unit_in_seconds
  // This score combines frequency and recency of the visit favoring ones that
  // are more frequent and more recent (see go/local-zps-frecency-ranking).
  // `recency_decay_unit_sec` is the number of seconds until the recency
  // component of the score decays to half. `frequency_exponent` is factor by
  // which the frequency of the visit is exponentiated.
  double GetFrecency(base::Time now,
                     int recency_decay_unit_sec,
                     double frequency_exponent) const;

  std::u16string term;             // The search term that was used.
  std::u16string normalized_term;  // The search term, in lower case and with
                                   // extra whitespaces collapsed.
  int visit_count{0};              // The visit count.
  base::Time last_visit_time;      // The time of the most recent visit.
};

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow();
  KeywordSearchTermRow(const KeywordSearchTermRow& other);
  ~KeywordSearchTermRow();

  KeywordID keyword_id;  // ID of the keyword.
  URLID url_id;  // ID of the url.
  std::u16string term;             // The search term that was used.
  std::u16string normalized_term;  // The search term, in lower case and with
                                   // extra whitespaces collapsed.
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
