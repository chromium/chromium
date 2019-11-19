// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_

#include <vector>

#include "base/macros.h"
#include "components/history/core/browser/history_types.h"

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace history {

// A visit database is one which stores visits for URLs, that is, times and
// linking information. A visit database must also be a URLDatabase, as this
// modifies tables used by URLs directly and could be thought of as inheriting
// from URLDatabase. However, this inheritance is not explicit as things would
// get too complicated and have multiple inheritance.
class VisitDatabase {
 public:
  // Must call InitVisitTable() before using to make sure the database is
  // initialized.
  VisitDatabase();
  virtual ~VisitDatabase();

  // Deletes the visit table. Used for rapidly clearing all visits. In this
  // case, InitVisitTable would be called immediately afterward to re-create it.
  // Returns true on success.
  bool DropVisitTable();

  // Adds a line to the visit database with the given information, returning
  // the added row ID on success, 0 on failure. The given visit is updated with
  // the new row ID on success. In addition, adds its source into visit_source
  // table.
  VisitID AddVisit(VisitRow* visit, VisitSource source);

  // Deletes the given visit from the database. If a visit with the given ID
  // doesn't exist, it will not do anything.
  void DeleteVisit(const VisitRow& visit);

  // Query a VisitInfo giving an visit id, filling the given VisitRow.
  // Returns true on success.
  bool GetRowForVisit(VisitID visit_id, VisitRow* out_visit);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The visit must exist. Returns true on success.
  bool UpdateVisitRow(const VisitRow& visit);

  // Fills in the given vector with all of the visits for the given page ID,
  // sorted in ascending order of date. Returns true on success (although there
  // may still be no matches).
  bool GetVisitsForURL(URLID url_id, VisitVector* visits);

  // Fills in the given vector with the visits for the given page ID which
  // should be user-visible, which excludes things like redirects and subframes,
  // and match the set of options passed, sorted in ascending order of date.
  //
  // Returns true if there are more results available, i.e. if the number of
  // results was restricted by |options.max_count|.
  bool GetVisibleVisitsForURL(URLID url_id,
                              const QueryOptions& options,
                              VisitVector* visits);

  // Fills the vector with all visits with times in the given list.
  //
  // The results will be in no particular order.  Also, no duplicate
  // detection is performed, so if |times| has duplicate times,
  // |visits| may have duplicate visits.
  bool GetVisitsForTimes(const std::vector<base::Time>& times,
                         VisitVector* visits);

  // Fills all visits in the time range [begin, end) to the given vector. Either
  // time can be is_null(), in which case the times in that direction are
  // unbounded.
  //
  // If |max_results| is non-zero, up to that many results will be returned. If
  // there are more results than that, the oldest ones will be returned. (This
  // is used for history expiration.)
  //
  // The results will be in increasing order of date.
  bool GetAllVisitsInRange(base::Time begin_time,
                           base::Time end_time,
                           int max_results,
                           VisitVector* visits);

  // Fills all visits with specified transition in the time range [begin, end)
  // to the given vector. Either time can be is_null(), in which case the times
  // in that direction are unbounded.
  //
  // If |max_results| is non-zero, up to that many results will be returned. If
  // there are more results than that, the oldest ones will be returned. (This
  // is used for history expiration.)
  //
  // The results will be in increasing order of date.
  bool GetVisitsInRangeForTransition(base::Time begin_time,
                                     base::Time end_time,
                                     int max_results,
                                     ui::PageTransition transition,
                                     VisitVector* visits);

  // Looks up URLIDs for all visits with specified transition. Returns true on
  // success and false otherwise.
  bool GetAllURLIDsForTransition(ui::PageTransition transition,
                                 std::vector<URLID>* urls);

  // Fills all visits in the given time range into the given vector that should
  // be user-visible, which excludes things like redirects and subframes. The
  // begin time is inclusive, the end time is exclusive. Either time can be
  // is_null(), in which case the times in that direction are unbounded.
  //
  // Up to |max_count| visits will be returned. If there are more visits than
  // that, the most recent |max_count| will be returned. If 0, all visits in the
  // range will be computed.
  //
  // Only one visit for each URL will be returned, and it will be the most
  // recent one in the time range.
  //
  // Returns true if there are more results available, i.e. if the number of
  // results was restricted by |options.max_count|.
  bool GetVisibleVisitsInRange(const QueryOptions& options,
                               VisitVector* visits);

  // Returns the visit ID for the most recent visit of the given URL ID, or 0
  // if there is no visit for the URL.
  //
  // If non-NULL, the given visit row will be filled with the information of
  // the found visit. When no visit is found, the row will be unchanged.
  VisitID GetMostRecentVisitForURL(URLID url_id, VisitRow* visit_row);

  // Returns the |max_results| most recent visit sessions for |url_id|.
  //
  // Returns false if there's a failure preparing the statement. True
  // otherwise. (No results are indicated with an empty |visits|
  // vector.)
  bool GetMostRecentVisitsForURL(URLID url_id,
                                 int max_results,
                                 VisitVector* visits);

  // Finds a redirect coming from the given |from_visit|. If a redirect is
  // found, it fills the visit ID and URL into the out variables and returns
  // true. If there is no redirect from the given visit, returns false.
  //
  // If there is more than one redirect, this will compute a random one. But
  // duplicates should be very rare, and we don't actually care which one we
  // get in most cases. These will occur when the user goes back and gets
  // redirected again.
  //
  // to_visit and to_url can be NULL in which case they are ignored.
  bool GetRedirectFromVisit(VisitID from_visit,
                            VisitID* to_visit,
                            GURL* to_url);

  // Similar to the above function except finds a redirect going to a given
  // |to_visit|.
  bool GetRedirectToVisit(VisitID to_visit,
                          VisitID* from_visit,
                          GURL* from_url);

  // Gets the number of user-visible visits to all URLs on the same
  // scheme/host/port as |url|, as well as the time of the earliest visit.
  // "User-visible" is defined as in GetVisibleVisitsInRange() above, i.e.
  // excluding redirects and subframes.
  // This function is only valid for HTTP and HTTPS URLs; all other schemes
  // cause the function to return false.
  bool GetVisibleVisitCountToHost(const GURL& url,
                                  int* count,
                                  base::Time* first_visit);

  // Gets the number of URLs as seen in chrome://history within the time
  // range [|begin_time|, |end_time|). "User-visible" is defined as in
  // GetVisibleVisitsInRange() above, i.e. excluding redirects and subframes.
  // Each URL is counted only once per day. For determination of the date,
  // timestamps are converted to dates using local time. Returns false if
  // there is a failure executing the statement. True otherwise.
  bool GetHistoryCount(const base::Time& begin_time,
                       const base::Time& end_time,
                       int* count);

  // Gets the last time any webpage on the given host was visited within the
  // time range [|begin_time|, |end_time|). If the given host has not been
  // visited in the given time range, this will return true and |last_visit|
  // will be set to null. False will be returned if the host is not a valid
  // HTTP or HTTPS url or for other database errors.
  bool GetLastVisitToHost(const GURL& host,
                          base::Time begin_time,
                          base::Time end_time,
                          base::Time* last_visit);

  // Get the time of the first item in our database.
  bool GetStartDate(base::Time* first_visit);

  // Get the source information about the given visits.
  void GetVisitsSource(const VisitVector& visits, VisitSourceMap* sources);

  // Returns the list of Google domain visits of the user based on the Google
  // searches issued in the specified time interval.
  // begin_time is inclusive, end_time is exclusive.
  std::vector<DomainVisit> GetGoogleDomainVisitsFromSearchesInRange(
      base::Time begin_time,
      base::Time end_time);

 protected:
  // Returns the database for the functions in this interface.
  virtual sql::Database& GetDB() = 0;

  // Called by the derived classes on initialization to make sure the tables
  // and indices are properly set up. Must be called before anything else.
  bool InitVisitTable();

  // Convenience to fill a VisitRow. Assumes the visit values are bound starting
  // at index 0.
  static void FillVisitRow(const sql::Statement& statement, VisitRow* visit);

  // Convenience to fill a VisitVector. Assumes that statement.step()
  // hasn't happened yet.
  static bool FillVisitVector(sql::Statement& statement, VisitVector* visits);

  // Convenience to fill a VisitVector while respecting the set of options.
  // |statement| should order the query decending by visit_time to ensure
  // correct duplicate management behavior. Assumes that statement.step()
  // hasn't happened yet.
  static bool FillVisitVectorWithOptions(sql::Statement& statement,
                                         const QueryOptions& options,
                                         VisitVector* visits);

  // Called by the derived classes to migrate the older visits table which
  // don't have visit_duration column yet.
  bool MigrateVisitsWithoutDuration();

  // Called by the derived classes to migrate the older visits table which
  // don't have incremented_omnibox_typed_score column yet.
  bool MigrateVisitsWithoutIncrementedOmniboxTypedScore();

  // A subprocedure in the process of migration to version 40.
  bool GetAllVisitedURLRowidsForMigrationToVersion40(
      std::vector<URLID>* visited_url_rowids_sorted);

 private:
  DISALLOW_COPY_AND_ASSIGN(VisitDatabase);
};

// Columns, in order, of the visit table.
#define HISTORY_VISIT_ROW_FIELDS                                        \
  " id,url,visit_time,from_visit,transition,segment_id,visit_duration," \
  "incremented_omnibox_typed_score "

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_
