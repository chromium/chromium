// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_

#include "components/history/core/browser/url_database.h"
#include "sql/database.h"

namespace history {

// Class used for a fast in-memory cache of typed URLs. Used for inline
// autocomplete since it is fast enough to be called synchronously as the user
// is typing.
class InMemoryDatabase : public URLDatabase {
 public:
  InMemoryDatabase();

  InMemoryDatabase(const InMemoryDatabase&) = delete;
  InMemoryDatabase& operator=(const InMemoryDatabase&) = delete;

  ~InMemoryDatabase() override;

  // Creates an empty in-memory database.
  bool InitFromScratch();

  // Initializes the database with the subset of `history_db` (the main,
  // already initialized history database) that this cache holds: the URLs that
  // were typed at least once or that have a keyword search term, and all
  // keyword search terms. Returns false only if the database could not be
  // created; a failure to copy rows leaves the cache empty or incomplete.
  bool InitFromUrlDatabase(URLDatabase& history_db);

 protected:
  // Implemented for URLDatabase.
  sql::Database& GetDB() override;

 private:
  // Initializes the database connection, this is the shared code between
  // `InitFromScratch()` and `InitFromUrlDatabase()` above. Returns true on
  // success.
  bool InitDB();

  // Copies the cached subset of `history_db` into this database, inside a
  // transaction that is rolled back if any insert fails.
  void PopulateFrom(URLDatabase& history_db);

  sql::Database db_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_
