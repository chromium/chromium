// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_

#include "components/history/core/browser/url_database.h"
#include "sql/database.h"

namespace base {
class FilePath;
}

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

  // Initializes the database by directly slurping the data from the given
  // file. Conceptually, the InMemoryHistoryBackend should do the populating
  // after this object does some common initialization, but that would be
  // much slower.
  bool InitFromDisk(const base::FilePath& history_name);

 protected:
  // Implemented for URLDatabase.
  sql::Database& GetDB() override;

 private:
  // Initializes the database connection, this is the shared code between
  // InitFromScratch() and InitFromDisk() above. Returns true on success.
  bool InitDB();

  sql::Database db_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_IN_MEMORY_DATABASE_H_
