// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_HISTORY_TYPES_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_HISTORY_TYPES_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/keyword_id.h"

namespace sql {
class Statement;
}

namespace history {

typedef int64_t AndroidURLID;
typedef int64_t SearchTermID;

// Wraps all columns needed to support android.provider.Browser.BookmarkColumns.
// It is used in insert() and update() to specify the columns need to insert or
// update.
// The column is not valid until it set. Using is_valid() to find out whether
// the specific column could be used.
//
// The defult copy constructor is used.
class HistoryAndBookmarkRow {
 public:
  enum ColumnID {
    ID,
    URL,
    TITLE,
    CREATED,
    LAST_VISIT_TIME,
    VISIT_COUNT,
    FAVICON,
    BOOKMARK,
    RAW_URL,
    PARENT_ID,
    URL_ID,
    COLUMN_END  // This must be the last.
  };

  HistoryAndBookmarkRow();
  HistoryAndBookmarkRow(const HistoryAndBookmarkRow& other);
  virtual ~HistoryAndBookmarkRow();

  // Returns the column name defined in Android.
  static std::string GetAndroidName(ColumnID id);

  static ColumnID GetColumnID(const std::string& name);

  // URLs for the page.
  void set_url(const GURL& url) {
    set_value_explicitly(URL);
    url_ = url;
  }
  const GURL& url() const { return url_; }

  // Raw input URL
  void set_raw_url(const std::string& raw_url) {
    set_value_explicitly(RAW_URL);
    raw_url_ = raw_url;
  }
  const std::string& raw_url() const { return raw_url_; }

  // The title of page.
  void set_title(const std::u16string& title) {
    set_value_explicitly(TITLE);
    title_ = title;
  }
  const std::u16string& title() const { return title_; }

  // The page's first visit time.
  void set_created(const base::Time created) {
    set_value_explicitly(CREATED);
    created_ = created;
  }
  const base::Time& created() const { return created_; }

  // The page's last visit time.
  void set_last_visit_time(const base::Time last_visit_time) {
    set_value_explicitly(LAST_VISIT_TIME);
    last_visit_time_ = last_visit_time;
  }
  const base::Time& last_visit_time() const { return last_visit_time_; }

  // The visit times
  void set_visit_count(int visit_count) {
    set_value_explicitly(VISIT_COUNT);
    visit_count_ = visit_count;
  }
  int visit_count() const { return visit_count_; }

  // Whether the page is bookmarked.
  void set_is_bookmark(bool is_bookmark) {
    set_value_explicitly(BOOKMARK);
    is_bookmark_ = is_bookmark;
  }
  bool is_bookmark() const { return is_bookmark_; }

  // The favicon related to page if any.
  void set_favicon(const scoped_refptr<base::RefCountedMemory>& data) {
    set_value_explicitly(FAVICON);
    favicon_ = data;
  }
  const scoped_refptr<base::RefCountedMemory>& favicon() const {
    return favicon_;
  }

  bool favicon_valid() const { return favicon_.get() && favicon_->size(); }

  // The id of android url.
  void set_id(AndroidURLID id) {
    set_value_explicitly(ID);
    id_ = id;
  }
  AndroidURLID id() const { return id_; }

  // The id of the parent folder containing the bookmark, if any.
  void set_parent_id(int64_t parent_id) {
    set_value_explicitly(PARENT_ID);
    parent_id_ = parent_id;
  }
  int64_t parent_id() const { return parent_id_; }

  // The internal URLID
  void set_url_id(URLID url_id) {
    set_value_explicitly(URL_ID);
    url_id_ = url_id;
  }
  URLID url_id() const { return url_id_; }

  // Returns true if the given `id` has been set explicitly.
  bool is_value_set_explicitly(ColumnID id) const {
    return values_set_.find(id) != values_set_.end();
  }

 private:
  void set_value_explicitly(ColumnID id) { values_set_.insert(id); }

  AndroidURLID id_;
  GURL url_;
  std::string raw_url_;
  std::u16string title_;
  base::Time created_;
  base::Time last_visit_time_;
  scoped_refptr<base::RefCountedMemory> favicon_;
  int visit_count_;
  bool is_bookmark_;
  int64_t parent_id_;
  URLID url_id_;

  // Used to find whether a column has been set a value explicitly.
  std::set<ColumnID> values_set_;

  // We support the implicit copy constuctor and operator=.
};

// Wraps all columns needed to support android.provider.Browser.SearchColumns.
// It is used in insert() and update() to specify the columns need to insert or
// update.
//
// The column is not valid until it set. Using is_valid() to find out whether
// the specific column could be used.
//
// The defult copy constructor is used.
class SearchRow {
 public:
  enum ColumnID { ID, SEARCH_TERM, SEARCH_TIME, URL, KEYWORD_ID, COLUMN_END };

  SearchRow();
  SearchRow(const SearchRow& other);
  virtual ~SearchRow();

  // Returns the column name defined in Android.
  static std::string GetAndroidName(ColumnID id);

  static ColumnID GetColumnID(const std::string& name);

  SearchTermID id() const { return id_; }
  void set_id(SearchTermID id) {
    set_value_explicitly(SearchRow::ID);
    id_ = id;
  }

  const std::u16string& search_term() const { return search_term_; }
  void set_search_term(const std::u16string& search_term) {
    set_value_explicitly(SearchRow::SEARCH_TERM);
    search_term_ = search_term;
  }

  const base::Time search_time() const { return search_time_; }
  void set_search_time(const base::Time& time) {
    set_value_explicitly(SearchRow::SEARCH_TIME);
    search_time_ = time;
  }

  const GURL& url() const { return url_; }
  void set_url(const GURL& url) {
    set_value_explicitly(SearchRow::URL);
    url_ = url;
  }

  KeywordID keyword_id() const { return keyword_id_; }
  void set_keyword_id(KeywordID keyword_id) {
    set_value_explicitly(SearchRow::KEYWORD_ID);
    keyword_id_ = keyword_id;
  }

  // Returns true if the given `id` has been set explicitly.
  bool is_value_set_explicitly(ColumnID id) const {
    return values_set_.find(id) != values_set_.end();
  }

 private:
  void set_value_explicitly(ColumnID id) { values_set_.insert(id); }

  SearchTermID id_;
  std::u16string search_term_;
  base::Time search_time_;
  GURL url_;
  KeywordID keyword_id_;

  // Used to find whether a column has been set a value.
  std::set<ColumnID> values_set_;

  // We support the implicit copy constuctor and operator=.
};

// Defines the row stored in android_urls table.
struct AndroidURLRow {
  AndroidURLRow();
  ~AndroidURLRow();

  // The unique id of the row
  AndroidURLID id;
  // The corresponding URLID in the url table.
  URLID url_id;
  // The orignal URL string passed in by client.
  std::string raw_url;
};

// Defines the row of keyword_cache table.
struct SearchTermRow {
  SearchTermRow();
  ~SearchTermRow();

  // The unique id of the row.
  SearchTermID id;
  // The keyword.
  std::u16string term;
  // The last visit time.
  base::Time last_visit_time;
};

// This class wraps the sql statement and favicon column index in statement if
// any. It is returned by AndroidProviderBackend::Query().
//
// Using favicon_index() to get the index of favicon; The value of that column
// is the Favicon ID, Client should call HistoryService::GetFavicon() to get the
// actual value.
class AndroidStatement {
 public:
  AndroidStatement(sql::Statement* statement, int favicon_index);

  AndroidStatement(const AndroidStatement&) = delete;
  AndroidStatement& operator=(const AndroidStatement&) = delete;

  ~AndroidStatement();

  sql::Statement* statement() { return statement_.get(); }

  // The favicon index in statement; -1 is returned if favicon is not in
  // the statement.
  int favicon_index() const { return favicon_index_; }

 private:
  std::unique_ptr<sql::Statement> statement_;
  int favicon_index_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_HISTORY_TYPES_H_
