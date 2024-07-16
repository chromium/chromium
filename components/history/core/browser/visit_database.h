// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_

#include <string>
#include <vector>

#include "components/history/core/browser/history_types.h"
#include "url/origin.h"

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

  VisitDatabase(const VisitDatabase&) = delete;
  VisitDatabase& operator=(const VisitDatabase&) = delete;

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

  // Query a VisitRow given a visit id, filling the given VisitRow.
  // Returns true on success.
  bool GetRowForVisit(VisitID visit_id, VisitRow* out_visit);

  // Query a VisitRow given a visit time, filling the given VisitRow. If there
  // are multiple visits with the given visit time (which happens in case of
  // redirects), returns the one with the largest ID, i.e. the most recently
  // added one, i.e. the end of the redirect chain.
  // Returns true on success.
  bool GetLastRowForVisitByVisitTime(base::Time visit_time,
                                     VisitRow* out_visit);

  // Query a VisitRow given `originator_cache_guid` and `originator_visit_id`.
  // If found, returns true and writes the visit into `visit_row`; otherwise
  // returns false.
  bool GetRowForForeignVisit(const std::string& originator_cache_guid,
                             VisitID originator_visit_id,
                             VisitRow* out_visit);

  // Updates an existing row. The new information is set on the row, using the
  // VisitID as the key. The visit must exist. Returns true on success.
  // WARNING: when a VisitRow is created, it is assigned a VisitedLinkID
  // corresponding to its url and referring top-level and frame urls. Other than
  // sync code (crbug.com/1476511), callers should carefully consider what
  // columns are being updated, and if that update causes the visit's
  // VisitedLinkID to be incorrect going forward.
  bool UpdateVisitRow(const VisitRow& visit);

  // Marks ALL visits as NOT known to sync. This is called when Sync is turned
  // off by the user or disabled via feature flag. Visits can be marked as known
  // to sync again when Sync is re-enabled. This is used to flag which visits
  // have permission to fetch URL-keyed metadata.
  bool SetAllVisitsAsNotKnownToSync();

  // Fills in the given vector with all of the visits for the given page ID,
  // sorted in ascending order of date. Returns true on success (although there
  // may still be no matches).
  bool GetVisitsForURL(URLID url_id, VisitVector* visits);

  // Fills in the given vector with the visits for the given page ID which
  // should be user-visible, which excludes things like redirects and subframes,
  // and match the set of options passed, sorted in ascending order of date.
  //
  // Returns true if there are more results available, i.e. if the number of
  // results was restricted by `options.max_count`.
  bool GetVisibleVisitsForURL(URLID url_id,
                              const QueryOptions& options,
                              VisitVector* visits);

  // Fills the vector with all visits with times in the given list.
  //
  // The results will be in no particular order.  Also, no duplicate
  // detection is performed, so if `times` has duplicate times,
  // `visits` may have duplicate visits.
  bool GetVisitsForTimes(const std::vector<base::Time>& times,
                         VisitVector* visits);

  // Fills all visits in the time range [begin, end) to the given vector. Either
  // time can be is_null(), in which case the times in that direction are
  // unbounded. If app_id is present, restrict the results to those matching
  // the app_id only.
  //
  // If `max_results` is non-zero, up to that many results will be returned. If
  // there are more results than that, the oldest ones will be returned. (This
  // is used for history expiration.)
  //
  // The results will be in increasing order of date.
  bool GetAllVisitsInRange(base::Time begin_time,
                           base::Time end_time,
                           std::optional<std::string> app_id,
                           int max_results,
                           VisitVector* visits);

  // Fills all visits with specified transition in the time range [begin, end)
  // to the given vector. Either time can be is_null(), in which case the times
  // in that direction are unbounded.
  //
  // If `max_results` is non-zero, up to that many results will be returned. If
  // there are more results than that, the oldest ones will be returned. (This
  // is used for history expiration.)
  //
  // The results will be in increasing order of date.
  bool GetVisitsInRangeForTransition(base::Time begin_time,
                                     base::Time end_time,
                                     int max_results,
                                     ui::PageTransition transition,
                                     VisitVector* visits);

  // Fills some foreign visits (i.e. with a non-empty `originator_cache_guid`)
  // into `visits` - at most `max_visits` of them, and only those with a (local)
  // visit_id <= `max_visit_id`. Returns true on success and false otherwise.
  // NOTE: This returns only redirect-chain-ends (including individual visits
  // without redirects).
  bool GetSomeForeignVisits(VisitID max_visit_id,
                            int max_results,
                            VisitVector* visits);

  // Looks up URLIDs for all visits with specified transition. Returns true on
  // success and false otherwise.
  bool GetAllURLIDsForTransition(ui::PageTransition transition,
                                 std::vector<URLID>* urls);

  // Looks up all the app IDs found in the database entries. Returns a struct
  // containing the list of the IDs.
  GetAllAppIdsResult GetAllAppIds();

  // Fills all visits in the given time range into the given vector that should
  // be user-visible, which excludes things like redirects and subframes. The
  // begin time is inclusive, the end time is exclusive. Either time can be
  // is_null(), in which case the times in that direction are unbounded.
  //
  // Use `options.duplicate_policy` to control the URL deduplication policy -
  // for instance, if only a single visit should be returned for each URL.
  //
  // Up to `options.max_count` visits will be returned. If there are more visits
  // than that, the most recent `options.max_count` will be returned. If 0, all
  // visits in the range will be computed.
  //
  // Returns true if there are more results available, i.e. if the number of
  // results was restricted by `options.max_count`.
  bool GetVisibleVisitsInRange(const QueryOptions& options,
                               VisitVector* visits);

  // Returns the visit ID for the most recent visit of the given URL ID, or 0
  // if there is no visit for the URL.
  //
  // If non-NULL, the given visit row will be filled with the information of
  // the found visit. When no visit is found, the row will be unchanged.
  VisitID GetMostRecentVisitForURL(URLID url_id, VisitRow* visit_row);

  // Returns the `max_results` most recent visit sessions for `url_id`.
  //
  // Returns false if there's a failure preparing the statement. True
  // otherwise. (No results are indicated with an empty `visits`
  // vector.)
  bool GetMostRecentVisitsForURL(URLID url_id,
                                 int max_results,
                                 VisitVector* visits);

  // Finds a redirect coming from the given `from_visit`. If a redirect is
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
  // `to_visit`; or, if there is no such redirect, finds the referral going to
  // the given `to_visit`.
  bool GetRedirectToVisit(VisitID to_visit,
                          VisitID* from_visit,
                          GURL* from_url);

  // Gets the number of user-visible visits to all URLs on the same
  // scheme/host/port as `url`, as well as the time of the earliest visit.
  // "User-visible" is defined as in GetVisibleVisitsInRange() above, i.e.
  // excluding redirects and subframes.
  // This function is only valid for HTTP and HTTPS URLs; all other schemes
  // cause the function to return false.
  bool GetVisibleVisitCountToHost(const GURL& url,
                                  int* count,
                                  base::Time* first_visit);

  // Gets the number of URLs as seen in chrome://history within the time
  // range [`begin_time`, `end_time`). "User-visible" is defined as in
  // GetVisibleVisitsInRange() above, i.e. excluding redirects and subframes.
  // Each URL is counted only once per day. For determination of the date,
  // timestamps are converted to dates using local time. Returns false if
  // there is a failure executing the statement. True otherwise.
  bool GetHistoryCount(const base::Time& begin_time,
                       const base::Time& end_time,
                       int* count);

  // Gets the last time any webpage on the given host was visited within the
  // time range [`begin_time`, `end_time`). If the given host has not been
  // visited in the given time range, this will return true and `last_visit`
  // will be set to base::Time(). False will be returned if the host is not a
  // valid HTTP or HTTPS url or for other database errors.
  bool GetLastVisitToHost(const std::string& host,
                          base::Time begin_time,
                          base::Time end_time,
                          base::Time* last_visit);

  // Same as the above, but for the given origin instead of host.
  bool GetLastVisitToOrigin(const url::Origin& origin,
                            base::Time begin_time,
                            base::Time end_time,
                            base::Time* last_visit);

  // Gets the last time `url` was visited before `end_time`. If the given `url`
  // has no past visits, this will return true and `last_visit` will be set to
  // base::Time(). False will be returned if `url` is not a valid HTTP or HTTPS
  // url or for other database errors.
  bool GetLastVisitToURL(const GURL& url,
                         base::Time end_time,
                         base::Time* last_visit);

  // Gets counts for total visits and days visited for pages matching `host`'s
  // scheme, port, and host. Counts only user-visible visits.
  DailyVisitsResult GetDailyVisitsToHost(const GURL& host,
                                         base::Time begin_time,
                                         base::Time end_time);

  // Get the time of the first item in our database.
  bool GetStartDate(base::Time* first_visit);

  // Returns the maximum VisitID that is currently used in the DB. Due to
  // AUTOINCREMENT, any VisitIDs created after this call are guaranteed to be
  // larger than the returned value.
  // If there are no visits in the table, returns `kInvalidVisitId` (aka 0).
  VisitID GetMaxVisitIDInUse();

  // Get the source information about the given visit(s).
  void GetVisitsSource(const VisitVector& visits, VisitSourceMap* sources);
  VisitSource GetVisitSource(const VisitID visit_id);

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
  static void FillVisitRow(sql::Statement& statement, VisitRow* visit);

  // Convenience to fill a VisitVector. Assumes that statement.step()
  // hasn't happened yet.
  static bool FillVisitVector(sql::Statement& statement, VisitVector* visits);

  // Convenience to fill a VisitVector while respecting the set of options.
  // `statement` should order the query descending by visit_time to ensure
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

  // Called by the derived classes to migrate the older visits table which
  // don't have publicly_routable column yet.
  bool MigrateVisitsWithoutPubliclyRoutableColumn();

  // Called by the derived classes to do early checks before migrating the older
  // visits table's floc_allowed (for historical reasons named
  // "publicly_routable" in the schema) column to another table.
  bool CanMigrateFlocAllowed();

  // Called by the derived classes to migrate the older visits table which
  // which doesn't have `opener_visit` column and also drops `publicly_routable`
  // column which is no longer used.
  bool MigrateVisitsWithoutOpenerVisitColumnAndDropPubliclyRoutableColumn();

  // Called by the derived classes to migrate the older visits table which
  // which aren't ready to accommodate Sync. It sets `id` to AUTOINCREMENT, and
  // ensures the existence of the `originator_cache_guid` and
  // `originator_visit_id` columns.
  bool MigrateVisitsAutoincrementIdAndAddOriginatorColumns();

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `originator_from_visit` and `originator_opener_visit`
  // columns.
  bool MigrateVisitsAddOriginatorFromVisitAndOpenerVisitColumns();

  // Return true if the visits table's schema contains "AUTOINCREMENT".
  // false if table do not contain AUTOINCREMENT, or the table is not created.
  bool VisitTableContainsAutoincrement();

  // A subprocedure in the process of migration to version 40.
  bool GetAllVisitedURLRowidsForMigrationToVersion40(
      std::vector<URLID>* visited_url_rowids_sorted);

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `is_known_to_sync` column.
  bool MigrateVisitsAddIsKnownToSyncColumn();

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `consider_for_ntp_most_visited` column.
  bool MigrateVisitsAddConsiderForNewTabPageMostVisitedColumn();

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `external_referrer_url` column.
  bool MigrateVisitsAddExternalReferrerUrlColumn();

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `visited_link_id` column.
  bool MigrateVisitsAddVisitedLinkIdColumn();

  // Called by the derived classes to migrate the older visits table which
  // doesn't have the `app_id` column.
  bool MigrateVisitsAddAppId();
};

// Columns, in order, of the visit table.
#define HISTORY_VISIT_ROW_FIELDS                                    \
  " id,url,visit_time,from_visit,external_referrer_url,transition," \
  "segment_id,visit_duration,incremented_omnibox_typed_score,"      \
  "opener_visit,originator_cache_guid,originator_visit_id,"         \
  "originator_from_visit,originator_opener_visit,is_known_to_sync," \
  "consider_for_ntp_most_visited,visited_link_id,app_id "

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_VISIT_DATABASE_H_
