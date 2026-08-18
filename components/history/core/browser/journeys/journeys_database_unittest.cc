// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_database.h"

#include <optional>
#include <string>
#include <vector>

#include "base/test/protobuf_matchers.h"
#include "components/sync/protocol/journey_specifics.pb.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history::journeys {

namespace {

using base::test::EqualsProto;
using sync_pb::JourneySpecifics;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Optional;

JourneySpecifics CreateTestJourney(const std::string& journey_id,
                                   const std::string& title,
                                   int64_t creation_time_micros) {
  JourneySpecifics journey;
  journey.set_journey_id(journey_id);
  journey.set_title(title);
  journey.set_emoji("✈️");
  journey.set_overview("Trip overview");
  journey.set_short_overview("Short overview");
  journey.set_creation_time_windows_epoch_micros(creation_time_micros);

  JourneySpecifics::HistoryEntry* entry1 = journey.add_history_entries();
  entry1->set_visit_timestamp_windows_epoch_micros(1000);
  JourneySpecifics::HistoryEntry* entry2 = journey.add_history_entries();
  entry2->set_visit_timestamp_windows_epoch_micros(2000);

  JourneySpecifics::ContinuationQuery* query1 =
      journey.add_continuation_queries();
  query1->set_title("Next flights");
  query1->set_prompt("Find more flights");

  JourneySpecifics::ContinuationQuery* query2 =
      journey.add_continuation_queries();
  query2->set_title("Hotels");
  query2->set_prompt("Find hotels in Paris");

  return journey;
}

class JourneysDatabaseTest : public testing::Test {
 public:
  JourneysDatabaseTest() : journeys_db_(&db_) {
    EXPECT_TRUE(db_.OpenInMemory());
    EXPECT_TRUE(journeys_db_.Init());
  }

  JourneysDatabaseTest(const JourneysDatabaseTest&) = delete;
  JourneysDatabaseTest& operator=(const JourneysDatabaseTest&) = delete;

  ~JourneysDatabaseTest() override { db_.Close(); }

  JourneysDatabase* journeys_db() { return &journeys_db_; }

 private:
  sql::Database db_{sql::test::kTestTag};
  JourneysDatabase journeys_db_;
};

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
  JourneySpecifics journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip to Paris",
      /*creation_time_micros=*/5000);
  JourneySpecifics journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip to London",
      /*creation_time_micros=*/6000);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));

  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(EqualsProto(journey1)));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_2"),
              Optional(EqualsProto(journey2)));
}

TEST_F(JourneysDatabaseTest, AddAndGetMinimalJourney) {
  JourneySpecifics minimal;
  minimal.set_journey_id("minimal_1");
  minimal.set_title("Minimal Title");
  minimal.set_creation_time_windows_epoch_micros(12345);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({minimal}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"minimal_1"),
              Optional(EqualsProto(minimal)));
}

TEST_F(JourneysDatabaseTest, UpdateJourneysBatch) {
  JourneySpecifics journey = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Initial Title",
      /*creation_time_micros=*/5000);
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(EqualsProto(journey)));

  // Update with completely different history entries and continuation queries.
  journey.set_title("Updated Title");
  journey.set_emoji("🗼");
  journey.clear_history_entries();
  journey.add_history_entries()->set_visit_timestamp_windows_epoch_micros(3000);
  journey.add_history_entries()->set_visit_timestamp_windows_epoch_micros(4000);
  journey.clear_continuation_queries();
  JourneySpecifics::ContinuationQuery* new_query =
      journey.add_continuation_queries();
  new_query->set_title("Car rentals");
  new_query->set_prompt("Rent a car in Paris");

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  // Verify outdated history entries (1000, 2000) and continuation queries
  // ("Next flights", "Hotels") are no longer present.
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(EqualsProto(journey)));
  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(EqualsProto(journey)));

  // Update again to clear all child entries to empty.
  journey.clear_history_entries();
  journey.clear_continuation_queries();
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_1"),
              Optional(EqualsProto(journey)));
}

TEST_F(JourneysDatabaseTest, DeleteJourneysBatch) {
  JourneySpecifics journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip 1",
      /*creation_time_micros=*/5000);
  JourneySpecifics journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip 2",
      /*creation_time_micros=*/6000);
  JourneySpecifics journey3 = CreateTestJourney(
      /*journey_id=*/"journey_3", /*title=*/"Trip 3",
      /*creation_time_micros=*/7000);
  EXPECT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));

  // Delete journey_1 and a non-existent ID in a single batch.
  EXPECT_TRUE(journeys_db()->DeleteJourneys({"journey_1", "non_existent"}));
  EXPECT_FALSE(
      journeys_db()->GetJourney(/*journey_id=*/"journey_1").has_value());
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_2"),
              Optional(EqualsProto(journey2)));
  EXPECT_THAT(journeys_db()->GetJourney(/*journey_id=*/"journey_3"),
              Optional(EqualsProto(journey3)));
}

TEST_F(JourneysDatabaseTest, DeleteAllJourneys) {
  JourneySpecifics journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip 1",
      /*creation_time_micros=*/5000);
  JourneySpecifics journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Trip 2",
      /*creation_time_micros=*/6000);
  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey1, journey2}));

  EXPECT_TRUE(journeys_db()->DeleteAllJourneys());
  EXPECT_THAT(journeys_db()->GetAllJourneys(), IsEmpty());
}

TEST_F(JourneysDatabaseTest, GetAllJourneysSorted) {
  JourneySpecifics journey1 = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Older Trip",
      /*creation_time_micros=*/1000);
  JourneySpecifics journey2 = CreateTestJourney(
      /*journey_id=*/"journey_2", /*title=*/"Newer Trip",
      /*creation_time_micros=*/3000);
  JourneySpecifics journey3 = CreateTestJourney(
      /*journey_id=*/"journey_3", /*title=*/"Middle Trip",
      /*creation_time_micros=*/2000);

  EXPECT_TRUE(
      journeys_db()->AddOrUpdateJourneys({journey1, journey2, journey3}));

  // Should be strictly ordered by creation_time DESC (journey2, journey3,
  // journey1).
  EXPECT_THAT(journeys_db()->GetAllJourneys(),
              ElementsAre(EqualsProto(journey2), EqualsProto(journey3),
                          EqualsProto(journey1)));
}

TEST_F(JourneysDatabaseTest, DuplicateHistoryEntriesHandledGracefully) {
  JourneySpecifics journey = CreateTestJourney(
      /*journey_id=*/"journey_1", /*title=*/"Trip with Duplicates",
      /*creation_time_micros=*/5000);
  // Add duplicate visit timestamp 1000 (already in CreateTestJourney).
  journey.add_history_entries()->set_visit_timestamp_windows_epoch_micros(1000);

  EXPECT_TRUE(journeys_db()->AddOrUpdateJourneys({journey}));

  // The retrieved journey should contain only distinct timestamps.
  std::optional<JourneySpecifics> retrieved =
      journeys_db()->GetJourney(/*journey_id=*/"journey_1");
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->history_entries_size(), 2);
}

}  // namespace

}  // namespace history::journeys
