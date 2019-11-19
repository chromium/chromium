// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "components/history/core/browser/keyword_id.h"
#include "components/history/core/browser/url_row.h"

namespace history {

// KeywordSearchTermVisit is returned from GetMostRecentKeywordSearchTerms. It
// gives the time and search term of the keyword visit.
struct KeywordSearchTermVisit {
  KeywordSearchTermVisit();
  ~KeywordSearchTermVisit();

  base::string16 term;  // The search term that was used.
  base::string16 normalized_term;  // The search term, in lower case and with
                                   // extra whitespaces collapsed.
  int visits;  // The visit count.
  base::Time time;  // The time of the most recent visit.
};

// Used for URLs that have a search term associated with them.
struct KeywordSearchTermRow {
  KeywordSearchTermRow();
  KeywordSearchTermRow(const KeywordSearchTermRow& other);
  ~KeywordSearchTermRow();

  KeywordID keyword_id;  // ID of the keyword.
  URLID url_id;  // ID of the url.
  base::string16 term;  // The search term that was used.
  base::string16 normalized_term;  // The search term, in lower case and with
                                   // extra whitespaces collapsed.
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_KEYWORD_SEARCH_TERM_H_
