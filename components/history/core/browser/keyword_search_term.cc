// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/keyword_search_term.h"

#include <cmath>

namespace history {

namespace {

// Returns a KeywordSearchTermVisit populated with the columns returned from
// |statement|. |statement| is expected to return the following columns which
// match in order and type to the fields in the KeywordSearchTermVisit less the
// score which is a calculated field.
//+----------+-----------------+-------------+-----------------+
//| term     | normalized_term | visit_count | last_visit_time |
//+----------+-----------------+-------------+-----------------+
//| string16 | string16        | int         | int64           |
//+----------+-----------------+-------------+-----------------+
std::unique_ptr<KeywordSearchTermVisit> KeywordSearchTermVisitFromStatement(
    sql::Statement& statement) {
  auto search_term = std::make_unique<KeywordSearchTermVisit>();
  search_term->term = statement.ColumnString16(0);
  search_term->normalized_term = statement.ColumnString16(1);
  search_term->visit_count = statement.ColumnInt(2);
  search_term->last_visit_time =
      base::Time::FromInternalValue(statement.ColumnInt64(3));
  return search_term;
}

}  // namespace

// KeywordSearchTermVisitEnumerator --------------------------------------------

std::unique_ptr<KeywordSearchTermVisit>
KeywordSearchTermVisitEnumerator::GetNextVisit() {
  if (initialized_ && statement_.Step()) {
    return KeywordSearchTermVisitFromStatement(statement_);
  }
  initialized_ = false;
  return nullptr;
}

}  // namespace history
