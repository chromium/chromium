// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history/core/browser/android/android_history_types.h"

#include <stddef.h>

#include "sql/statement.h"

namespace history {

namespace {
// The column name defined in android.provider.Browser.BookmarkColumns
const char* const kAndroidBookmarkColumn[] = {
    "_id",
    "url",
    "title",
    "created",
    "date",
    "visits",
    "favicon",
    "bookmark",
    "raw_url",
};

// The column name defined in android.provider.Browser.SearchColumns
const char* const kAndroidSearchColumn[] = {
    "_id",
    "search",
    "date",
};

class BookmarkIDMapping
    : public std::map<std::string, HistoryAndBookmarkRow::ColumnID> {
 public:
  BookmarkIDMapping() {
    static_assert(
        std::size(kAndroidBookmarkColumn) <= HistoryAndBookmarkRow::COLUMN_END,
        "kAndroidBookmarkColumn should not have more than COLUMN_END elements");
    for (size_t i = 0; i < std::size(kAndroidBookmarkColumn); ++i) {
      (*this)[kAndroidBookmarkColumn[i]] =
          static_cast<HistoryAndBookmarkRow::ColumnID>(i);
    }
  }
};

// The mapping from Android column name to ColumnID; It is initialized
// once it used.
BookmarkIDMapping* g_bookmark_id_mapping = NULL;

class SearchIDMapping : public std::map<std::string, SearchRow::ColumnID> {
 public:
  SearchIDMapping() {
    static_assert(std::size(kAndroidSearchColumn) <= SearchRow::COLUMN_END,
                  "kAndroidSearchColumn should not have more than "
                  "COLUMN_END elements");
    for (size_t i = 0; i < std::size(kAndroidSearchColumn); ++i) {
      (*this)[kAndroidSearchColumn[i]] = static_cast<SearchRow::ColumnID>(i);
    }
  }
};

// The mapping from Android column name to ColumnID; It is initialized
// once it used.
SearchIDMapping* g_search_id_mapping = NULL;

}  // namespace

HistoryAndBookmarkRow::HistoryAndBookmarkRow()
    : id_(0),
      created_(base::Time()),
      last_visit_time_(base::Time()),
      visit_count_(0),
      is_bookmark_(false),
      parent_id_(0),
      url_id_(0) {
}

HistoryAndBookmarkRow::HistoryAndBookmarkRow(
    const HistoryAndBookmarkRow& other) = default;

HistoryAndBookmarkRow::~HistoryAndBookmarkRow() {
}

std::string HistoryAndBookmarkRow::GetAndroidName(ColumnID id) {
  return kAndroidBookmarkColumn[id];
}

HistoryAndBookmarkRow::ColumnID HistoryAndBookmarkRow::GetColumnID(
    const std::string& name) {
  if (!g_bookmark_id_mapping)
    g_bookmark_id_mapping = new BookmarkIDMapping();

  BookmarkIDMapping::const_iterator i = g_bookmark_id_mapping->find(name);
  if (i == g_bookmark_id_mapping->end())
    return HistoryAndBookmarkRow::COLUMN_END;
  return i->second;
}

SearchRow::SearchRow() : id_(0), keyword_id_(0) {
}

SearchRow::SearchRow(const SearchRow& other) = default;

SearchRow::~SearchRow() {
}

std::string SearchRow::GetAndroidName(ColumnID id) {
  return kAndroidSearchColumn[id];
}

SearchRow::ColumnID SearchRow::GetColumnID(const std::string& name) {
  if (!g_search_id_mapping)
    g_search_id_mapping = new SearchIDMapping();

  SearchIDMapping::const_iterator i = g_search_id_mapping->find(name);
  if (i == g_search_id_mapping->end())
    return SearchRow::COLUMN_END;
  return i->second;
}

AndroidURLRow::AndroidURLRow() : id(0), url_id(0) {
}

AndroidURLRow::~AndroidURLRow() {
}

SearchTermRow::SearchTermRow() : id(0) {
}

SearchTermRow::~SearchTermRow() {
}

AndroidStatement::AndroidStatement(sql::Statement* statement, int favicon_index)
    : statement_(statement), favicon_index_(favicon_index) {
}

AndroidStatement::~AndroidStatement() {
}

}  // namespace history.
