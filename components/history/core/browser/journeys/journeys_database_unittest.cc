// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_database.h"

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "sql/database.h"
#include "sql/sqlite_result_code_values.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {

namespace {

using testing::_;
using testing::AllOf;
using testing::Each;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Field;
using testing::Gt;
using testing::IsEmpty;
using testing::Optional;
using testing::Pair;
using testing::UnorderedElementsAre;
using testing::UnorderedElementsAreArray;

testing::Matcher<const JourneyRow&> MatchesJourney(const JourneyRow& expected) {
  return AllOf(
      Field("journey_id", &JourneyRow::journey_id, expected.journey_id),
      Field("title", &JourneyRow::title, expected.title),
      Field("creation_time", &JourneyRow::creation_time,
            expected.creation_time),
      Field("emoji", &JourneyRow::emoji, expected.emoji),
      Field("overview", &JourneyRow::overview, expected.overview),
      Field("short_overview", &JourneyRow::short_overview,
            expected.short_overview),
      Field("history_entries", &JourneyRow::history_entries,
            UnorderedElementsAreArray(expected.history_entries)),
      Field("continuation_queries", &JourneyRow::continuation_queries,
            UnorderedElementsAreArray(expected.continuation_queries)));
}

JourneyRow CreateTestJourney(const std::string& journey_id,
                             const std::string& title,
                             int64_t creation_time_micros) {
  return JourneyRow(
      journey_id, title,
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(
          base::Microseconds(creation_time_micros)),
      /*emoji=*/"✈️",
      /*overview=*/"Trip overview",
      /*short_overview=*/"Short overview",
      /*history_entries=*/
      {JourneyHistoryEntry(
           base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000))),
       JourneyHistoryEntry(
           base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000)))},
      /*continuation_queries=*/
      {JourneyContinuationQuery("Next flights", "Find more flights"),
       JourneyContinuationQuery("Hotels", "Find hotels in Paris")});
}

class JourneysDatabaseTest : public testing::Test, public JourneysDatabase {
 public:
  JourneysDatabaseTest() {
    EXPECT_TRUE(db_.OpenInMemory());
    EXPECT_TRUE(InitJourneysTables());
  }

  JourneysDatabaseTest(const JourneysDatabaseTest&) = delete;
  JourneysDatabaseTest& operator=(const JourneysDatabaseTest&) = delete;

  ~JourneysDatabaseTest() override { db_.Close(); }

  JourneysDatabase* journeys_db() { return this; }

  // Returns the names of all tables in the database, excluding SQLite's
  // internal tables (e.g. `sqlite_sequence`, `sqlite_stat1`).
  std::vector<std::string> GetAllTableNames() {
    sql::Statement s(GetDB().GetUniqueStatement(
        "SELECT name FROM sqlite_schema WHERE type = 'table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name"));
    std::vector<std::string> names;
    while (s.Step()) {
      names.push_back(s.ColumnString(0));
    }
    return names;
  }

  // Returns the row count of every table, keyed by table name. Since the
  // schema is enumerated, tests using this automatically cover new tables.
  std::map<std::string, size_t> CountRowsPerTable() {
    std::map<std::string, size_t> counts;
    for (const std::string& table : GetAllTableNames()) {
      size_t count = 0;
      EXPECT_TRUE(sql::test::CountTableRows(&GetDB(), table.c_str(), &count))
          << table;
      counts[table] = count;
    }
    return counts;
  }

 protected:
  sql::Database& GetDB() override { return db_; }

 private:
  sql::Database db_{sql::test::kTestTag};
};

// Pins the schema that the tests enumerating tables below rely on, so that
// they can't pass vacuously on an empty table list. When adding a table, add
// it here and extend CreateTestJourney().
TEST_F(JourneysDatabaseTest, InitJourneysTablesCreatesAllTables) {
  EXPECT_THAT(GetAllTableNames(),
              UnorderedElementsAre("journeys", "journey_history_entries",
                                   "journey_continuation_queries"));
}

// CreateTestJourney() must populate every table, so that the deletion tests
// below cover all of them.
TEST_F(JourneysDatabaseTest, TestJourneyPopulatesAllTables) {
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({CreateTestJourney(
      "journey_1", "Trip 1", /*creation_time_micros=*/5000)}));
  EXPECT_THAT(CountRowsPerTable(), Each(Pair(_, Gt(0u))));
}

TEST_F(JourneysDatabaseTest, DropJourneysTables) {
  JourneyRow journey = CreateTestJourney("journey_1", "Trip to Paris",
                                         /*creation_time_micros=*/5000);
  ASSERT_TRUE(AddOrUpdateJourneys({journey}));
  EXPECT_TRUE(GetJourney("journey_1").has_value());

  EXPECT_TRUE(DropJourneysTables());
  EXPECT_THAT(GetAllTableNames(), IsEmpty());

  EXPECT_TRUE(InitJourneysTables());
  EXPECT_FALSE(GetJourney("journey_1").has_value());
  EXPECT_THAT(GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, EmptyDatabase) {
  EXPECT_FALSE(journeys_db()->GetJourney("non_existent").has_value());
  EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, EmptyBatchNoOp) {
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({}));
  EXPECT_TRUE(journeys_db()->DeleteJourneys({}));
}

TEST_F(JourneysDatabaseTest, AddAndGetJourneysBatch) {
  JourneyRow journey1 = CreateTestJourney("journey_1", "Trip to Paris",
                                          /*creation_time_micros=*/5000);
  JourneyRow journey2 = CreateTestJourney("journey_2", "Trip to London",
                                          /*creation_time_micros=*/6000);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));

  EXPECT_THAT(journeys_db()->GetJourney("journey_1"),
              Optional(MatchesJourney(journey1)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_2"),
              Optional(MatchesJourney(journey2)));
}

// History entries are read in visit time order and continuation queries in
// insertion order, from both getters.
TEST_F(JourneysDatabaseTest, ChildRowsAreReadInOrder) {
  const base::Time time1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  const base::Time time2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  const base::Time time3 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(3000));
  JourneyRow journey = CreateTestJourney("journey_1", "Trip to Paris",
                                         /*creation_time_micros=*/5000);
  journey.history_entries = {JourneyHistoryEntry(time3),
                             JourneyHistoryEntry(time1),
                             JourneyHistoryEntry(time2)};
  journey.continuation_queries = {
      JourneyContinuationQuery("Museums", "Find museums"),
      JourneyContinuationQuery("Hotels", "Find hotels"),
      JourneyContinuationQuery("Restaurants", "Find restaurants")};
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  const auto in_order = AllOf(
      Field("history_entries", &JourneyRow::history_entries,
            ElementsAre(JourneyHistoryEntry(time1), JourneyHistoryEntry(time2),
                        JourneyHistoryEntry(time3))),
      Field("continuation_queries", &JourneyRow::continuation_queries,
            ElementsAreArray(journey.continuation_queries)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_1"), Optional(in_order));
  EXPECT_THAT(journeys_db()->GetAllJourneys(), ElementsAre(in_order));
}

// Each journey gets only its own child rows, from both getters.
TEST_F(JourneysDatabaseTest, ChildRowsAttachToTheirOwnJourney) {
  JourneyRow paris = CreateTestJourney("journey_1", "Trip to Paris",
                                       /*creation_time_micros=*/5000);
  paris.history_entries = {JourneyHistoryEntry(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000)))};
  paris.continuation_queries = {
      JourneyContinuationQuery("Hotels", "Find hotels in Paris")};
  JourneyRow london = CreateTestJourney("journey_2", "Trip to London",
                                        /*creation_time_micros=*/6000);
  london.history_entries = {
      JourneyHistoryEntry(
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000))),
      JourneyHistoryEntry(
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(3000)))};
  london.continuation_queries = {
      JourneyContinuationQuery("Museums", "Find museums in London"),
      JourneyContinuationQuery("Theatre", "Find shows in London")};
  JourneyRow empty = CreateTestJourney("journey_3", "Empty Trip",
                                       /*creation_time_micros=*/4000);
  empty.history_entries.clear();
  empty.continuation_queries.clear();
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({paris, london, empty}));

  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(MatchesJourney(london), MatchesJourney(paris),
                          MatchesJourney(empty)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_1"),
              Optional(MatchesJourney(paris)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_2"),
              Optional(MatchesJourney(london)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_3"),
              Optional(MatchesJourney(empty)));
}

TEST_F(JourneysDatabaseTest, AddAndGetMinimalJourney) {
  JourneyRow minimal(
      "minimal_1", "Minimal Title",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(12345)));

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({minimal}));
  EXPECT_THAT(journeys_db()->GetJourney("minimal_1"),
              Optional(MatchesJourney(minimal)));
}

TEST_F(JourneysDatabaseTest, UpdateJourneysBatch) {
  JourneyRow journey = CreateTestJourney("journey_1", "Initial Title",
                                         /*creation_time_micros=*/5000);
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney("journey_1"),
              Optional(MatchesJourney(journey)));

  // Update with completely different history entries and continuation queries.
  journey.title = "Updated Title";
  journey.emoji = "🗼";
  journey.history_entries = {
      JourneyHistoryEntry(
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(3000))),
      JourneyHistoryEntry(
          base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(4000)))};
  journey.continuation_queries = {
      JourneyContinuationQuery("Car rentals", "Rent a car in Paris")};

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  // Verify outdated history entries (1000, 2000) and continuation queries
  // ("Next flights", "Hotels") are no longer present.
  EXPECT_THAT(journeys_db()->GetJourney("journey_1"),
              Optional(MatchesJourney(journey)));
  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(MatchesJourney(journey)));

  // Update again to clear all child entries to empty.
  journey.history_entries.clear();
  journey.continuation_queries.clear();
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney("journey_1"),
              Optional(MatchesJourney(journey)));
}

TEST_F(JourneysDatabaseTest, DeleteJourneysBatch) {
  JourneyRow journey1 =
      CreateTestJourney("journey_1", "Trip 1", /*creation_time_micros=*/5000);
  JourneyRow journey2 =
      CreateTestJourney("journey_2", "Trip 2", /*creation_time_micros=*/6000);
  JourneyRow journey3 =
      CreateTestJourney("journey_3", "Trip 3", /*creation_time_micros=*/7000);
  ASSERT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));
  const std::map<std::string, size_t> counts_before = CountRowsPerTable();
  ASSERT_THAT(counts_before, Each(Pair(_, Gt(0u))));

  // Delete journey_1 and a non-existent ID in a single batch.
  EXPECT_TRUE(journeys_db()->DeleteJourneys({"journey_1", "non_existent"}));
  EXPECT_FALSE(journeys_db()->GetJourney("journey_1").has_value());
  EXPECT_THAT(journeys_db()->GetJourney("journey_2"),
              Optional(MatchesJourney(journey2)));
  EXPECT_THAT(journeys_db()->GetJourney("journey_3"),
              Optional(MatchesJourney(journey3)));

  // All journeys have the same shape, so every table keeps exactly two thirds
  // of its rows.
  for (const auto& [table, count] : CountRowsPerTable()) {
    EXPECT_EQ(count * 3, counts_before.at(table) * 2) << table;
  }

  EXPECT_TRUE(journeys_db()->DeleteJourneys({"journey_2", "journey_3"}));
  EXPECT_THAT(CountRowsPerTable(), Each(Pair(_, 0u)));
}

TEST_F(JourneysDatabaseTest, DeleteJourneysAttemptsAllChildRowDeletions) {
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({CreateTestJourney(
      "journey_1", "Trip 1", /*creation_time_micros=*/5000)}));
  // Make the first child-row deletion fail.
  ASSERT_TRUE(GetDB().Execute("DROP TABLE journey_history_entries"));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kError);
    EXPECT_FALSE(journeys_db()->DeleteJourneys({"journey_1"}));
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }

  // The remaining tables are still cleared.
  EXPECT_THAT(CountRowsPerTable(), Each(Pair(_, 0u)));
}

TEST_F(JourneysDatabaseTest, DeleteAllJourneys) {
  JourneyRow journey1 =
      CreateTestJourney("journey_1", "Trip 1", /*creation_time_micros=*/5000);
  JourneyRow journey2 =
      CreateTestJourney("journey_2", "Trip 2", /*creation_time_micros=*/6000);
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));
  ASSERT_THAT(CountRowsPerTable(), Each(Pair(_, Gt(0u))));

  EXPECT_TRUE(journeys_db()->DeleteAllJourneys());
  EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
  EXPECT_THAT(CountRowsPerTable(), Each(Pair(_, 0u)));
}

TEST_F(JourneysDatabaseTest, GetAllJourneysSorted) {
  JourneyRow journey1 = CreateTestJourney("journey_1", "Older Trip",
                                          /*creation_time_micros=*/1000);
  JourneyRow journey2 = CreateTestJourney("journey_2", "Newer Trip",
                                          /*creation_time_micros=*/3000);
  JourneyRow journey3 = CreateTestJourney("journey_3", "Middle Trip",
                                          /*creation_time_micros=*/2000);

  EXPECT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));

  // Should be strictly ordered by creation_time DESC (journey2, journey3,
  // journey1).
  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(MatchesJourney(journey2), MatchesJourney(journey3),
                          MatchesJourney(journey1)));
}

// A child statement that fails while stepping makes both getters return no
// journeys instead of journeys with missing child rows.
TEST_F(JourneysDatabaseTest, ReadErrorReturnsNoJourneys) {
  ASSERT_TRUE(journeys_db()->AddOrUpdateJourneys({CreateTestJourney(
      "journey_1", "Trip 1", /*creation_time_micros=*/5000)}));
  // Replace `prompt` with a generated column whose expression overflows, so
  // reading a continuation query fails while stepping.
  ASSERT_TRUE(
      GetDB().Execute("ALTER TABLE journey_continuation_queries "
                      "RENAME COLUMN prompt TO stored_prompt"));
  ASSERT_TRUE(GetDB().Execute(
      "ALTER TABLE journey_continuation_queries ADD COLUMN prompt "
      "GENERATED ALWAYS AS (abs(-9223372036854775807 - 1)) VIRTUAL"));

  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(sql::SqliteResultCode::kError);
    EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
    EXPECT_FALSE(journeys_db()->GetJourney("journey_1").has_value());
    EXPECT_TRUE(expecter.SawExpectedErrors());
  }
}

TEST_F(JourneysDatabaseTest, DuplicateHistoryEntriesHandledGracefully) {
  JourneyRow journey = CreateTestJourney("journey_1", "Trip with Duplicates",
                                         /*creation_time_micros=*/5000);
  base::Time visit_time_1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  base::Time visit_time_2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  // Add duplicate visit timestamp 1000 (already in CreateTestJourney).
  journey.history_entries.emplace_back(visit_time_1);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  // The retrieved journey should contain only distinct timestamps.
  std::optional<JourneyRow> retrieved = journeys_db()->GetJourney("journey_1");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->history_entries.size(), 2u);
  EXPECT_THAT(retrieved->history_entries,
              UnorderedElementsAre(JourneyHistoryEntry(visit_time_1),
                                   JourneyHistoryEntry(visit_time_2)));
}

}  // namespace

}  // namespace history::journeys
