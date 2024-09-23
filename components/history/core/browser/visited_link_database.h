// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISITED_LINK_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISITED_LINK_DATABASE_H_

#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "sql/statement.h"

class GURL;

namespace sql {
class Database;
}

namespace history {

// A SQLite database that holds triple-key partitioned :visited links
// history info. It is independent from the currently unpartitioned
// VisitedLinks hash table. VisitedLinksWriter should NEVER alter this database.
// In the future, it will provide the necessary state to rebuild the partitioned
// VisitedLinks hash table in the event of corruption and to delete specific
// partition keys from the hash table.
//
// This is a subset of the full history data. It has a one to many relationship
// with the VisitDatabase, i.e. the VisitedLinkDatabase will only
// contain one row for N visits to the url, top-level url, and frame url
// found in the partition key. It has a many to one relationship with the
// URLDatabase, i.e. the VisitedLinkDatabase may contain N rows
// for each URL, as a URL can be visited from many different top-level urls or
// frame urls. Only link urls which can be attributed to a top-level url and
// frame url will be stored as entries in this database (i.e. link clicks
// and scripted navigations).
class VisitedLinkDatabase {
 public:
  // Must call CreateVisitedLinkTable() before using to make sure the database
  // is initialized.
  VisitedLinkDatabase();

  VisitedLinkDatabase(const VisitedLinkDatabase&) = delete;
  VisitedLinkDatabase& operator=(const VisitedLinkDatabase&) = delete;

  // This object must be destroyed on the thread where all accesses are
  // happening to avoid thread-safety problems.
  virtual ~VisitedLinkDatabase();

  // VisitedLink table functions
  // -------------------------------------------------------

  // Deletes the visited link database. Used for rapidly clearing all visits. In
  // this case, CreateVisitedLinkTable() would be called immediately afterward
  // to re-create it. Returns true on success.
  bool DropVisitedLinkTable();

  // Looks up a visited link given an id. Fills info with the data. Returns true
  // on success and false otherwise.
  bool GetVisitedLinkRow(VisitedLinkID visited_link_id, VisitedLinkRow& info);

  // Looks up the given visited link partition key, and if it exists, fills the
  // given pointer with the associated info and returns the ID of that visited
  // link. Returns 0 if the visited link partition key was not found.
  VisitedLinkID GetRowForVisitedLink(URLID link_url_id,
                                     const GURL& top_level_url,
                                     const GURL& frame_url,
                                     VisitedLinkRow& info);

  // Given an already-existing row in the visited link table, updates that
  // visited link's visit count. This can not change the link url id, top-level
  // url, or frame url. Returns true on success.
  bool UpdateVisitedLinkRowVisitCount(VisitedLinkID visited_link_id,
                                      int visit_count);

  // Adds a row to the visited link database with the given information and
  // returns the newly generated ID for the row. A row with the given visited
  // link must not already exist. Returns 0 on error.
  VisitedLinkID AddVisitedLink(URLID link_url_id,
                               const GURL& top_level_url,
                               const GURL& frame_url,
                               int visit_count);

  // Delete the row of the corresponding visited link. Returns true if
  // the row existed and was deleted.
  bool DeleteVisitedLinkRow(VisitedLinkID id);

  // Enumeration ---------------------------------------------------------------

  // The enumerator of the VisitedLinkDatabase.
  class VisitedLinkEnumerator {
   public:
    VisitedLinkEnumerator();

    VisitedLinkEnumerator(const VisitedLinkEnumerator&) = delete;
    VisitedLinkEnumerator& operator=(const VisitedLinkEnumerator&) = delete;

    virtual ~VisitedLinkEnumerator();

    // Retrieves the next visited link. Returns false if no more visited links
    // are available.
    bool GetNextVisitedLink(VisitedLinkRow& r);

   private:
    friend class VisitedLinkDatabase;

    bool initialized_;
    sql::Statement statement_;
  };

  // Initializes the given enumerator to enumerate all visited links in the
  // database.
  bool InitVisitedLinkEnumeratorForEverything(
      VisitedLinkEnumerator& enumerator);

 protected:
  friend class VisitDatabase;

  // Creates and initializes the SQLite database. Must be called before anything
  // else.
  bool CreateVisitedLinkTable();

  // Return true if the visited_links table's schema contains "AUTOINCREMENT".
  // false if table do not contain AUTOINCREMENT, or the table is not created.
  bool VisitedLinkTableContainsAutoincrement();

  // Convenience to fill a VisitedLinkRow. Must be in sync with the fields in
  // kHistoryVisitedLinkRowFields.
  static void FillVisitedLinkRow(sql::Statement& s, VisitedLinkRow& i);

  // Returns the database for the functions in this interface. The descendant of
  // this class implements these functions to return its objects.
  virtual sql::Database& GetDB() = 0;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISITED_LINK_DATABASE_H_
