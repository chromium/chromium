// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_database.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>

#include "base/logging.h"
#include "base/not_fatal_until.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace history {

namespace {

// Returns [lower, upper) bounds for matching a URL against `origin`.
std::pair<std::string, std::string> GetOriginSearchBounds(const GURL& origin) {
  // We need to search for URLs with a matching origin. One way to query
  // for this is to use the GLOB operator, eg 'url GLOB "http://google.com/*"'.
  // This approach requires escaping the * and ? and such a query would also
  // need to be recompiled on every Step(). The same query can be executed by
  // using >= and < operator. The query becomes: 'url >= http://google.com/' and
  // 'url < http://google.com0'. 0 is used as it is one character greater than
  // '/'. This effectively applies the GLOB optimization by doing it in C++
  // instead of relying on SQLite to do it.
  static_assert('/' + 1 == '0', "");
  std::string origin_query_min = origin.DeprecatedGetOriginAsURL().spec();
  DCHECK(!origin_query_min.empty());
  DCHECK_EQ('/', origin_query_min.back());

  std::string origin_query_max =
      origin_query_min.substr(0, origin_query_min.size() - 1) + '0';

  return {std::move(origin_query_min), std::move(origin_query_max)};
}

// Returns [lower, upper) bounds for matching a URL against origins with
// a non-standard port. 'origin' parameter must not have a port itself.
std::pair<std::string, std::string>
GetSearchBoundsForAllOriginsWithNonDefaultPort(const GURL& origin) {
  // Similar to the above function, but we use ';' instead of 0 to cover origins
  // with a port. The query becomes: 'url >= http://google.com:' and 'url <
  // http://google.com;'.
  static_assert(':' + 1 == ';', "");
  const std::string spec = origin.DeprecatedGetOriginAsURL().spec();
  DCHECK(!spec.empty());
  DCHECK_EQ('/', spec.back());
  DCHECK(!origin.has_port());

  // Need to replace the end character accordingly.
  auto end = spec.size() - 1;
  std::string origin_query_min = spec.substr(0, end) + ':';
  std::string origin_query_max = spec.substr(0, end) + ';';

  return {std::move(origin_query_min), std::move(origin_query_max)};
}

// Returns a vector of four [lower, upper) bounds for matching a URL against
// `host_name`.
std::array<std::pair<std::string, std::string>, 4> GetHostSearchBounds(
    const std::string& host_name) {
  std::array<std::pair<std::string, std::string>, 4> bounds;
  // GetOriginSearchBounds only handles origin, so we need to query both http
  // and https versions, as well as origins with non-default ports.
  const GURL http("http://" + host_name);
  const GURL https("https://" + host_name);
  bounds[0] = GetOriginSearchBounds(http);
  bounds[1] = GetSearchBoundsForAllOriginsWithNonDefaultPort(http);
  bounds[2] = GetOriginSearchBounds(https);
  bounds[3] = GetSearchBoundsForAllOriginsWithNonDefaultPort(https);
  return bounds;
}

// Transition IDs are from possibly-corrupt databases or incorrect IDs due to
// version skew. Where `transition` isn't valid we fall back on
// PAGE_TRANSITION_LINK.
ui::PageTransition PageTransitionFromIntWithFallback(int32_t transition) {
  return ui::IsValidPageTransitionType(transition)
             ? ui::PageTransitionFromInt(transition)
             : ui::PAGE_TRANSITION_LINK;
}

// Is the transition user-visible.
bool TransitionIsVisible(int32_t transition) {
  const ui ::PageTransition page_transition =
      PageTransitionFromIntWithFallback(transition);
  return (ui::PAGE_TRANSITION_CHAIN_END & transition) != 0 &&
         ui::PageTransitionIsMainFrame(page_transition) &&
         !ui::PageTransitionCoreTypeIs(page_transition,
                                       ui::PAGE_TRANSITION_KEYWORD_GENERATED);
}

VisitSource VisitSourceFromInt(int value) {
  auto converted = static_cast<VisitSource>(value);
  // Verify that `converted` is actually a valid enum value.
  switch (converted) {
    case SOURCE_SYNCED:
    case SOURCE_BROWSED:
    case SOURCE_EXTENSION:
    case SOURCE_FIREFOX_IMPORTED:
    case SOURCE_IE_IMPORTED:
    case SOURCE_SAFARI_IMPORTED:
      return converted;
  }
  // In cases of database corruption, SOURCE_BROWSED is a safe default value.
  return SOURCE_BROWSED;
}

}  // namespace

VisitDatabase::VisitDatabase() = default;

VisitDatabase::~VisitDatabase() = default;

bool VisitDatabase::InitVisitTable() {
  if (!GetDB().DoesTableExist("visits")) {
    // Note that it is possible that expiration code could leave both
    // `from_visit` and `opener_visit` to refer to IDs that don't exist in the
    // database. As such, this code should not use AUTOINCREMENT. If it did, we
    // will need to ensure expiring resets from_visit and opener_visit fields
    // for each row.
    if (!GetDB().Execute(
            "CREATE TABLE visits("
            // The `id` uses AUTOINCREMENT to support Sync. Chrome Sync uses the
            // `id` in conjunction with the Client ID as a unique identifier.
            // If this was not AUTOINCREMENT, deleting a row and creating a new
            // one could reuse the same `id` for an entirely new visit, which
            // would confuse Sync, as Sync would be unable to distinguish
            // an update from a deletion plus a creation.
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "url INTEGER NOT NULL,"  // key of the URL this corresponds to
            "visit_time INTEGER NOT NULL,"
            // Although NULLable, our code writes 0 to visits without referrers.
            "from_visit INTEGER,"
            "external_referrer_url TEXT,"
            "transition INTEGER DEFAULT 0 NOT NULL,"
            "segment_id INTEGER,"
            // Some old DBs may have an "is_indexed" field here, but this is no
            // longer used and should NOT be read or written from any longer.
            "visit_duration INTEGER DEFAULT 0 NOT NULL,"
            "incremented_omnibox_typed_score BOOLEAN DEFAULT FALSE NOT NULL,"
            // Although NULLable, our code writes 0 to visits without openers.
            "opener_visit INTEGER,"
            // For remote visits synced onto our local machine:
            //  - `originator_cache_guid` is the unique identifier for the
            //    machine the visit was originally made on (called the
            //    "originator" below).
            //  - `originator_visit_id` is the `id` of the visit row as
            //    originally assigned by AUTOINCREMENT on the originator.
            //  - The tuple of (`originator_cache_guid`, `originator_visit_id`)
            //    is globally unique.
            //  - `originator_from_visit` and `originator_opener_visit` refer to
            //    `originator_visit_id`, NOT the local visit IDs.
            //  - The `from_visit` and `opener_visit` columns are remapped to
            //    local IDs.
            // For local visits:
            //  - Although NULLable, local visits always write an empty string
            //    and 0s to these columns for implementation simplicity and
            //    consistency with C++ types. It's harmless, because NULL is
            //    interpreted that way upon reading anyways.
            //  - NULL values in the database can occur in the wild for old
            //    database versions that were migrated, but this is harmless.
            "originator_cache_guid TEXT,"
            "originator_visit_id INTEGER,"
            "originator_from_visit INTEGER,"
            "originator_opener_visit INTEGER,"
            // Set to true for visits known to Chrome Sync, which can be:
            //  1. Remote visits that have been synced to the local machine.
            //  2. Local visits that have been sent to Sync.
            "is_known_to_sync BOOLEAN DEFAULT FALSE NOT NULL,"
            // Specifies whether a navigation should contribute to the Most
            // Visited tiles in the New Tab Page. Note that setting this to true
            // (most common case) doesn't guarantee it's relevant for Most
            // Visited, since other requirements exist (e.g. certain page
            // transition types).
            "consider_for_ntp_most_visited BOOLEAN DEFAULT FALSE NOT NULL,"
            // VisitedLinkID this visit corresponds to (if any). If this visit
            // does not has a transition type of `LINK` or `MANUAL_SUBFRAME`, it
            // will not be stored in the VisitedLinkDatabase and the code will
            // write its `visited_link_id` as 0 (kInvalidVisitedLinkID).
            "visited_link_id INTEGER DEFAULT 0 NOT NULL,"
            // Package name (e.g. com.google.android.youtube) of the app opening
            // the Custom Tab that contributes to this visit. This is set to a
            // non-null string only on Android, if the app identity is known to
            // the Custom Tab.
            "app_id TEXT)")) {
      return false;
    }
  }

  // Visit source table contains the source information for all the visits. To
  // save space, we do not record those user browsed visits which would be the
  // majority in this table. Only other sources are recorded.
  // Due to the tight relationship between visit_source and visits table, they
  // should be created and dropped at the same time.
  if (!GetDB().DoesTableExist("visit_source")) {
    if (!GetDB().Execute("CREATE TABLE visit_source("
                         "id INTEGER PRIMARY KEY,source INTEGER NOT NULL)"))
      return false;
  }

  // Index over url so we can quickly find visits for a page.
  if (!GetDB().Execute(
          "CREATE INDEX IF NOT EXISTS visits_url_index ON visits (url)"))
    return false;

  // Create an index over from visits so that we can efficiently find
  // referrers and redirects.
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS visits_from_index ON "
                       "visits (from_visit)"))
    return false;

  // Create an index over time so that we can efficiently find the visits in a
  // given time range (most history views are time-based).
  if (!GetDB().Execute("CREATE INDEX IF NOT EXISTS visits_time_index ON "
                       "visits (visit_time)"))
    return false;

  // Create an index over originator visit IDs so that Sync can efficiently
  // re-map them into local IDs.
  // Note: Some tests manually create older versions of the DB where the
  // `originator_visit_id` column doesn't exist yet. In those cases, don't try
  // creating an index (which would fail).
  if (GetDB().DoesColumnExist("visits", "originator_visit_id")) {
    if (!GetDB().Execute(
            "CREATE INDEX IF NOT EXISTS visits_originator_id_index ON visits "
            "(originator_visit_id)"))
      return false;
  }

  return true;
}

bool VisitDatabase::DropVisitTable() {
  // This will also drop the indices over the table.
  return GetDB().Execute("DROP TABLE IF EXISTS visit_source") &&
         GetDB().Execute("DROP TABLE visits");
}

// Must be in sync with HISTORY_VISIT_ROW_FIELDS.
// static
void VisitDatabase::FillVisitRow(sql::Statement& statement, VisitRow* visit) {
  visit->visit_id = statement.ColumnInt64(0);
  visit->url_id = statement.ColumnInt64(1);
  visit->visit_time = statement.ColumnTime(2);
  visit->referring_visit = statement.ColumnInt64(3);
  visit->external_referrer_url = GURL(statement.ColumnString(4));
  visit->transition = PageTransitionFromIntWithFallback(statement.ColumnInt(5));
  visit->segment_id = statement.ColumnInt64(6);
  visit->visit_duration = statement.ColumnTimeDelta(7);
  visit->incremented_omnibox_typed_score = statement.ColumnBool(8);
  visit->opener_visit = statement.ColumnInt64(9);
  visit->originator_cache_guid = statement.ColumnString(10);
  visit->originator_visit_id = statement.ColumnInt64(11);
  visit->originator_referring_visit = statement.ColumnInt64(12);
  visit->originator_opener_visit = statement.ColumnInt64(13);
  visit->is_known_to_sync = statement.ColumnBool(14);
  visit->consider_for_ntp_most_visited = statement.ColumnBool(15);
  visit->visited_link_id = statement.ColumnInt64(16);
  std::string app_id = statement.ColumnString(17);
  if (!app_id.empty()) {
    visit->app_id = app_id;
  }
}

// static
bool VisitDatabase::FillVisitVector(sql::Statement& statement,
                                    VisitVector* visits) {
  if (!statement.is_valid())
    return false;

  while (statement.Step()) {
    VisitRow visit;
    FillVisitRow(statement, &visit);
    visits->push_back(visit);
  }

  return statement.Succeeded();
}

// static
bool VisitDatabase::FillVisitVectorWithOptions(sql::Statement& statement,
                                               const QueryOptions& options,
                                               VisitVector* visits) {
  std::map<URLID, VisitRow> found_urls;

  // Keeps track of the day that `found_urls` is holding the URLs for, in order
  // to handle removing per-day duplicates.
  base::Time found_urls_midnight;

  while (statement.Step()) {
    VisitRow visit;
    FillVisitRow(statement, &visit);

    // Skip transitions that aren't user-visible.
    if (!TransitionIsVisible(visit.transition))
      continue;

    if (options.duplicate_policy != QueryOptions::KEEP_ALL_DUPLICATES) {
      if (options.duplicate_policy == QueryOptions::REMOVE_DUPLICATES_PER_DAY &&
          found_urls_midnight != visit.visit_time.LocalMidnight()) {
        found_urls.clear();
        found_urls_midnight = visit.visit_time.LocalMidnight();
      }
      // Make sure the URL this visit corresponds to is unique.
      auto it = found_urls.find(visit.url_id);
      if (it != found_urls.end()) {
#if defined(ANDROID)
        // The visit with app ID is preferred. Replace the already added visit
        // with a new one if it doesn't have an app ID but the new one does.
        VisitRow& ov = it->second;
        if (!ov.app_id && visit.app_id) {
          auto is_matched = [ov](VisitRow v) { return ov.url_id == v.url_id; };
          auto pos = std::find_if(visits->begin(), visits->end(), is_matched);
          CHECK(pos != visits->end(), base::NotFatalUntil::M130);
          *pos = visit;
          found_urls[visit.url_id] = visit;
        }
#endif
        continue;
      }
      found_urls[visit.url_id] = visit;
    }

    if (static_cast<int>(visits->size()) >= options.EffectiveMaxCount())
      return true;
    visits->push_back(visit);
  }
  return false;
}

VisitID VisitDatabase::AddVisit(VisitRow* visit, VisitSource source) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO visits "
      "(url, visit_time, from_visit, external_referrer_url, transition, "
      "segment_id, visit_duration, incremented_omnibox_typed_score,"
      "opener_visit, originator_cache_guid, originator_visit_id, "
      "originator_from_visit, originator_opener_visit, is_known_to_sync, "
      "consider_for_ntp_most_visited, visited_link_id, app_id) "
      "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  // Although some columns are NULLable, we never write NULL. We write 0 or ""
  // instead for simplicity. See the CREATE TABLE comments for details.
  statement.BindInt64(0, visit->url_id);
  statement.BindTime(1, visit->visit_time);
  statement.BindInt64(2, visit->referring_visit);
  statement.BindString(3, visit->external_referrer_url.spec());
  statement.BindInt64(4, visit->transition);
  statement.BindInt64(5, visit->segment_id);
  statement.BindTimeDelta(6, visit->visit_duration);
  statement.BindBool(7, visit->incremented_omnibox_typed_score);
  statement.BindInt64(8, visit->opener_visit);
  statement.BindString(9, visit->originator_cache_guid);
  statement.BindInt64(10, visit->originator_visit_id);
  statement.BindInt64(11, visit->originator_referring_visit);
  statement.BindInt64(12, visit->originator_opener_visit);
  statement.BindBool(13, visit->is_known_to_sync);
  statement.BindBool(14, visit->consider_for_ntp_most_visited);
  statement.BindInt64(15, visit->visited_link_id);
  statement.BindString(16, visit->app_id ? *visit->app_id : "");

  if (!statement.Run()) {
    DVLOG(0) << "Failed to execute visit insert statement:  "
             << "url_id = " << visit->url_id;
    return 0;
  }

  visit->visit_id = GetDB().GetLastInsertRowId();

  if (source != SOURCE_BROWSED) {
    // Record the source of this visit when it is not browsed.
    sql::Statement statement1(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "INSERT INTO visit_source (id, source) VALUES (?,?)"));
    statement1.BindInt64(0, visit->visit_id);
    statement1.BindInt64(1, source);

    if (!statement1.Run()) {
      DVLOG(0) << "Failed to execute visit_source insert statement:  "
               << "id = " << visit->visit_id;
      return 0;
    }
  }

  return visit->visit_id;
}

void VisitDatabase::DeleteVisit(const VisitRow& visit) {
  // Patch around this visit. Any visits that this went to will now have their
  // "source" be the deleted visit's source.
  sql::Statement update_chain(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "UPDATE visits SET from_visit=? WHERE from_visit=?"));
  update_chain.BindInt64(0, visit.referring_visit);
  update_chain.BindInt64(1, visit.visit_id);
  if (!update_chain.Run())
    return;

  // Now delete the actual visit.
  sql::Statement del(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM visits WHERE id=?"));
  del.BindInt64(0, visit.visit_id);
  if (!del.Run())
    return;

  // Try to delete the entry in visit_source table as well.
  // If the visit was browsed, there is no corresponding entry in visit_source
  // table, and nothing will be deleted.
  del.Assign(GetDB().GetCachedStatement(SQL_FROM_HERE,
                                        "DELETE FROM visit_source WHERE id=?"));
  del.BindInt64(0, visit.visit_id);
  del.Run();
}

bool VisitDatabase::GetRowForVisit(VisitID visit_id, VisitRow* out_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits WHERE id=?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step())
    return false;

  FillVisitRow(statement, out_visit);

  // We got a different visit than we asked for, something is wrong.
  DCHECK_EQ(visit_id, out_visit->visit_id);
  if (visit_id != out_visit->visit_id)
    return false;

  return true;
}

bool VisitDatabase::GetLastRowForVisitByVisitTime(base::Time visit_time,
                                                  VisitRow* out_visit) {
  // In the case of redirects, there may be multiple visits with the same
  // timestamp. In that case, the one with the largest ID should be the end of
  // the redirect chain.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits WHERE visit_time=? ORDER BY id DESC LIMIT 1"));
  statement.BindTime(0, visit_time);

  if (!statement.Step())
    return false;

  FillVisitRow(statement, out_visit);

  // We got a different visit than we asked for, something is wrong.
  DCHECK_EQ(visit_time, out_visit->visit_time);
  if (visit_time != out_visit->visit_time)
    return false;

  return true;
}

bool VisitDatabase::GetRowForForeignVisit(
    const std::string& originator_cache_guid,
    VisitID originator_visit_id,
    VisitRow* out_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits WHERE originator_cache_guid=? and originator_visit_id=?"));
  statement.BindString(0, originator_cache_guid);
  statement.BindInt64(1, originator_visit_id);

  if (!statement.Step())
    return false;

  FillVisitRow(statement, out_visit);
  return true;
}

bool VisitDatabase::UpdateVisitRow(const VisitRow& visit) {
  // Don't store inconsistent data to the database.
  DCHECK_NE(visit.visit_id, visit.referring_visit);
  if (visit.visit_id == visit.referring_visit)
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE visits SET "
      "url=?,visit_time=?,from_visit=?,external_referrer_url=?,transition=?,"
      "segment_id=?,visit_duration=?,incremented_omnibox_typed_score=?,"
      "opener_visit=?,originator_cache_guid=?,originator_visit_id=?,"
      "is_known_to_sync=?,consider_for_ntp_most_visited=?,visited_link_id=?,"
      "app_id=? WHERE id=?"));
  // Although some columns are NULLable, we never write NULL. We write 0 or ""
  // instead for simplicity. See the CREATE TABLE comments for details.
  // |app_id| is not expected to be updated. Not included in the statement.
  statement.BindInt64(0, visit.url_id);
  statement.BindTime(1, visit.visit_time);
  statement.BindInt64(2, visit.referring_visit);
  statement.BindString(3, visit.external_referrer_url.spec());
  statement.BindInt64(4, visit.transition);
  statement.BindInt64(5, visit.segment_id);
  statement.BindTimeDelta(6, visit.visit_duration);
  statement.BindBool(7, visit.incremented_omnibox_typed_score);
  statement.BindInt64(8, visit.opener_visit);
  statement.BindString(9, visit.originator_cache_guid);
  statement.BindInt64(10, visit.originator_visit_id);
  statement.BindInt64(11, visit.is_known_to_sync);
  statement.BindInt64(12, visit.consider_for_ntp_most_visited);
  statement.BindInt64(13, visit.visited_link_id);
  statement.BindString(14, visit.app_id ? *visit.app_id : "");
  statement.BindInt64(15, visit.visit_id);

  return statement.Run();
}

bool VisitDatabase::SetAllVisitsAsNotKnownToSync() {
  sql::Statement statement(
      GetDB().GetUniqueStatement("UPDATE visits SET is_known_to_sync=0"));
  return statement.Run();
}

bool VisitDatabase::GetVisitsForURL(URLID url_id, VisitVector* visits) {
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                     "WHERE url=? "
                     "ORDER BY visit_time ASC"));
  statement.BindInt64(0, url_id);
  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetVisibleVisitsForURL(URLID url_id,
                                           const QueryOptions& options,
                                           VisitVector* visits) {
  visits->clear();

  sql::Statement statement;
  if (options.visit_order == QueryOptions::RECENT_FIRST) {
    if (options.app_id) {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT" HISTORY_VISIT_ROW_FIELDS
          "FROM visits "
          "WHERE url=? AND visit_time>=? AND visit_time<? AND app_id=? "
          "ORDER BY visit_time DESC"));
    } else {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE url=? AND visit_time>=? AND visit_time<? "
                         "ORDER BY visit_time DESC"));
    }
  } else {
    if (options.app_id) {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT" HISTORY_VISIT_ROW_FIELDS
          "FROM visits "
          "WHERE url=? AND visit_time>? AND visit_time<=? AND app_id=? "
          "ORDER BY visit_time ASC"));
    } else {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE url=? AND visit_time>? AND visit_time<=? "
                         "ORDER BY visit_time ASC"));
    }
  }

  statement.BindInt64(0, url_id);
  statement.BindInt64(1, options.EffectiveBeginTime());
  statement.BindInt64(2, options.EffectiveEndTime());
  if (options.app_id) {
    statement.BindString(3, *options.app_id);
  }

  return FillVisitVectorWithOptions(statement, options, visits);
}

bool VisitDatabase::GetVisitsForTimes(const std::vector<base::Time>& times,
                                      VisitVector* visits) {
  visits->clear();

  for (const auto& time : times) {
    sql::Statement statement(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                       "WHERE visit_time == ?"));

    statement.BindTime(0, time);

    if (!FillVisitVector(statement, visits))
      return false;
  }
  return true;
}

bool VisitDatabase::GetAllVisitsInRange(base::Time begin_time,
                                        base::Time end_time,
                                        std::optional<std::string> app_id,
                                        int max_results,
                                        VisitVector* visits) {
  visits->clear();

  sql::Statement statement;
  if (app_id) {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                       "WHERE visit_time >= ? AND visit_time < ? AND app_id=? "
                       "ORDER BY visit_time LIMIT ?"));
  } else {
    statement.Assign(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                       "WHERE visit_time >= ? AND visit_time < ? "
                       "ORDER BY visit_time LIMIT ?"));
  }

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64_t end = end_time.ToInternalValue();
  statement.BindTime(0, begin_time);
  statement.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());
  if (app_id) {
    statement.BindString(2, *app_id);
    statement.BindInt64(
        3, max_results ? max_results : std::numeric_limits<int64_t>::max());
  } else {
    statement.BindInt64(
        2, max_results ? max_results : std::numeric_limits<int64_t>::max());
  }
  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetVisitsInRangeForTransition(base::Time begin_time,
                                                  base::Time end_time,
                                                  int max_results,
                                                  ui::PageTransition transition,
                                                  VisitVector* visits) {
  DCHECK(visits);
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                     "WHERE visit_time >= ? AND visit_time < ? "
                     "AND (transition & ?) == ?"
                     "ORDER BY visit_time LIMIT ?"));

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64_t end = end_time.ToInternalValue();
  statement.BindTime(0, begin_time);
  statement.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());
  statement.BindInt64(2, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt64(3, transition);
  statement.BindInt64(
      4, max_results ? max_results : std::numeric_limits<int64_t>::max());

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetSomeForeignVisits(VisitID max_visit_id,
                                         int max_results,
                                         VisitVector* visits) {
  DCHECK(visits);
  visits->clear();

  // Exactly all foreign visits (i.e. coming from a different device) have an
  // `originator_cache_guid` set.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE originator_cache_guid IS NOT NULL AND originator_cache_guid != '' "
      "AND (transition & ?) <> 0 "
      "AND id <= ? "
      "LIMIT ?"));
  statement.BindInt64(0, ui::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt64(1, max_visit_id);
  statement.BindInt(2, max_results);

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetAllURLIDsForTransition(ui::PageTransition transition,
                                              std::vector<URLID>* urls) {
  DCHECK(urls);
  urls->clear();
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT DISTINCT url FROM visits "
                                 "WHERE (transition & ?) == ?"));
  statement.BindInt64(0, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt64(1, transition);

  while (statement.Step()) {
    urls->push_back(statement.ColumnInt64(0));
  }
  return statement.Succeeded();
}

GetAllAppIdsResult VisitDatabase::GetAllAppIds() {
  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT DISTINCT app_id FROM visits "
      "WHERE app_id != '' ORDER BY visit_time DESC"));

  GetAllAppIdsResult result;

  while (statement.Step()) {
    result.app_ids.push_back(statement.ColumnString(0));
  }
  return result;
}

bool VisitDatabase::GetVisibleVisitsInRange(const QueryOptions& options,
                                            VisitVector* visits) {
  visits->clear();
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.

  sql::Statement statement;
  if (options.visit_order == QueryOptions::RECENT_FIRST) {
    if (options.app_id) {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE visit_time>=? AND visit_time<? AND app_id=? "
                         "ORDER BY visit_time DESC, id DESC"));
    } else {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE visit_time>=? AND visit_time<? "
                         "ORDER BY visit_time DESC, id DESC"));
    }
  } else {
    if (options.app_id) {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE visit_time>? AND visit_time<=? AND app_id=? "
                         "ORDER BY visit_time ASC, id DESC"));
    } else {
      statement.Assign(GetDB().GetCachedStatement(
          SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                         "WHERE visit_time>? AND visit_time<=? "
                         "ORDER BY visit_time ASC, id DESC"));
    }
  }

  statement.BindInt64(0, options.EffectiveBeginTime());
  statement.BindInt64(1, options.EffectiveEndTime());
  if (options.app_id) {
    statement.BindString(2, *options.app_id);
  }

  return FillVisitVectorWithOptions(statement, options, visits);
}

VisitID VisitDatabase::GetMostRecentVisitForURL(URLID url_id,
                                                VisitRow* visit_row) {
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                     "WHERE url=? "
                     "ORDER BY visit_time DESC, id DESC "
                     "LIMIT 1"));
  statement.BindInt64(0, url_id);
  if (!statement.Step())
    return 0;  // No visits for this URL.

  if (visit_row) {
    FillVisitRow(statement, visit_row);
    return visit_row->visit_id;
  }
  return statement.ColumnInt64(0);
}

bool VisitDatabase::GetMostRecentVisitsForURL(URLID url_id,
                                              int max_results,
                                              VisitVector* visits) {
  visits->clear();

  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                     "WHERE url=? "
                     "ORDER BY visit_time DESC, id DESC "
                     "LIMIT ?"));
  statement.BindInt64(0, url_id);
  statement.BindInt(1, max_results);

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetRedirectFromVisit(VisitID from_visit,
                                         VisitID* to_visit,
                                         GURL* to_url) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT v.id,u.url "
      "FROM visits v JOIN urls u ON v.url = u.id "
      "WHERE v.from_visit = ? "
      "AND (v.transition & ?) != 0"));  // IS_REDIRECT_MASK
  statement.BindInt64(0, from_visit);
  statement.BindInt64(1, ui::PAGE_TRANSITION_IS_REDIRECT_MASK);

  if (!statement.Step())
    return false;  // No redirect from this visit. (Or SQL error)
  if (to_visit)
    *to_visit = statement.ColumnInt64(0);
  if (to_url)
    *to_url = GURL(statement.ColumnString(1));
  return true;
}

bool VisitDatabase::GetRedirectToVisit(VisitID to_visit,
                                       VisitID* from_visit,
                                       GURL* from_url) {
  VisitRow row;
  if (!GetRowForVisit(to_visit, &row))
    return false;

  if (from_visit)
    *from_visit = row.referring_visit;

  if (from_url) {
    sql::Statement statement(GetDB().GetCachedStatement(
        SQL_FROM_HERE,
        "SELECT u.url "
        "FROM visits v JOIN urls u ON v.url = u.id "
        "WHERE v.id = ? AND (v.transition & ?) != 0"));
    statement.BindInt64(0, row.referring_visit);
    statement.BindInt64(1, (ui::PAGE_TRANSITION_IS_REDIRECT_MASK |
                            ui::PAGE_TRANSITION_CHAIN_START));

    if (!statement.Step())
      return false;

    *from_url = GURL(statement.ColumnString(0));
  }
  return true;
}

bool VisitDatabase::GetVisibleVisitCountToHost(const GURL& url,
                                               int* count,
                                               base::Time* first_visit) {
  if (!url.SchemeIs(url::kHttpScheme) && !url.SchemeIs(url::kHttpsScheme))
    return false;

  // We need to search for URLs with a matching host/port. One way to query for
  // this is to use the LIKE operator, eg 'url LIKE http://google.com/%'. This
  // is inefficient though in that it doesn't use the index and each entry must
  // be visited. The same query can be executed by using >= and < operator.
  // The query becomes:
  // 'url >= http://google.com/' and url < http://google.com0'.
  // 0 is used as it is one character greater than '/'.
  const std::string host_query_min = url.DeprecatedGetOriginAsURL().spec();
  if (host_query_min.empty())
    return false;

  // We also want to restrict ourselves to main frame navigations that are not
  // in the middle of redirect chains, hence the transition checks.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT v.visit_time,transition "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE u.url >= ? AND u.url < ?"));
  statement.BindString(0, host_query_min);
  statement.BindString(
      1, host_query_min.substr(0, host_query_min.size() - 1) + '0');

  int visit_count = 0;
  base::Time min_visit_time = base::Time::Max();
  while (statement.Step()) {
    if (!TransitionIsVisible(statement.ColumnInt(1)))
      continue;
    ++visit_count;
    min_visit_time = std::min(statement.ColumnTime(0), min_visit_time);
  }

  if (!statement.Succeeded())
    return false;

  *count = visit_count;
  if (visit_count > 0)
    *first_visit = min_visit_time;

  return true;
}

bool VisitDatabase::GetHistoryCount(const base::Time& begin_time,
                                    const base::Time& end_time,
                                    int* count) {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE,
                                 "SELECT url,"
                                 "visit_time,"
                                 "transition "
                                 "FROM visits "
                                 "WHERE visit_time >= ? AND visit_time < ?"));

  statement.BindTime(0, begin_time);
  statement.BindTime(1, end_time);

  // Set of (date, url) pairs.
  std::set<std::pair<base::Time, std::string>> url_days;
  while (statement.Step()) {
    if (!TransitionIsVisible(statement.ColumnInt(2)))
      continue;
    url_days.emplace(statement.ColumnTime(1).LocalMidnight(),
                     statement.ColumnString(0));
  }

  *count = url_days.size();
  return true;
}

bool VisitDatabase::GetLastVisitToHost(const std::string& host,
                                       base::Time begin_time,
                                       base::Time end_time,
                                       base::Time* last_visit) {
  const GURL http("http://" + host);
  const GURL https("https://" + host);
  if (!http.is_valid() || !https.is_valid())
    return false;

  // GetOriginSearchBounds only handles origin, so we need to query both http
  // and https versions.
  std::array<std::pair<std::string, std::string>, 4> bounds =
      GetHostSearchBounds(host);

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "  v.visit_time, v.transition "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE "
      "  ( (u.url >= ? AND u.url < ?) OR "
      "    (u.url >= ? AND u.url < ?) OR "
      "    (u.url >= ? AND u.url < ?) OR "
      "    (u.url >= ? AND u.url < ?) ) AND "
      "  v.visit_time >= ? AND "
      "  v.visit_time < ? "
      "ORDER BY v.visit_time DESC "));
  statement.BindString(0, bounds.at(0).first);
  statement.BindString(1, bounds.at(0).second);
  statement.BindString(2, bounds.at(1).first);
  statement.BindString(3, bounds.at(1).second);
  statement.BindString(4, bounds.at(2).first);
  statement.BindString(5, bounds.at(2).second);
  statement.BindString(6, bounds.at(3).first);
  statement.BindString(7, bounds.at(3).second);
  statement.BindTime(8, begin_time);
  statement.BindTime(9, end_time);

  while (statement.Step()) {
    if (ui::PageTransitionIsMainFrame(
            PageTransitionFromIntWithFallback(statement.ColumnInt(1)))) {
      *last_visit = statement.ColumnTime(0);
      return true;
    }
  }
  // If there are no entries from the statement, the host may not have been
  // visited in the given time range. Zero the time result and report the
  // success of the statement.
  *last_visit = base::Time();
  return statement.Succeeded();
}

bool VisitDatabase::GetLastVisitToOrigin(const url::Origin& origin,
                                         base::Time begin_time,
                                         base::Time end_time,
                                         base::Time* last_visit) {
  if (origin.opaque() || !(origin.scheme() == url::kHttpScheme ||
                           origin.scheme() == url::kHttpsScheme))
    return false;

  std::pair<std::string, std::string> origin_bounds =
      GetOriginSearchBounds(origin.GetURL());

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "  v.visit_time "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE "
      "  u.url >= ? AND "
      "  u.url < ? AND "
      "  v.visit_time >= ? AND "
      "  v.visit_time < ? "
      "ORDER BY v.visit_time DESC "
      "LIMIT 1"));
  statement.BindString(0, origin_bounds.first);
  statement.BindString(1, origin_bounds.second);
  statement.BindTime(2, begin_time);
  statement.BindTime(3, end_time);

  if (!statement.Step()) {
    // If there are no entries from the statement, the host may not have been
    // visited in the given time range. Zero the time result and report the
    // success of the statement.
    *last_visit = base::Time();
    return statement.Succeeded();
  }

  *last_visit = statement.ColumnTime(0);
  return true;
}

bool VisitDatabase::GetLastVisitToURL(const GURL& url,
                                      base::Time end_time,
                                      base::Time* last_visit) {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS())
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "  v.visit_time "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE "
      "  u.url = ? AND "
      "  v.visit_time < ? "
      "ORDER BY v.visit_time DESC "
      "LIMIT 1"));
  statement.BindString(0, url.spec());
  statement.BindTime(1, end_time);

  if (!statement.Step()) {
    // If there are no entries from the statement, the URL may not have been
    // visited in the given time range. Zero the time result and report the
    // success of the statement.
    *last_visit = base::Time();
    return statement.Succeeded();
  }

  *last_visit = statement.ColumnTime(0);
  return true;
}

DailyVisitsResult VisitDatabase::GetDailyVisitsToHost(const GURL& host,
                                                      base::Time begin_time,
                                                      base::Time end_time) {
  DailyVisitsResult result;
  if (!host.is_valid() || !host.SchemeIsHTTPOrHTTPS())
    return result;

  std::pair<std::string, std::string> host_bounds = GetOriginSearchBounds(host);

  sql::Statement statement(GetDB().GetCachedStatement(
      // clang-format off
      SQL_FROM_HERE,
        "SELECT "
        "visit_time,"
        "transition "
        "FROM visits v INNER JOIN urls u ON v.url=u.id "
        "WHERE "
          "u.url>=? AND "
          "u.url<? AND "
          "v.visit_time>=? AND "
          "v.visit_time<?"
      // clang-format on
      ));

  statement.BindString(0, host_bounds.first);
  statement.BindString(1, host_bounds.second);
  statement.BindTime(2, begin_time);
  statement.BindTime(3, end_time);

  std::vector<base::Time> dates;
  while (statement.Step()) {
    if (!TransitionIsVisible(statement.ColumnInt(1)))
      continue;
    ++result.total_visits;
    dates.push_back(statement.ColumnTime(0).LocalMidnight());
  }
  std::sort(dates.begin(), dates.end());
  result.days_with_visits =
      std::unique(dates.begin(), dates.end()) - dates.begin();
  result.success = true;

  return result;
}

bool VisitDatabase::GetStartDate(base::Time* first_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT MIN(visit_time) FROM visits WHERE visit_time != 0"));
  if (!statement.Step() || statement.ColumnInt64(0) == 0) {
    *first_visit = base::Time::Now();
    return false;
  }
  *first_visit = statement.ColumnTime(0);
  return true;
}

VisitID VisitDatabase::GetMaxVisitIDInUse() {
  sql::Statement statement(
      GetDB().GetCachedStatement(SQL_FROM_HERE, "SELECT MAX(id) FROM visits"));
  if (!statement.Step()) {
    // The visits table must be empty.
    return kInvalidVisitID;
  }
  return statement.ColumnInt64(0);
}

void VisitDatabase::GetVisitsSource(const VisitVector& visits,
                                    VisitSourceMap* sources) {
  DCHECK(sources);
  sources->clear();

  // We query the source in batch. Here defines the batch size.
  const size_t batch_size = 500;
  size_t visits_size = visits.size();

  size_t start_index = 0, end_index = 0;
  while (end_index < visits_size) {
    start_index = end_index;
    end_index = end_index + batch_size < visits_size ? end_index + batch_size
                                                     : visits_size;

    // Compose the sql statement with a list of ids.
    std::string sql = "SELECT id,source FROM visit_source ";
    sql.append("WHERE id IN (");
    // Append all the ids in the statement.
    for (size_t j = start_index; j < end_index; j++) {
      if (j != start_index)
        sql.push_back(',');
      sql.append(base::NumberToString(visits[j].visit_id));
    }
    sql.append(") ORDER BY id");
    sql::Statement statement(GetDB().GetUniqueStatement(sql));

    // Get the source entries out of the query result.
    while (statement.Step()) {
      std::pair<VisitID, VisitSource> source_entry(
          statement.ColumnInt64(0), VisitSourceFromInt(statement.ColumnInt(1)));
      sources->insert(source_entry);
    }
  }
}

VisitSource VisitDatabase::GetVisitSource(const VisitID visit_id) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT source FROM visit_source WHERE id=?"));
  statement.BindInt64(0, visit_id);
  if (!statement.Step())
    return VisitSource::SOURCE_BROWSED;
  return VisitSourceFromInt(statement.ColumnInt(0));
}

std::vector<DomainVisit>
VisitDatabase::GetGoogleDomainVisitsFromSearchesInRange(base::Time begin_time,
                                                        base::Time end_time) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      // clang-format off
      "SELECT "
          "visit_time,"
          "u.url "
          "FROM "
              "urls u JOIN visits v ON u.id=v.url "
          "WHERE "
              // Pre-filtering to limit the number of entries to process in
              // C++. The url column is indexed so this makes the query more
              // efficient. We then confirm in C++ that the domain of an entry
              // is a valid Google domain before counting the visit.
              "(u.url LIKE 'https://www.google.__/search%' OR "
               "u.url LIKE 'https://www.google.___/search%' OR "
               "u.url LIKE 'https://www.google.__.__/search%' OR "
               "u.url LIKE 'https://www.google.___.__/search%') AND "
              // Restrict to visits that are more recent than the specified
              // start time.
              "visit_time >= ? AND "
              // Restrict to visits that are older than the specified end time.
              "visit_time < ?"));
  // clang-format on
  statement.BindTime(0, begin_time);
  statement.BindTime(1, end_time);
  std::vector<DomainVisit> domain_visits;
  while (statement.Step()) {
    const GURL url(statement.ColumnString(1));
    if (google_util::IsGoogleSearchUrl(url)) {
      domain_visits.emplace_back(url.host(), statement.ColumnTime(0));
    }
  }
  return domain_visits;
}

bool VisitDatabase::MigrateVisitsWithoutDuration() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "visit_duration")) {
    // Old versions don't have the visit_duration column, we modify the table
    // to add that field.
    if (!GetDB().Execute(
            "ALTER TABLE visits "
            "ADD COLUMN visit_duration INTEGER DEFAULT 0 NOT NULL"))
      return false;
  }
  return true;
}

bool VisitDatabase::MigrateVisitsWithoutIncrementedOmniboxTypedScore() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "incremented_omnibox_typed_score")) {
    // Wrap the creation and initialization of the new column in a transaction
    // since the value must be computed outside of SQL and iteratively updated.
    sql::Transaction committer(&GetDB());
    if (!committer.Begin())
      return false;

    // Old versions don't have the incremented_omnibox_typed_score column, we
    // modify the table to add that field. We iterate through the table and
    // compute the result for each row.
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN incremented_omnibox_typed_score BOOLEAN "
                         "DEFAULT FALSE NOT NULL"))
      return false;

    // Iterate through rows in the visits table and update each with the
    // appropriate increment_omnibox_typed_score value. Because this column was
    // newly added, the existing (default) value is not valid/correct.
    sql::Statement read(GetDB().GetUniqueStatement(
        "SELECT "
        "id,url,visit_time,from_visit,transition,segment_id,visit_duration,"
        "incremented_omnibox_typed_score FROM visits"));
    while (read.is_valid() && read.Step()) {
      VisitRow row;
      row.visit_id = read.ColumnInt64(0);
      row.url_id = read.ColumnInt64(1);
      row.visit_time = read.ColumnTime(2);
      row.referring_visit = read.ColumnInt64(3);
      row.transition = PageTransitionFromIntWithFallback(read.ColumnInt(4));
      row.segment_id = read.ColumnInt64(5);
      row.visit_duration = read.ColumnTimeDelta(6);
      // Check if the visit row is in an invalid state and if it is then
      // leave the new field as the default value.
      if (row.visit_id == row.referring_visit)
        continue;
      row.incremented_omnibox_typed_score =
          HistoryBackend::IsTypedIncrement(row.transition);

      sql::Statement statement(GetDB().GetCachedStatement(
          SQL_FROM_HERE,
          "UPDATE visits SET "
          "url=?,visit_time=?,from_visit=?,transition=?,segment_id=?,"
          "visit_duration=?,incremented_omnibox_typed_score=? "
          "WHERE id=?"));
      statement.BindInt64(0, row.url_id);
      statement.BindTime(1, row.visit_time);
      statement.BindInt64(2, row.referring_visit);
      statement.BindInt64(3, row.transition);
      statement.BindInt64(4, row.segment_id);
      statement.BindTimeDelta(5, row.visit_duration);
      statement.BindBool(6, row.incremented_omnibox_typed_score);
      statement.BindInt64(7, row.visit_id);

      if (!statement.Run())
        return false;
    }
    if (!read.Succeeded() || !committer.Commit())
      return false;
  }
  return true;
}

bool VisitDatabase::MigrateVisitsWithoutPubliclyRoutableColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("visits", "publicly_routable"))
    return true;

  // Old versions don't have the publicly_routable column, we modify the table
  // to add that field.
  return GetDB().Execute(
      "ALTER TABLE visits "
      "ADD COLUMN publicly_routable BOOLEAN "
      "DEFAULT FALSE NOT NULL");
}

bool VisitDatabase::CanMigrateFlocAllowed() {
  // Migration expects a "visits" table with a "publicly_routable" column.
  return GetDB().DoesTableExist("visits") &&
         GetDB().DoesColumnExist("visits", "publicly_routable");
}

bool VisitDatabase::
    MigrateVisitsWithoutOpenerVisitColumnAndDropPubliclyRoutableColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("visits", "opener_visit"))
    return true;

  sql::Transaction transaction(&GetDB());
  return transaction.Begin() &&
         GetDB().Execute(
             "CREATE TABLE visits_tmp("
             "id INTEGER PRIMARY KEY,"
             "url INTEGER NOT NULL,"  // key of the URL this corresponds to
             "visit_time INTEGER NOT NULL,"
             "from_visit INTEGER,"
             "transition INTEGER DEFAULT 0 NOT NULL,"
             "segment_id INTEGER,"
             "visit_duration INTEGER DEFAULT 0 NOT NULL,"
             "incremented_omnibox_typed_score BOOLEAN DEFAULT FALSE NOT "
             "NULL)") &&
         GetDB().Execute(
             "INSERT INTO visits_tmp SELECT "
             "id, url, visit_time, from_visit, transition, segment_id, "
             "visit_duration, incremented_omnibox_typed_score FROM visits") &&
         GetDB().Execute(
             "ALTER TABLE visits_tmp ADD COLUMN opener_visit INTEGER") &&
         GetDB().Execute("DROP TABLE visits") &&
         GetDB().Execute("ALTER TABLE visits_tmp RENAME TO visits") &&
         transaction.Commit();
}

bool VisitDatabase::MigrateVisitsAutoincrementIdAndAddOriginatorColumns() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (GetDB().DoesColumnExist("visits", "originator_cache_guid") &&
      GetDB().DoesColumnExist("visits", "originator_visit_id") &&
      VisitTableContainsAutoincrement()) {
    return true;
  }

  sql::Transaction transaction(&GetDB());
  return transaction.Begin() &&
         GetDB().Execute(
             "CREATE TABLE visits_tmp("
             "id INTEGER PRIMARY KEY AUTOINCREMENT,"
             "url INTEGER NOT NULL,"  // key of the URL this corresponds to
             "visit_time INTEGER NOT NULL,"
             "from_visit INTEGER,"
             "transition INTEGER DEFAULT 0 NOT NULL,"
             "segment_id INTEGER,"
             "visit_duration INTEGER DEFAULT 0 NOT NULL,"
             "incremented_omnibox_typed_score BOOLEAN DEFAULT FALSE NOT NULL,"
             "opener_visit INTEGER)") &&
         GetDB().Execute(
             "INSERT INTO visits_tmp SELECT "
             "id, url, visit_time, from_visit, transition, segment_id, "
             "visit_duration, incremented_omnibox_typed_score, opener_visit "
             "FROM visits") &&
         GetDB().Execute(
             "ALTER TABLE visits_tmp ADD COLUMN originator_cache_guid TEXT") &&
         GetDB().Execute(
             "ALTER TABLE visits_tmp ADD COLUMN originator_visit_id INTEGER") &&
         GetDB().Execute("DROP TABLE visits") &&
         GetDB().Execute("ALTER TABLE visits_tmp RENAME TO visits") &&
         transaction.Commit();
}

bool VisitDatabase::MigrateVisitsAddOriginatorFromVisitAndOpenerVisitColumns() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  // Old versions don't have the originator_from_visit or
  // originator_opener_visit columns; modify the table to add those.
  if (!GetDB().DoesColumnExist("visits", "originator_from_visit")) {
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN originator_from_visit INTEGER")) {
      return false;
    }
  }
  if (!GetDB().DoesColumnExist("visits", "originator_opener_visit")) {
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN originator_opener_visit INTEGER")) {
      return false;
    }
  }

  return true;
}

bool VisitDatabase::VisitTableContainsAutoincrement() {
  // sqlite_schema has columns:
  //   type - "index" or "table".
  //   name - name of created element.
  //   tbl_name - name of element, or target table in case of index.
  //   rootpage - root page of the element in database file.
  //   sql - SQL to create the element.
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT sql FROM sqlite_schema WHERE type = "
                                 "'table' AND name = 'visits'"));

  // visits table does not exist.
  if (!statement.Step())
    return false;

  std::string urls_schema = statement.ColumnString(0);
  // We check if the whole schema contains "AUTOINCREMENT", since
  // "AUTOINCREMENT" only can be used for "INTEGER PRIMARY KEY", so we assume no
  // other columns could contain "AUTOINCREMENT".
  return urls_schema.find("AUTOINCREMENT") != std::string::npos;
}

bool VisitDatabase::GetAllVisitedURLRowidsForMigrationToVersion40(
    std::vector<URLID>* visited_url_rowids_sorted) {
  DCHECK(visited_url_rowids_sorted);
  sql::Statement statement(GetDB().GetUniqueStatement(
      "SELECT DISTINCT url FROM visits ORDER BY url"));

  while (statement.Step()) {
    visited_url_rowids_sorted->push_back(statement.ColumnInt64(0));
  }
  return statement.Succeeded();
}

bool VisitDatabase::MigrateVisitsAddIsKnownToSyncColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "is_known_to_sync")) {
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN is_known_to_sync "
                         "BOOLEAN DEFAULT FALSE NOT NULL")) {
      return false;
    }

    // Note we specifically DO NOT update the existing visits that have
    // `visit_source` == `SOURCE_SYNCED` to have `is_known_to_sync` set to true.
    //
    // This is because we don't know if the user has subsequently turned off
    // Sync, and we only want to flag this on for visits that are CURRENTLY
    // known to Sync and associated with the current user.
  }

  return true;
}

bool VisitDatabase::MigrateVisitsAddConsiderForNewTabPageMostVisitedColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "consider_for_ntp_most_visited")) {
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN consider_for_ntp_most_visited "
                         "BOOLEAN DEFAULT FALSE NOT NULL")) {
      return false;
    }
  }

  return true;
}

bool VisitDatabase::MigrateVisitsAddExternalReferrerUrlColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }

  if (!GetDB().DoesColumnExist("visits", "external_referrer_url")) {
    if (!GetDB().Execute("ALTER TABLE visits "
                         "ADD COLUMN external_referrer_url TEXT")) {
      return false;
    }
  }

  return true;
}

bool VisitDatabase::MigrateVisitsAddVisitedLinkIdColumn() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }
  if (!GetDB().DoesColumnExist("visits", "visited_link_id")) {
    if (!GetDB().Execute(
            "ALTER TABLE visits ADD COLUMN visited_link_id INTEGER")) {
      return false;
    }
  }
  return true;
}

bool VisitDatabase::MigrateVisitsAddAppId() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED_IN_MIGRATION() << " Visits table should exist before migration";
    return false;
  }
  if (!GetDB().DoesColumnExist("visits", "app_id")) {
    if (!GetDB().Execute("ALTER TABLE visits ADD COLUMN app_id TEXT")) {
      return false;
    }
  }
  return true;
}
}  // namespace history
