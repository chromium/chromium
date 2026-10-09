// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_database.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
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
constexpr char kCollectionsTableName[] = "journey_collections";
constexpr char kCollectionItemsTableName[] = "journey_collection_items";

// Populates top-level JourneyRow fields from a statement row.
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
  journey.creation_time = s.ColumnTime(5);
  return journey;
}

// Runs `statement`, whose only parameter is a journey ID, for `journey_id`.
bool RunForJourney(sql::Statement& statement, const std::string& journey_id) {
  statement.Reset(true);
  statement.BindString(0, journey_id);
  return statement.Run();
}

// Reads journeys and their child rows from prepared, bound statements.
// `journeys_statement` returns the columns ParseJourneyRow() reads; each
// child statement returns journey_id first. Child rows of journeys that
// `journeys_statement` did not return are ignored. Journeys are returned in
// the order `journeys_statement` returns them. If any statement fails, no
// journeys are returned, rather than journeys with missing child rows.
//
// `collections` LEFT JOINs journey_collection_items onto journey_collections,
// returns journey_id, position, title and visit_timestamp_micros (NULL for a
// collection without items), and is ordered by journey_id, collection position
// and item position.
std::vector<JourneyRow> ReadJourneys(sql::Statement& journeys_statement,
                                     sql::Statement& entries,
                                     sql::Statement& queries,
                                     sql::Statement& collections) {
  std::vector<JourneyRow> journeys;
  absl::flat_hash_map<std::string, size_t> journey_id_to_index;

  // 1. Read the top-level journeys.
  while (journeys_statement.Step()) {
    JourneyRow journey = ParseJourneyRow(journeys_statement);
    journey_id_to_index[journey.journey_id] = journeys.size();
    journeys.push_back(std::move(journey));
  }
  if (!journeys_statement.Succeeded()) {
    return {};
  }

  if (journeys.empty()) {
    return journeys;
  }

  // `journeys` doesn't grow from here on, so the returned pointers stay valid.
  auto find_journey = [&](std::string_view journey_id) -> JourneyRow* {
    auto it = journey_id_to_index.find(journey_id);
    return it != journey_id_to_index.end() ? &journeys[it->second] : nullptr;
  };

  // 2. Attach history entries.
  while (entries.Step()) {
    const std::string_view journey_id = entries.ColumnStringView(0);
    if (JourneyRow* journey = find_journey(journey_id)) {
      journey->history_entries.emplace_back(entries.ColumnTime(1));
    } else {
      // TODO(crbug.com/526686844): Consider recording orphaned history
      // entries in a histogram.
      VLOG(1) << "Dropping history entry of unknown journey_id=" << journey_id;
    }
  }
  if (!entries.Succeeded()) {
    return {};
  }

  // 3. Attach continuation queries.
  while (queries.Step()) {
    const std::string_view journey_id = queries.ColumnStringView(0);
    if (JourneyRow* journey = find_journey(journey_id)) {
      journey->continuation_queries.emplace_back(queries.ColumnString(1),
                                                 queries.ColumnString(2));
    } else {
      // TODO(crbug.com/526686844): Consider recording orphaned continuation
      // queries in a histogram.
      VLOG(1) << "Dropping continuation query of unknown journey_id="
              << journey_id;
    }
  }
  if (!queries.Succeeded()) {
    return {};
  }

  // 4. Attach collections and their items in display order. Positions only
  // order rows, so gaps are fine. Items only attach to the collection they
  // reference; items whose collection row is missing are dropped. Rows are
  // grouped by journey, so each journey is looked up once. The current_*
  // variables hold the journey ID and collection position of the previous row.
  std::optional<std::string> current_journey_id;
  std::optional<int64_t> current_position;
  JourneyRow* journey = nullptr;
  while (collections.Step()) {
    const std::string_view journey_id = collections.ColumnStringView(0);
    if (journey_id != current_journey_id) {
      current_journey_id = std::string(journey_id);
      current_position.reset();
      journey = find_journey(journey_id);
      if (!journey) {
        // TODO(crbug.com/568227865): Consider recording orphaned collection
        // rows in a histogram.
        VLOG(1) << "Dropping collections of unknown journey_id=" << journey_id;
      }
    }
    const int64_t position = collections.ColumnInt64(1);
    const bool starts_collection = position != current_position;
    current_position = position;
    if (!journey) {
      continue;
    }
    if (starts_collection) {
      journey->collections.emplace_back().title = collections.ColumnString(2);
    }
    if (collections.GetColumnType(3) != sql::ColumnType::kNull) {
      journey->collections.back().items.emplace_back(collections.ColumnTime(3));
    }
  }
  if (!collections.Succeeded()) {
    return {};
  }

  return journeys;
}

// Inserts `journey.history_entries`. Duplicate visits are ignored.
bool InsertHistoryEntries(sql::Database& db, const JourneyRow& journey) {
  sql::Statement insert_entry(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO journey_history_entries "
      "(journey_id, visit_timestamp_micros) VALUES(?, ?)"));
  for (const JourneyHistoryEntry& entry : journey.history_entries) {
    insert_entry.Reset(true);
    insert_entry.BindString(0, journey.journey_id);
    insert_entry.BindTime(1, entry.visit_time);
    if (!insert_entry.Run()) {
      return false;
    }
  }
  return true;
}

// Inserts `journey.continuation_queries`, in order.
bool InsertContinuationQueries(sql::Database& db, const JourneyRow& journey) {
  sql::Statement insert_query(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "INSERT INTO journey_continuation_queries "
                            "(journey_id, title, prompt) VALUES(?, ?, ?)"));
  for (const JourneyContinuationQuery& query : journey.continuation_queries) {
    insert_query.Reset(true);
    insert_query.BindString(0, journey.journey_id);
    insert_query.BindString(1, query.title);
    insert_query.BindString(2, query.prompt);
    if (!insert_query.Run()) {
      return false;
    }
  }
  return true;
}

// Inserts `journey.collections` and their items, keeping display order.
// Repeated visits within a collection are stored only once.
bool InsertCollections(sql::Database& db, const JourneyRow& journey) {
  sql::Statement insert_collection(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO journey_collections (journey_id, position, title) "
      "VALUES(?, ?, ?)"));
  sql::Statement insert_item(db.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO journey_collection_items (journey_id, "
      "collection_position, item_position, visit_timestamp_micros) "
      "VALUES(?, ?, ?, ?)"));

  // Positions define display order. They are written contiguously, but reads
  // don't rely on that.
  for (size_t i = 0; i < journey.collections.size(); ++i) {
    const JourneyHistoryEntryCollection& collection = journey.collections[i];
    insert_collection.Reset(true);
    insert_collection.BindString(0, journey.journey_id);
    insert_collection.BindInt64(1, static_cast<int64_t>(i));
    insert_collection.BindString(2, collection.title);
    if (!insert_collection.Run()) {
      return false;
    }

    base::flat_set<base::Time> seen_visit_times;
    int64_t item_position = 0;
    for (const JourneyHistoryEntry& item : collection.items) {
      // Skip visits already in this collection, so each visit is stored once,
      // at its first position.
      if (!seen_visit_times.insert(item.visit_time).second) {
        continue;
      }
      insert_item.Reset(true);
      insert_item.BindString(0, journey.journey_id);
      insert_item.BindInt64(1, static_cast<int64_t>(i));
      insert_item.BindInt64(2, item_position++);
      insert_item.BindTime(3, item.visit_time);
      if (!insert_item.Run()) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

JourneysDatabase::JourneysDatabase() = default;

JourneysDatabase::~JourneysDatabase() = default;

bool JourneysDatabase::InitJourneysTables() {
  // 1. Main journeys table.
  // Stores top-level journey metadata keyed by journey_id (GUID).
  // `creation_time_micros` stores microseconds since Windows epoch
  // consistent with `visits.visit_time`.
  if (!GetDB().DoesTableExist(kJourneysTableName)) {
    static constexpr char kCreateJourneysTableSql[] =
        "CREATE TABLE journeys "
        "(journey_id TEXT PRIMARY KEY NOT NULL, "
        "title TEXT NOT NULL, "
        "emoji TEXT, "
        "overview TEXT, "
        "short_overview TEXT, "
        "creation_time_micros INTEGER NOT NULL)";
    if (!GetDB().Execute(kCreateJourneysTableSql)) {
      return false;
    }

    // Index over creation_time so GetAllJourneys() can efficiently sort
    // journeys in reverse chronological order.
    static constexpr char kCreateJourneysTimeIndexSql[] =
        "CREATE INDEX IF NOT EXISTS journeys_creation_time_idx ON "
        "journeys (creation_time_micros)";
    if (!GetDB().Execute(kCreateJourneysTimeIndexSql)) {
      return false;
    }
  }

  // 2. Child table: journey_history_entries.
  // Links a journey to its constituent visit timestamps, identifying the
  // local history visit.
  if (!GetDB().DoesTableExist(kHistoryEntriesTableName)) {
    static constexpr char kCreateEntriesTableSql[] =
        "CREATE TABLE journey_history_entries "
        "(journey_id TEXT NOT NULL, "
        "visit_timestamp_micros INTEGER NOT NULL, "
        "PRIMARY KEY (journey_id, visit_timestamp_micros)) "
        "WITHOUT ROWID";
    if (!GetDB().Execute(kCreateEntriesTableSql)) {
      return false;
    }

    // Index over visit timestamps to support fast reverse lookups and
    // efficient JOINs with `visits.visit_time` in VisitDatabase.
    static constexpr char kCreateEntriesTimestampIndexSql[] =
        "CREATE INDEX IF NOT EXISTS "
        "journey_history_entries_timestamp_idx ON "
        "journey_history_entries (visit_timestamp_micros)";
    if (!GetDB().Execute(kCreateEntriesTimestampIndexSql)) {
      return false;
    }
  }

  // 3. Child table: journey_continuation_queries.
  if (!GetDB().DoesTableExist(kContinuationQueriesTableName)) {
    static constexpr char kCreateQueriesTableSql[] =
        "CREATE TABLE journey_continuation_queries "
        "(id INTEGER PRIMARY KEY, "
        "journey_id TEXT NOT NULL, "
        "title TEXT NOT NULL, "
        "prompt TEXT NOT NULL)";
    if (!GetDB().Execute(kCreateQueriesTableSql)) {
      return false;
    }

    // Index over journey_id so continuation queries can be efficiently fetched
    // and deleted for a given journey.
    static constexpr char kCreateQueriesIndexSql[] =
        "CREATE INDEX IF NOT EXISTS "
        "journey_continuation_queries_journey_id_idx ON "
        "journey_continuation_queries (journey_id)";
    if (!GetDB().Execute(kCreateQueriesIndexSql)) {
      return false;
    }
  }

  // 4. Child table: journey_collections.
  // `position` is the display order of the collection within its journey.
  // The primary key starts with `journey_id`, so it also serves per-journey
  // lookups and deletions; no extra index is needed. WITHOUT ROWID stores the
  // small rows directly in the primary key B-tree.
  if (!GetDB().DoesTableExist(kCollectionsTableName)) {
    static constexpr char kCreateCollectionsTableSql[] =
        "CREATE TABLE journey_collections "
        "(journey_id TEXT NOT NULL, "
        "position INTEGER NOT NULL, "
        "title TEXT NOT NULL, "
        "PRIMARY KEY (journey_id, position)) "
        "WITHOUT ROWID";
    if (!GetDB().Execute(kCreateCollectionsTableSql)) {
      return false;
    }
  }

  // 5. Child table: journey_collection_items.
  // Each item references one of the journey's history entries by visit
  // timestamp. `collection_position` matches `journey_collections.position`
  // and `item_position` is the display order of the item within its collection.
  // As above, the primary key serves per-journey lookups and deletions.
  if (!GetDB().DoesTableExist(kCollectionItemsTableName)) {
    static constexpr char kCreateCollectionItemsTableSql[] =
        "CREATE TABLE journey_collection_items "
        "(journey_id TEXT NOT NULL, "
        "collection_position INTEGER NOT NULL, "
        "item_position INTEGER NOT NULL, "
        "visit_timestamp_micros INTEGER NOT NULL, "
        "PRIMARY KEY (journey_id, collection_position, item_position)) "
        "WITHOUT ROWID";
    if (!GetDB().Execute(kCreateCollectionItemsTableSql)) {
      return false;
    }
  }

  return true;
}

bool JourneysDatabase::DropJourneysTables() {
  return GetDB().Execute("DROP TABLE IF EXISTS journeys") &&
         GetDB().Execute("DROP TABLE IF EXISTS journey_history_entries") &&
         GetDB().Execute("DROP TABLE IF EXISTS journey_continuation_queries") &&
         GetDB().Execute("DROP TABLE IF EXISTS journey_collections") &&
         GetDB().Execute("DROP TABLE IF EXISTS journey_collection_items");
}

bool JourneysDatabase::AddOrUpdateJourneys(
    const std::vector<JourneyRow>& journeys) {
  if (journeys.empty()) {
    return true;
  }

  // Prepare SQL statements once outside the loop and reuse them across
  // iterations with Reset(true) for maximum batching efficiency.
  sql::Statement insert_journey(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO journeys (journey_id, title, emoji, overview, "
      "short_overview, creation_time_micros) VALUES(?, ?, ?, ?, ?, ?)"));

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
    insert_journey.BindTime(5, journey.creation_time);
    if (!insert_journey.Run()) {
      return false;
    }

    // 2. Replace the child rows wholesale, so that an update leaves no stale
    // rows behind.
    if (!DeleteChildRows(journey.journey_id) || !InsertChildRows(journey)) {
      return false;
    }
  }

  return true;
}

std::optional<JourneyRow> JourneysDatabase::GetJourney(
    const std::string& journey_id) {
  if (journey_id.empty()) {
    return std::nullopt;
  }

  sql::Statement s_journey(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, title, emoji, overview, short_overview, "
      "creation_time_micros FROM journeys WHERE journey_id = ?"));
  s_journey.BindString(0, journey_id);

  sql::Statement s_entries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, visit_timestamp_micros FROM journey_history_entries "
      "WHERE journey_id = ? ORDER BY visit_timestamp_micros"));
  s_entries.BindString(0, journey_id);

  sql::Statement s_queries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, title, prompt FROM journey_continuation_queries "
      "WHERE journey_id = ? ORDER BY id"));
  s_queries.BindString(0, journey_id);

  sql::Statement s_collections(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT collection.journey_id, collection.position, collection.title, "
      "item.visit_timestamp_micros "
      "FROM journey_collections AS collection "
      "LEFT JOIN journey_collection_items AS item "
      "ON item.journey_id = collection.journey_id "
      "AND item.collection_position = collection.position "
      "WHERE collection.journey_id = ? "
      "ORDER BY collection.position, item.item_position"));
  s_collections.BindString(0, journey_id);

  std::vector<JourneyRow> journeys =
      ReadJourneys(s_journey, s_entries, s_queries, s_collections);
  if (journeys.empty()) {
    return std::nullopt;
  }
  // `journey_id` is the primary key.
  DCHECK_EQ(journeys.size(), 1u);
  return std::move(journeys.front());
}

std::vector<JourneyRow> JourneysDatabase::GetAllJourneys() {
  sql::Statement s_journeys(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, title, emoji, overview, short_overview, "
      "creation_time_micros FROM journeys "
      "ORDER BY creation_time_micros DESC"));

  sql::Statement s_entries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, visit_timestamp_micros FROM "
      "journey_history_entries ORDER BY journey_id, visit_timestamp_micros"));

  sql::Statement s_queries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT journey_id, title, prompt FROM journey_continuation_queries "
      "ORDER BY id"));

  sql::Statement s_collections(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT collection.journey_id, collection.position, collection.title, "
      "item.visit_timestamp_micros "
      "FROM journey_collections AS collection "
      "LEFT JOIN journey_collection_items AS item "
      "ON item.journey_id = collection.journey_id "
      "AND item.collection_position = collection.position "
      "ORDER BY collection.journey_id, collection.position, "
      "item.item_position"));

  return ReadJourneys(s_journeys, s_entries, s_queries, s_collections);
}

// TODO(crbug.com/526686844): When history is cleared or old visits expire,
// ensure matching rows in `journey_history_entries` and
// `journey_collection_items` are deleted as well, and cascade deletions to the
// respective journey and all its child rows.
bool JourneysDatabase::DeleteJourneys(
    const std::vector<std::string>& journey_ids) {
  if (journey_ids.empty()) {
    return true;
  }

  // Prepare delete statements once and reuse across IDs in the batch.
  sql::Statement s_journey(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM journeys WHERE journey_id = ?"));

  bool all_succeeded = true;
  for (const std::string& journey_id : journey_ids) {
    if (journey_id.empty()) {
      continue;
    }

    if (!RunForJourney(s_journey, journey_id)) {
      all_succeeded = false;
    }
    if (!DeleteChildRows(journey_id)) {
      all_succeeded = false;
    }
  }

  return all_succeeded;
}

bool JourneysDatabase::DeleteAllJourneys() {
  return GetDB().Execute("DELETE FROM journeys") &&
         GetDB().Execute("DELETE FROM journey_history_entries") &&
         GetDB().Execute("DELETE FROM journey_continuation_queries") &&
         GetDB().Execute("DELETE FROM journey_collections") &&
         GetDB().Execute("DELETE FROM journey_collection_items");
}

bool JourneysDatabase::InsertChildRows(const JourneyRow& journey) {
  return InsertHistoryEntries(GetDB(), journey) &&
         InsertContinuationQueries(GetDB(), journey) &&
         InsertCollections(GetDB(), journey);
}

bool JourneysDatabase::DeleteChildRows(const std::string& journey_id) {
  sql::Statement delete_history_entries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM journey_history_entries WHERE journey_id = ?"));
  sql::Statement delete_continuation_queries(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM journey_continuation_queries WHERE journey_id = ?"));
  sql::Statement delete_collections(GetDB().GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM journey_collections WHERE journey_id = ?"));
  sql::Statement delete_collection_items(GetDB().GetCachedStatement(
      SQL_FROM_HERE,
      "DELETE FROM journey_collection_items WHERE journey_id = ?"));

  // Attempt every deletion, even if an earlier one fails.
  const bool deleted_history_entries =
      RunForJourney(delete_history_entries, journey_id);
  const bool deleted_continuation_queries =
      RunForJourney(delete_continuation_queries, journey_id);
  const bool deleted_collections =
      RunForJourney(delete_collections, journey_id);
  const bool deleted_collection_items =
      RunForJourney(delete_collection_items, journey_id);
  return deleted_history_entries && deleted_continuation_queries &&
         deleted_collections && deleted_collection_items;
}

}  // namespace history::journeys
