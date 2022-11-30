// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/android/sql_handler.h"

namespace history {

TableIDRow::TableIDRow()
    : url_id(0),
      bookmarked(false) {
}

TableIDRow::~TableIDRow() {
}

SQLHandler::SQLHandler(const HistoryAndBookmarkRow::ColumnID columns[],
                       int column_count)
    : columns_(columns, columns + column_count) {
}

SQLHandler::~SQLHandler() {
}

bool SQLHandler::HasColumnIn(const HistoryAndBookmarkRow& row) {
  for (std::set<HistoryAndBookmarkRow::ColumnID>::const_iterator i =
           columns_.begin(); i != columns_.end(); ++i) {
    if (row.is_value_set_explicitly(*i))
      return true;
  }
  return false;
}

bool SQLHandler::HasColumn(HistoryAndBookmarkRow::ColumnID id) {
  return columns_.find(id) != columns_.end();
}

}  // namespace history.
