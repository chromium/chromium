// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "components/google/core/common/google_util.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history/core/browser/url_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/page_transition_types.h"
#include "url/url_constants.h"

namespace history {

VisitDatabase::VisitDatabase() {}

VisitDatabase::~VisitDatabase() {}

bool VisitDatabase::InitVisitTable() {
  if (!GetDB().DoesTableExist("visits")) {
    if (!GetDB().Execute(
            "CREATE TABLE visits("
            "id INTEGER PRIMARY KEY,"
            "url INTEGER NOT NULL,"  // key of the URL this corresponds to
            "visit_time INTEGER NOT NULL,"
            "from_visit INTEGER,"
            "transition INTEGER DEFAULT 0 NOT NULL,"
            "segment_id INTEGER,"
            // Some old DBs may have an "is_indexed" field here, but this is no
            // longer used and should NOT be read or written from any longer.
            "visit_duration INTEGER DEFAULT 0 NOT NULL,"
            "incremented_omnibox_typed_score BOOLEAN DEFAULT FALSE NOT NULL)"))
      return false;
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

  return true;
}

bool VisitDatabase::DropVisitTable() {
  // This will also drop the indices over the table.
  return GetDB().Execute("DROP TABLE IF EXISTS visit_source") &&
         GetDB().Execute("DROP TABLE visits");
}

// Must be in sync with HISTORY_VISIT_ROW_FIELDS.
// static
void VisitDatabase::FillVisitRow(const sql::Statement& statement,
                                 VisitRow* visit) {
  visit->visit_id = statement.ColumnInt64(0);
  visit->url_id = statement.ColumnInt64(1);
  visit->visit_time = base::Time::FromInternalValue(statement.ColumnInt64(2));
  visit->referring_visit = statement.ColumnInt64(3);
  visit->transition = ui::PageTransitionFromInt(statement.ColumnInt(4));
  visit->segment_id = statement.ColumnInt64(5);
  visit->visit_duration =
      base::TimeDelta::FromInternalValue(statement.ColumnInt64(6));
  visit->incremented_omnibox_typed_score = statement.ColumnBool(7);
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
  std::set<URLID> found_urls;

  // Keeps track of the day that |found_urls| is holding the URLs for, in order
  // to handle removing per-day duplicates.
  base::Time found_urls_midnight;

  while (statement.Step()) {
    VisitRow visit;
    FillVisitRow(statement, &visit);

    if (options.duplicate_policy != QueryOptions::KEEP_ALL_DUPLICATES) {
      if (options.duplicate_policy == QueryOptions::REMOVE_DUPLICATES_PER_DAY &&
          found_urls_midnight != visit.visit_time.LocalMidnight()) {
        found_urls.clear();
        found_urls_midnight = visit.visit_time.LocalMidnight();
      }
      // Make sure the URL this visit corresponds to is unique.
      if (found_urls.find(visit.url_id) != found_urls.end())
        continue;
      found_urls.insert(visit.url_id);
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
      "(url, visit_time, from_visit, transition, segment_id, "
      "visit_duration, incremented_omnibox_typed_score) "
      "VALUES (?,?,?,?,?,?,?)"));
  statement.BindInt64(0, visit->url_id);
  statement.BindInt64(1, visit->visit_time.ToInternalValue());
  statement.BindInt64(2, visit->referring_visit);
  statement.BindInt64(3, visit->transition);
  statement.BindInt64(4, visit->segment_id);
  statement.BindInt64(5, visit->visit_duration.ToInternalValue());
  statement.BindBool(6, visit->incremented_omnibox_typed_score);

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

bool VisitDatabase::UpdateVisitRow(const VisitRow& visit) {
  // Don't store inconsistent data to the database.
  DCHECK_NE(visit.visit_id, visit.referring_visit);
  if (visit.visit_id == visit.referring_visit)
    return false;

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE visits SET "
      "url=?,visit_time=?,from_visit=?,transition=?,segment_id=?,"
      "visit_duration=?,incremented_omnibox_typed_score=? WHERE id=?"));
  statement.BindInt64(0, visit.url_id);
  statement.BindInt64(1, visit.visit_time.ToInternalValue());
  statement.BindInt64(2, visit.referring_visit);
  statement.BindInt64(3, visit.transition);
  statement.BindInt64(4, visit.segment_id);
  statement.BindInt64(5, visit.visit_duration.ToInternalValue());
  statement.BindBool(6, visit.incremented_omnibox_typed_score);
  statement.BindInt64(7, visit.visit_id);

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

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE url=? AND visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) != 0 "              // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // NO SUBFRAME or
                                                // KEYWORD_GENERATED
      "ORDER BY visit_time DESC"));
  statement.BindInt64(0, url_id);
  statement.BindInt64(1, options.EffectiveBeginTime());
  statement.BindInt64(2, options.EffectiveEndTime());
  statement.BindInt(3, ui::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(4, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(5, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(6, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(7, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  return FillVisitVectorWithOptions(statement, options, visits);
}

bool VisitDatabase::GetVisitsForTimes(const std::vector<base::Time>& times,
                                      VisitVector* visits) {
  visits->clear();

  for (auto it = times.begin(); it != times.end(); ++it) {
    sql::Statement statement(GetDB().GetCachedStatement(
        SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                       "WHERE visit_time == ?"));

    statement.BindInt64(0, it->ToInternalValue());

    if (!FillVisitVector(statement, visits))
      return false;
  }
  return true;
}

bool VisitDatabase::GetAllVisitsInRange(base::Time begin_time,
                                        base::Time end_time,
                                        int max_results,
                                        VisitVector* visits) {
  visits->clear();

  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits "
                     "WHERE visit_time >= ? AND visit_time < ?"
                     "ORDER BY visit_time LIMIT ?"));

  // See GetVisibleVisitsInRange for more info on how these times are bound.
  int64_t end = end_time.ToInternalValue();
  statement.BindInt64(0, begin_time.ToInternalValue());
  statement.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());
  statement.BindInt64(
      2, max_results ? max_results : std::numeric_limits<int64_t>::max());

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
  statement.BindInt64(0, begin_time.ToInternalValue());
  statement.BindInt64(1, end ? end : std::numeric_limits<int64_t>::max());
  statement.BindInt(2, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(3, transition);
  statement.BindInt64(
      4, max_results ? max_results : std::numeric_limits<int64_t>::max());

  return FillVisitVector(statement, visits);
}

bool VisitDatabase::GetAllURLIDsForTransition(ui::PageTransition transition,
                                              std::vector<URLID>* urls) {
  DCHECK(urls);
  urls->clear();
  sql::Statement statement(
      GetDB().GetUniqueStatement("SELECT DISTINCT url FROM visits "
                                 "WHERE (transition & ?) == ?"));
  statement.BindInt(0, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(1, transition);

  while (statement.Step()) {
    urls->push_back(statement.ColumnInt64(0));
  }
  return statement.Succeeded();
}

bool VisitDatabase::GetVisibleVisitsInRange(const QueryOptions& options,
                                            VisitVector* visits) {
  visits->clear();
  // The visit_time values can be duplicated in a redirect chain, so we sort
  // by id too, to ensure a consistent ordering just in case.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT" HISTORY_VISIT_ROW_FIELDS
      "FROM visits "
      "WHERE visit_time >= ? AND visit_time < ? "
      "AND (transition & ?) != 0 "              // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // NO SUBFRAME or
                                                // KEYWORD_GENERATED
      "ORDER BY visit_time DESC, id DESC"));

  statement.BindInt64(0, options.EffectiveBeginTime());
  statement.BindInt64(1, options.EffectiveEndTime());
  statement.BindInt(2, ui::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(3, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(4, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(5, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(6, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

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
  statement.BindInt(1, ui::PAGE_TRANSITION_IS_REDIRECT_MASK);

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
  const std::string host_query_min = url.GetOrigin().spec();
  if (host_query_min.empty())
    return false;

  // We also want to restrict ourselves to main frame navigations that are not
  // in the middle of redirect chains, hence the transition checks.
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT MIN(v.visit_time), COUNT(*) "
      "FROM visits v INNER JOIN urls u ON v.url = u.id "
      "WHERE u.url >= ? AND u.url < ? "
      "AND (transition & ?) != 0 "
      "AND (transition & ?) NOT IN (?, ?, ?)"));
  statement.BindString(0, host_query_min);
  statement.BindString(
      1, host_query_min.substr(0, host_query_min.size() - 1) + '0');
  statement.BindInt(2, ui::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(3, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(4, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(5, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(6, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  if (!statement.Step()) {
    // We've never been to this page before.
    *count = 0;
    return true;
  }

  if (!statement.Succeeded())
    return false;

  *first_visit = base::Time::FromInternalValue(statement.ColumnInt64(0));
  *count = statement.ColumnInt(1);
  return true;
}

bool VisitDatabase::GetHistoryCount(const base::Time& begin_time,
                                    const base::Time& end_time,
                                    int* count) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT COUNT(*) FROM ("
      "SELECT DISTINCT url, "
      // Convert unit of timestamp from the numbers of microseconds since
      // Windows Epoch to the number of seconds from Unix Epoch. Leap seconds
      // are not handled in both timestamp units, so a linear conversion is
      // valid here.
      "DATE((visit_time - ?) / ?, 'unixepoch', 'localtime')"
      "FROM visits "
      "WHERE (transition & ?) != 0 "            // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // NO SUBFRAME or
                                                // KEYWORD_GENERATED
      "AND visit_time >= ? AND visit_time < ?"
      ")"));

  statement.BindInt64(0, base::Time::kTimeTToMicrosecondsOffset);
  statement.BindInt64(1, base::Time::kMicrosecondsPerSecond);
  statement.BindInt(2, ui::PAGE_TRANSITION_CHAIN_END);
  statement.BindInt(3, ui::PAGE_TRANSITION_CORE_MASK);
  statement.BindInt(4, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  statement.BindInt(5, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  statement.BindInt(6, ui::PAGE_TRANSITION_KEYWORD_GENERATED);
  statement.BindInt64(7, begin_time.ToInternalValue());
  statement.BindInt64(8, end_time.ToInternalValue());

  if (!statement.Step())
    return false;

  *count = statement.ColumnInt(0);
  return true;
}

bool VisitDatabase::GetLastVisitToHost(const GURL& host,
                                       base::Time begin_time,
                                       base::Time end_time,
                                       base::Time* last_visit) {
  if (!host.is_valid() || !host.SchemeIsHTTPOrHTTPS())
    return false;

  // We need to search for URLs with a matching host/port. One way to query for
  // this is to use the GLOB operator, eg 'url GLOB "http://google.com/*"'. This
  // approach requires escaping the * and ? and such a query would also need to
  // be recompiled on every Step(). The same query can be executed by using >=
  // and < operator. The query becomes: 'url >= http://google.com/' and url <
  // http://google.com0'. 0 is used as it is one character greater than '/'.
  // This effectively applies the GLOB optimization by doing it in C++ instead
  // of relying on SQLite to do it.
  const std::string host_query_min = host.GetOrigin().spec();
  DCHECK(!host_query_min.empty());
  DCHECK_EQ('/', host_query_min.back());

  const std::string host_query_max =
      host_query_min.substr(0, host_query_min.size() - 1) + '0';

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
  statement.BindString(0, host_query_min);
  statement.BindString(1, host_query_max);
  statement.BindInt64(2, begin_time.ToInternalValue());
  statement.BindInt64(3, end_time.ToInternalValue());

  if (!statement.Step()) {
    // If there are no entries from the statement, the host may not have been
    // visited in the given time range. Zero the time result and report the
    // success of the statement.
    *last_visit = base::Time();
    return statement.Succeeded();
  }

  *last_visit = base::Time::FromInternalValue(statement.ColumnInt64(0));
  return true;
}

bool VisitDatabase::GetStartDate(base::Time* first_visit) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT MIN(visit_time) FROM visits WHERE visit_time != 0"));
  if (!statement.Step() || statement.ColumnInt64(0) == 0) {
    *first_visit = base::Time::Now();
    return false;
  }
  *first_visit = base::Time::FromInternalValue(statement.ColumnInt64(0));
  return true;
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
    sql::Statement statement(GetDB().GetUniqueStatement(sql.c_str()));

    // Get the source entries out of the query result.
    while (statement.Step()) {
      std::pair<VisitID, VisitSource> source_entry(
          statement.ColumnInt64(0),
          static_cast<VisitSource>(statement.ColumnInt(1)));
      sources->insert(source_entry);
    }
  }
}

std::vector<DomainVisit>
VisitDatabase::GetGoogleDomainVisitsFromSearchesInRange(base::Time begin_time,
                                                        base::Time end_time) {
  sql::Statement statement(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "  visit_time, "
      "  u.url "
      "FROM  "
      "  urls u JOIN visits v ON u.id = v.url "
      "WHERE "
      // Pre-filtering to limit the number of entries to process in
      // C++. The url column is indexed so this makes the query more
      // efficient. We then confirm in C++ that the domain of an entry
      // is a valid Google domain before counting the visit.
      "  (u.url LIKE \"https://www.google.__/search%\" OR "
      "   u.url LIKE \"https://www.google.___/search%\" OR "
      "   u.url LIKE \"https://www.google.__.__/search%\" OR "
      "   u.url LIKE \"https://www.google.___.__/search%\") AND "
      // Restrict to visits that are more recent than the specified start
      // time.
      "  visit_time >= ? AND "
      // Restrict to visits that are older than the specified end time.
      "  visit_time < ? "));
  statement.BindInt64(0,
                      begin_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  statement.BindInt64(1, end_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  std::vector<DomainVisit> domain_visits;
  while (statement.Step()) {
    const GURL url(statement.ColumnString(1));
    if (google_util::IsGoogleSearchUrl(url)) {
      domain_visits.emplace_back(
          url.host(),
          base::Time::FromDeltaSinceWindowsEpoch(
              base::TimeDelta::FromMicroseconds(statement.ColumnInt64(0))));
    }
  }
  return domain_visits;
}

bool VisitDatabase::MigrateVisitsWithoutDuration() {
  if (!GetDB().DoesTableExist("visits")) {
    NOTREACHED() << " Visits table should exist before migration";
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
    NOTREACHED() << " Visits table should exist before migration";
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
        "SELECT" HISTORY_VISIT_ROW_FIELDS "FROM visits"));
    while (read.is_valid() && read.Step()) {
      VisitRow row;
      FillVisitRow(read, &row);
      // Check if the visit row is in an invalid state and if it is then
      // leave the new field as the default value.
      if (row.visit_id == row.referring_visit)
        continue;
      row.incremented_omnibox_typed_score =
          HistoryBackend::IsTypedIncrement(row.transition);
      if (!UpdateVisitRow(row))
        return false;
    }
    if (!read.Succeeded() || !committer.Commit())
      return false;
  }
  return true;
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

}  // namespace history
