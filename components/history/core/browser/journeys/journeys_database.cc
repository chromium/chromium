// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_database.h"

#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace history::journeys {

namespace {

constexpr char kJourneysTableName[] = "journeys";
constexpr char kHistoryEntriesTableName[] = "journey_history_entries";
constexpr char kContinuationQueriesTableName[] = "journey_continuation_queries";

// Common column list for SELECT queries on the journeys table.
constexpr char kJourneyColumns[] =
    "journey_id, title, emoji, overview, short_overview, "
    "creation_time_micros";

// Populates top-level JourneyRow fields from a statement row matching
// `kJourneyColumns`.
JourneyRow ParseJourneyRow(sql::Statement& s) {
  JourneyRow journey;
  journey.journey_id = s.ColumnString(0);
  journey.title = s.ColumnString(1);
  if (s.GetColumnType(2) != sql::ColumnType::kNull) {
    journey.emoji = s.ColumnString(2);
  }
  if (s.GetColumnType(3) != sql::ColumnType::kNull) {
    journey.overview = s.ColumnString(3);
  }
  if (s.GetColumnType(4) != sql::ColumnType::kNull) {
    journey.short_overview = s.ColumnString(4);
  }
  journey.creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(s.ColumnInt64(5)));
  return journey;
}

}  // namespace

JourneysDatabase::JourneysDatabase() = default;

JourneysDatabase::~JourneysDatabase() = default;

// Note: This initializes tables for new databases (and unit tests). Schema
// upgrades on existing user databases must be coordinated with
// HistoryDatabase::EnsureCurrentVersion() by bumping kCurrentVersionNumber.
bool JourneysDatabase::InitJourneysTables() {
  // 1. Main journeys table.
  // Stores top-level journey metadata keyed by journey_id (GUID).
  // `creation_time_micros` stores microseconds since Windows epoch
  // consistent with `visits.visit_time`.
  if (!GetDB().DoesTableExist(kJourneysTableName)) {
    if (!GetDB().Execute(
            base::StrCat({"CREATE TABLE ", kJourneysTableName,
                          " (journey_id TEXT PRIMARY KEY NOT NULL, "
                          "title TEXT NOT NULL, "
                          "emoji TEXT, "
                          "overview TEXT, "
                          "short_overview TEXT, "
                          "creation_time_micros INTEGER NOT NULL)"}))) {
      return false;
    }

    // Index over creation_time so GetAllJourneys() can efficiently sort
    // journeys in reverse chronological order.
    if (!GetDB().Execute(base::StrCat(
            {"CREATE INDEX IF NOT EXISTS journeys_creation_time_idx ON ",
             kJourneysTableName, " (creation_time_micros)"}))) {
      return false;
    }
  }

  // 2. Child table: journey_history_entries.
  // Links a journey to its constituent visit timestamps, identifying the
  // local history visit.
  if (!GetDB().DoesTableExist(kHistoryEntriesTableName)) {
    if (!GetDB().Execute(
            base::StrCat({"CREATE TABLE ", kHistoryEntriesTableName,
                          " (journey_id TEXT NOT NULL, "
                          "visit_timestamp_micros INTEGER NOT NULL, "
                          "PRIMARY KEY (journey_id, visit_timestamp_micros)) "
                          "WITHOUT ROWID"}))) {
      return false;
    }

    // Index over visit timestamps to support fast reverse lookups and
    // efficient JOINs with `visits.visit_time` in VisitDatabase.
    if (!GetDB().Execute(base::StrCat(
            {"CREATE INDEX IF NOT EXISTS "
             "journey_history_entries_timestamp_idx ON ",
             kHistoryEntriesTableName, " (visit_timestamp_micros)"}))) {
      return false;
    }
  }

  // 3. Child table: journey_continuation_queries.
  if (!GetDB().DoesTableExist(kContinuationQueriesTableName)) {
    if (!GetDB().Execute(
            base::StrCat({"CREATE TABLE ", kContinuationQueriesTableName,
                          " (id INTEGER PRIMARY KEY, "
                          "journey_id TEXT NOT NULL, "
                          "title TEXT NOT NULL, "
                          "prompt TEXT NOT NULL)"}))) {
      return false;
    }

    // Index over journey_id so continuation queries can be efficiently fetched
    // and deleted for a given journey.
    if (!GetDB().Execute(
            base::StrCat({"CREATE INDEX IF NOT EXISTS "
                          "journey_continuation_queries_journey_id_idx ON ",
                          kContinuationQueriesTableName, " (journey_id)"}))) {
      return false;
    }
  }

  return true;
}

bool JourneysDatabase::DropJourneysTables() {
  return GetDB().Execute(base::StrCat(
             {"DROP TABLE IF EXISTS ", kHistoryEntriesTableName})) &&
         GetDB().Execute(base::StrCat(
             {"DROP TABLE IF EXISTS ", kContinuationQueriesTableName})) &&
         GetDB().Execute(
             base::StrCat({"DROP TABLE IF EXISTS ", kJourneysTableName}));
}

bool JourneysDatabase::AddOrUpdateJourneys(
    const std::vector<JourneyRow>& journeys) {
  if (journeys.empty()) {
    return true;
  }

  // Prepare SQL statements once outside the loop and reuse them across
  // iterations with Reset(true) for maximum batching efficiency.
  sql::Statement insert_journey(GetDB().GetUniqueStatement(
      base::StrCat({"INSERT OR REPLACE INTO ", kJourneysTableName, " (",
                    kJourneyColumns, ") VALUES(?, ?, ?, ?, ?, ?)"})));

  sql::Statement delete_entries(GetDB().GetUniqueStatement(base::StrCat(
      {"DELETE FROM ", kHistoryEntriesTableName, " WHERE journey_id = ?"})));

  sql::Statement delete_queries(GetDB().GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kContinuationQueriesTableName,
                    " WHERE journey_id = ?"})));

  sql::Statement insert_entry(GetDB().GetUniqueStatement(
      base::StrCat({"INSERT OR IGNORE INTO ", kHistoryEntriesTableName,
                    " (journey_id, visit_timestamp_micros) VALUES(?, ?)"})));

  sql::Statement insert_query(GetDB().GetUniqueStatement(
      base::StrCat({"INSERT INTO ", kContinuationQueriesTableName,
                    " (journey_id, title, prompt) VALUES(?, ?, ?)"})));

  for (const JourneyRow& journey : journeys) {
    if (journey.journey_id.empty()) {
      continue;
    }

    // 1. Insert or replace top-level journey metadata in `journeys`.
    // Bind NULL when optional nullable fields are unset.
    insert_journey.Reset(true);
    insert_journey.BindString(0, journey.journey_id);
    insert_journey.BindString(1, journey.title);
    if (journey.emoji.has_value()) {
      insert_journey.BindString(2, *journey.emoji);
    } else {
      insert_journey.BindNull(2);
    }
    if (journey.overview.has_value()) {
      insert_journey.BindString(3, *journey.overview);
    } else {
      insert_journey.BindNull(3);
    }
    if (journey.short_overview.has_value()) {
      insert_journey.BindString(4, *journey.short_overview);
    } else {
      insert_journey.BindNull(4);
    }
    insert_journey.BindInt64(
        5, journey.creation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
    if (!insert_journey.Run()) {
      return false;
    }

    // 2. Clear old child rows in case of update to avoid orphaned entries.
    delete_entries.Reset(true);
    delete_entries.BindString(0, journey.journey_id);
    if (!delete_entries.Run()) {
      return false;
    }

    delete_queries.Reset(true);
    delete_queries.BindString(0, journey.journey_id);
    if (!delete_queries.Run()) {
      return false;
    }

    // 3. Insert history entries.
    for (const JourneyHistoryEntry& entry : journey.history_entries) {
      insert_entry.Reset(true);
      insert_entry.BindString(0, journey.journey_id);
      insert_entry.BindInt64(
          1, entry.visit_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
      if (!insert_entry.Run()) {
        return false;
      }
    }

    // 4. Insert continuation queries.
    for (const JourneyContinuationQuery& query : journey.continuation_queries) {
      insert_query.Reset(true);
      insert_query.BindString(0, journey.journey_id);
      insert_query.BindString(1, query.title);
      insert_query.BindString(2, query.prompt);
      if (!insert_query.Run()) {
        return false;
      }
    }
  }

  return true;
}

std::optional<JourneyRow> JourneysDatabase::GetJourney(
    const std::string& journey_id) {
  if (journey_id.empty()) {
    return std::nullopt;
  }

  // 1. Fetch main journey fields.
  sql::Statement s_journey(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT ", kJourneyColumns, " FROM ", kJourneysTableName,
                    " WHERE journey_id = ?"})));
  s_journey.BindString(0, journey_id);

  if (!s_journey.Step()) {
    return std::nullopt;
  }

  JourneyRow journey = ParseJourneyRow(s_journey);

  // 2. Fetch history entries.
  sql::Statement s_entries(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT visit_timestamp_micros FROM ",
                    kHistoryEntriesTableName, " WHERE journey_id = ?"})));
  s_entries.BindString(0, journey_id);

  while (s_entries.Step()) {
    journey.history_entries.emplace_back(base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s_entries.ColumnInt64(0))));
  }

  // 3. Fetch continuation queries.
  sql::Statement s_queries(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT title, prompt FROM ", kContinuationQueriesTableName,
                    " WHERE journey_id = ?"})));
  s_queries.BindString(0, journey_id);

  while (s_queries.Step()) {
    journey.continuation_queries.emplace_back(s_queries.ColumnString(0),
                                              s_queries.ColumnString(1));
  }

  return journey;
}

std::vector<JourneyRow> JourneysDatabase::GetAllJourneys() {
  std::vector<JourneyRow> journeys;
  absl::flat_hash_map<std::string, size_t> journey_id_to_index;

  // 1. Read all top-level journeys ordered by creation time descending.
  sql::Statement s_journeys(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT ", kJourneyColumns, " FROM ", kJourneysTableName,
                    " ORDER BY creation_time_micros DESC"})));

  while (s_journeys.Step()) {
    JourneyRow journey = ParseJourneyRow(s_journeys);
    journey_id_to_index[journey.journey_id] = journeys.size();
    journeys.push_back(std::move(journey));
  }

  if (journeys.empty()) {
    return journeys;
  }

  // 2. Fetch all history entries in a single batch query and attach to
  // journeys.
  sql::Statement s_entries(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT journey_id, visit_timestamp_micros FROM ",
                    kHistoryEntriesTableName})));

  while (s_entries.Step()) {
    std::string journey_id = s_entries.ColumnString(0);
    auto it = journey_id_to_index.find(journey_id);
    if (it != journey_id_to_index.end()) {
      journeys[it->second].history_entries.emplace_back(
          base::Time::FromDeltaSinceWindowsEpoch(
              base::Microseconds(s_entries.ColumnInt64(1))));
    }
  }

  // 3. Fetch all continuation queries in a single batch query and attach.
  sql::Statement s_queries(GetDB().GetUniqueStatement(
      base::StrCat({"SELECT journey_id, title, prompt FROM ",
                    kContinuationQueriesTableName})));

  while (s_queries.Step()) {
    std::string journey_id = s_queries.ColumnString(0);
    auto it = journey_id_to_index.find(journey_id);
    if (it != journey_id_to_index.end()) {
      journeys[it->second].continuation_queries.emplace_back(
          s_queries.ColumnString(1), s_queries.ColumnString(2));
    }
  }

  return journeys;
}

// TODO(crbug.com/526686844): When history is cleared or old visits expire,
// ensure matching rows in `journey_history_entries` are deleted as well,
// and cascade deletions to the respective journey and all its associated
// history entries / continuation queries. A future CL should cascade visit
// deletions.
bool JourneysDatabase::DeleteJourneys(
    const std::vector<std::string>& journey_ids) {
  if (journey_ids.empty()) {
    return true;
  }

  // Prepare delete statements once and reuse across IDs in the batch.
  sql::Statement s_journey(GetDB().GetUniqueStatement(base::StrCat(
      {"DELETE FROM ", kJourneysTableName, " WHERE journey_id = ?"})));

  sql::Statement s_entries(GetDB().GetUniqueStatement(base::StrCat(
      {"DELETE FROM ", kHistoryEntriesTableName, " WHERE journey_id = ?"})));

  sql::Statement s_queries(GetDB().GetUniqueStatement(
      base::StrCat({"DELETE FROM ", kContinuationQueriesTableName,
                    " WHERE journey_id = ?"})));

  for (const std::string& journey_id : journey_ids) {
    if (journey_id.empty()) {
      continue;
    }

    s_journey.Reset(true);
    s_journey.BindString(0, journey_id);
    if (!s_journey.Run()) {
      return false;
    }

    s_entries.Reset(true);
    s_entries.BindString(0, journey_id);
    if (!s_entries.Run()) {
      return false;
    }

    s_queries.Reset(true);
    s_queries.BindString(0, journey_id);
    if (!s_queries.Run()) {
      return false;
    }
  }

  return true;
}

bool JourneysDatabase::DeleteAllJourneys() {
  return GetDB().Execute(base::StrCat({"DELETE FROM ", kJourneysTableName})) &&
         GetDB().Execute(
             base::StrCat({"DELETE FROM ", kHistoryEntriesTableName})) &&
         GetDB().Execute(
             base::StrCat({"DELETE FROM ", kContinuationQueriesTableName}));
}

}  // namespace history::journeys
