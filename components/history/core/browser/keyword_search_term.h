// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_

#include <optional>
#include <string>

#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_row.h"
#include "sql/statement.h"

namespace history {

// Represents one or more visits to a keyword search term. It contains the
// search term and the normalized search term in addition to the visit count and
// the last visit time. An optional frecency score may be provided by the
// utility functions/helpers in keyword_search_term_util.h where applicable.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit() = default;
  KeywordSearchTermVisit(const KeywordSearchTermVisit&) = delete;
  KeywordSearchTermVisit& operator=(const KeywordSearchTermVisit&) = delete;
  ~KeywordSearchTermVisit() = default;

  std::u16string term;             // The search term that was used.
  std::u16string normalized_term;  // The search term, in lower case and with
                                   // extra whitespace characters collapsed.
  int visit_count{0};              // The search term visit count.
  base::Time last_visit_time;      // The time of the last visit.
  std::optional<double> score;     // The optional calculated frecency score.
};

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow() = default;
  KeywordSearchTermRow(KeywordSearchTermRow&& other) = default;
  KeywordSearchTermRow& operator=(KeywordSearchTermRow&& other) = default;

  KeywordID keyword_id{0};         // ID of the keyword.
  URLID url_id{0};                 // ID of the url.
  std::u16string term;             // The search term that was used.
  std::u16string normalized_term;  // The search term, in lower case and with
                                   // extra whitespace characters collapsed.
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
