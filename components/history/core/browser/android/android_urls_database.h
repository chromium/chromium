// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_DATABASE_H_

#include "components/history/core/browser/android/android_history_types.h"

namespace sql {
class Database;
}

namespace history {

// The table is used to stores the raw url which was passed in from
// ContentProvider APIs' client.
//
// Android BookmarmkCoulmns API allows the url without protocol like
// "www.bookmarks.com", but Chrome requires the url to be unique, like
// "http://www.bookmarks.com/". To support client queries by the orignal URL,
// the raw URL and corresponding URLID is stored in this table.
//
// Though the raw URL is stored. The 'www.bookmark.com' and
// 'http://www.bookmark.com' are still treated as the same URL, which means
// if adding these two urls, the later one will fail.
class AndroidURLsDatabase {
 public:
  AndroidURLsDatabase();

  AndroidURLsDatabase(const AndroidURLsDatabase&) = delete;
  AndroidURLsDatabase& operator=(const AndroidURLsDatabase&) = delete;

  virtual ~AndroidURLsDatabase();

  // Creates the android_urls table if it doesn't exist. Returns true if the
  // table was created or already exists.
  bool CreateAndroidURLsTable();

  // Adds a new mapping between `raw_url` and `url_id`, returns the id if it
  // succeeds, otherwise 0 is returned.
  AndroidURLID AddAndroidURLRow(const std::string& raw_url, URLID url_id);

  // Looks up the given `url_id` in android_urls table. Returns true if success,
  // and fill in the `row` if it not NULL, returns false if the `url_id` is not
  // found.
  bool GetAndroidURLRow(URLID url_id, AndroidURLRow* row);

  // Deletes the rows whose url_id is in `url_ids`. Returns true if all
  // `url_ids` were found and deleted, otherwise false is returned.
  bool DeleteAndroidURLRows(const std::vector<URLID>& url_ids);

  // Deletes all the rows whose url_id doesn't exist in urls table. Returns true
  // on success.
  bool DeleteUnusedAndroidURLs();

  // Updates the row of `id` with the given `raw_url` and `url_id`. Returns true
  // on success.
  bool UpdateAndroidURLRow(AndroidURLID id,
                           const std::string& raw_url,
                           URLID url_id);

  // Clears all the rows in android_urls table, returns true on success, false
  // on error.
  bool ClearAndroidURLRows();

  // Migrate from version 21 to 22.
  bool MigrateToVersion22();

 protected:
  // Returns the database for the functions in this interface. The decendent of
  // this class implements these functions to return its objects.
  virtual sql::Database& GetDB() = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_DATABASE_H_
