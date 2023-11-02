// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_CACHE_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_CACHE_DATABASE_H_

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/android/android_history_types.h"
#include "sql/database.h"
#include "sql/init_status.h"

namespace history {

// This database is used to support Android ContentProvider APIs.
// It will be created only when it used, and deleted by HistoryBackend when
// history system shutdown.
class AndroidCacheDatabase {
 public:
  AndroidCacheDatabase();

  AndroidCacheDatabase(const AndroidCacheDatabase&) = delete;
  AndroidCacheDatabase& operator=(const AndroidCacheDatabase&) = delete;

  virtual ~AndroidCacheDatabase();

  // Creates the database, deletes existing one if any; also attach it to the
  // database returned by GetDB(). Returns sql::INIT_OK on success, otherwise
  // sql::INIT_FAILURE returned.
  sql::InitStatus InitAndroidCacheDatabase(const base::FilePath& db_name);

  // The bookmark_cache table ------------------------------------------------
  //
  // Adds a row to the bookmark_cache table. Returns true on success.
  bool AddBookmarkCacheRow(const base::Time& created_time,
                           const base::Time& last_visit_time,
                           URLID url_id);

  // Clears all rows in the bookmark_cache table; returns true on success.
  bool ClearAllBookmarkCache();

  // Marks the given `url_ids` as bookmarked; Returns true on success.
  bool MarkURLsAsBookmarked(const std::vector<URLID>& url_id);

  // Set the given `url_id`'s favicon column to `favicon_id`. Returns true on
  // success.
  bool SetFaviconID(URLID url_id, favicon_base::FaviconID favicon_id);

  // The search_terms table -------------------------------------------------
  //
  // Add a row in the search_term table with the given `term` and
  // `last_visit_time`. Return the new row's id on success, otherwise 0 is
  // returned.
  SearchTermID AddSearchTerm(const std::u16string& term,
                             const base::Time& last_visit_time);

  // Updates the `id`'s row with the given `row`; returns true on success.
  bool UpdateSearchTerm(SearchTermID id, const SearchTermRow& row);

  // Get SearchTermRow of the given `term`; return the row id on success.
  // otherwise 0 is returned.
  // The found row is return in `row` if it is not NULL.
  SearchTermID GetSearchTerm(const std::u16string& term, SearchTermRow* row);

  // Delete the search terms which don't exist in keyword_search_terms table.
  bool DeleteUnusedSearchTerms();

 protected:
  // Returns the database for the functions in this interface. The decendent of
  // this class implements these functions to return its objects.
  virtual sql::Database& GetDB() = 0;

 private:
  FRIEND_TEST_ALL_PREFIXES(AndroidCacheDatabaseTest, InitAndroidCacheDatabase);

  // Creates the database and make it ready for attaching; returns true on
  // success.
  bool CreateDatabase(const base::FilePath& db_name);

  // Creates the bookmark_cache table in attached DB; returns true on success.
  // The created_time, last_visit_time, favicon_id and bookmark are stored.
  //
  // The created_time and last_visit_time are cached because Android use the
  // millisecond for the time unit, and we don't want to convert it in the
  // runtime for it requires to parsing the SQL.
  //
  // The favicon_id is also cached because it is in thumbnail database. Its
  // default value is set to null as the type of favicon column in Android APIs
  // is blob. To use default value null, we can support client query by
  // 'WHERE favicon IS NULL'.
  //
  // Bookmark column is used to indicate whether the url is bookmarked.
  bool CreateBookmarkCacheTable();

  // Creates the search_terms table in attached DB; returns true on success.
  // This table has _id, search, and date fields which match the Android's
  // definition.
  //
  // When Android Client require update the search term, the search term can't
  // be updated as it always associated a URL. We simulate the update by
  // deleting the old search term then inserting a new one, but the ID given
  // to client can not be changed, so it appears to client as update. This
  // table is used to mapping the ID given to client to the search term.
  //
  // The search term last visit time is stored in date as Android needs the time
  // in milliseconds.
  bool CreateSearchTermsTable();

  // Attachs to history database; returns true on success.
  bool Attach();

  // Does the real attach. Returns true on success.
  bool DoAttach();

  base::FilePath db_name_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_CACHE_DATABASE_H_
