// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_

#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_row.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace history {

// KeywordSearchTermVisit is returned from GetMostRecentKeywordSearchTerms()
// and contains either the search term and the normalized search term. It also
// contains the visit count, and the last visit time for either a single keyword
// visit or a set of keyword visits, depending on the overloaded functions it is
// returned from.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit();
  KeywordSearchTermVisit(const KeywordSearchTermVisit& other);
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
  int visit_count{0};              // The search term visit count.
  base::Time last_visit_time;      // The time of the last visit.
  absl::optional<double> score;    // The optional calculated frecency score.
};

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow() = default;
  KeywordSearchTermRow(const KeywordSearchTermRow& other) = default;
  ~KeywordSearchTermRow() = default;

  KeywordID keyword_id{0};         // ID of the keyword.
  URLID url_id{0};                 // ID of the url.
  std::u16string term;             // The search term that was used.
  std::u16string normalized_term;  // The search term, in lower case and with
                                   // extra whitespaces collapsed.
};

// KeywordSearchTermVisitEnumerator --------------------------------------------

// A basic enumerator to enumerate keyword search term visits. May be created
// and initialized by URLDatabase only.
class KeywordSearchTermVisitEnumerator {
 public:
  KeywordSearchTermVisitEnumerator(const KeywordSearchTermVisitEnumerator&) =
      delete;
  KeywordSearchTermVisitEnumerator& operator=(
      const KeywordSearchTermVisitEnumerator&) = delete;

  ~KeywordSearchTermVisitEnumerator() = default;

  // Returns the next search term visit or nullptr if no more visits are left.
  std::unique_ptr<KeywordSearchTermVisit> GetNextVisit();

 private:
  friend class URLDatabase;
  KeywordSearchTermVisitEnumerator() = default;

  sql::Statement statement_;  // The statement to create KeywordSearchTermVisit.
  bool initialized_{false};   // Whether |statement_| can be executed.
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
