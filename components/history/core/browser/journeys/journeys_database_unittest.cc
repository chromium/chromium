// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_database.h"

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {

namespace {

using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;

JourneyRow CreateTestJourney(const std::string& journey_id,
                             const std::string& title,
                             int64_t creation_time_micros) {
  JourneyRow journey;
  journey.journey_id = journey_id;
  journey.title = title;
  journey.emoji = "✈️";
  journey.overview = "Trip overview";
  journey.short_overview = "Short overview";
  journey.creation_time = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(creation_time_micros));

  journey.history_entries.emplace_back(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000)));
  journey.history_entries.emplace_back(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000)));

  journey.continuation_queries.emplace_back("Next flights",
                                            "Find more flights");
  journey.continuation_queries.emplace_back("Hotels", "Find hotels in Paris");

  return journey;
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

 protected:
  sql::Database& GetDB() override { return db_; }

 private:
  sql::Database db_{sql::test::kTestTag};
};

TEST_F(JourneysDatabaseTest, DropJourneysTables) {
  JourneyRow journey = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip to Paris",
      /*creation_time_micros=*/5000);
  EXPECT_TRUE(AddOrUpdateJourneys({journey}));
  EXPECT_TRUE(GetJourney("journey_1").has_value());

  EXPECT_TRUE(DropJourneysTables());
  EXPECT_TRUE(InitJourneysTables());
  EXPECT_FALSE(GetJourney("journey_1").has_value());
  EXPECT_THAT(GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, EmptyDatabase) {
  EXPECT_FALSE(
      journeys_db()->GetJourney(/*journey_id=*/"non_existent").has_value());
  EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, EmptyBatchNoOp) {
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({}));
  EXPECT_TRUE(journeys_db()->DeleteJourneys({}));
}

TEST_F(JourneysDatabaseTest, AddAndGetJourneysBatch) {
  JourneyRow journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip to Paris",
      /*creation_time_micros=*/5000);
  JourneyRow journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip to London",
      /*creation_time_micros=*/6000);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));

  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(journey1));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_2"),
              Optional(journey2));
}

TEST_F(JourneysDatabaseTest, AddAndGetMinimalJourney) {
  JourneyRow minimal;
  minimal.journey_id = "minimal_1";
  minimal.title = "Minimal Title";
  minimal.creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(12345));

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({minimal}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"minimal_1"),
              Optional(minimal));
}

TEST_F(JourneysDatabaseTest, UpdateJourneysBatch) {
  JourneyRow journey = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Initial Title",
      /*creation_time_micros=*/5000);
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(journey));

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
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(journey));
  EXPECT_THAT(journeys_db()->GetAllJourneys(), ElementsAre(journey));

  // Update again to clear all child entries to empty.
  journey.history_entries.clear();
  journey.continuation_queries.clear();
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(journey));
}

TEST_F(JourneysDatabaseTest, DeleteJourneysBatch) {
  JourneyRow journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip 1",
      /*creation_time_micros=*/5000);
  JourneyRow journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip 2",
      /*creation_time_micros=*/6000);
  JourneyRow journey3 = CreateTestJourney(
      /*journey_id=*/"journey_3", /*title=*/"Trip 3",
      /*creation_time_micros=*/7000);
  EXPECT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));

  // Delete journey_1 and a non-existent ID in a single batch.
  EXPECT_TRUE(journeys_db()->DeleteJourneys({"journey_1", "non_existent"}));
  EXPECT_FALSE(
      journeys_db()->GetJourney(/*journey_id=*/"journey_1").has_value());
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_2"),
              Optional(journey2));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_3"),
              Optional(journey3));
}

TEST_F(JourneysDatabaseTest, DeleteAllJourneys) {
  JourneyRow journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip 1",
      /*creation_time_micros=*/5000);
  JourneyRow journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip 2",
      /*creation_time_micros=*/6000);
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));

  EXPECT_TRUE(journeys_db()->DeleteAllJourneys());
  EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, GetAllJourneysSorted) {
  JourneyRow journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Older Trip",
      /*creation_time_micros=*/1000);
  JourneyRow journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Newer Trip",
      /*creation_time_micros=*/3000);
  JourneyRow journey3 = CreateTestJourney(
      /*journey_id=*/"journey_3", /*title=*/"Middle Trip",
      /*creation_time_micros=*/2000);

  EXPECT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));

  // Should be strictly ordered by creation_time DESC (journey2, journey3,
  // journey1).
  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(journey2, journey3, journey1));
}

TEST_F(JourneysDatabaseTest, DuplicateHistoryEntriesHandledGracefully) {
  JourneyRow journey = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip with Duplicates",
      /*creation_time_micros=*/5000);
  // Add duplicate visit timestamp 1000 (already in CreateTestJourney).
  journey.history_entries.emplace_back(
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000)));

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  // The retrieved journey should contain only distinct timestamps.
  std::optional<JourneyRow> retrieved =
      journeys_db()->GetJourney(/*journey_id=*/"journey_1");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->history_entries.size(), 2u);
}

}  // namespace

}  // namespace history::journeys
