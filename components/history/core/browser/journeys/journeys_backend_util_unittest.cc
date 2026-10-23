// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/history/core/browser/journeys/journeys_database.h"
#include "components/history/core/browser/journeys/journeys_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history::journeys {
namespace {

using ::base::Bucket;
using ::base::BucketsAre;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;

constexpr char kResolutionResultHistogram[] =
    "History.SyncedJourneys.Resolution.Result";

using LookupStatus = JourneyHistoryEntryLookup::Status;

// Matches a lookup with status `kMissingVisit` or `kMissingUrl`. The URL row
// fields aren't set for these, so only the entry and status are checked.
testing::Matcher<const JourneyHistoryEntryLookup&> MatchesMissingLookup(
    const std::string& journey_id,
    base::Time visit_time,
    LookupStatus status) {
  CHECK_NE(status, LookupStatus::kFound);
  return AllOf(
      Field("journey_id", &JourneyHistoryEntryLookup::journey_id, journey_id),
      Field("visit_time", &JourneyHistoryEntryLookup::visit_time, visit_time),
      Field("status", &JourneyHistoryEntryLookup::status, status));
}

// Matches a lookup with status `kFound`, checking every field.
testing::Matcher<const JourneyHistoryEntryLookup&> MatchesFoundLookup(
    const std::string& journey_id,
    base::Time visit_time,
    const GURL& url,
    const std::u16string& title,
    bool is_foreign) {
  return AllOf(
      Field("journey_id", &JourneyHistoryEntryLookup::journey_id, journey_id),
      Field("visit_time", &JourneyHistoryEntryLookup::visit_time, visit_time),
      Field("status", &JourneyHistoryEntryLookup::status, LookupStatus::kFound),
      Field("url", &JourneyHistoryEntryLookup::url, url),
      Field("title", &JourneyHistoryEntryLookup::title, title),
      Field("is_foreign", &JourneyHistoryEntryLookup::is_foreign, is_foreign));
}

}  // namespace

class JourneysBackendUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_file_ = temp_dir_.GetPath().AppendASCII("JourneysBackendUtilTest.db");
    ASSERT_EQ(db_.Init(db_file_), sql::INIT_OK);
  }

  URLID AddTestURL(const GURL& url, const std::u16string& title) {
    URLRow url_row(url);
    url_row.set_title(title);
    return db_.AddURL(url_row);
  }

  VisitID AddTestVisit(URLID url_id,
                       base::Time visit_time,
                       ui::PageTransition transition = ui::PAGE_TRANSITION_LINK,
                       const std::string& originator_cache_guid = "") {
    VisitRow visit_row;
    visit_row.url_id = url_id;
    visit_row.visit_time = visit_time;
    visit_row.transition = transition;
    visit_row.source =
        originator_cache_guid.empty() ? SOURCE_BROWSED : SOURCE_SYNCED;
    visit_row.originator_cache_guid = originator_cache_guid;
    return db_.AddVisit(&visit_row) ? visit_row.visit_id : 0;
  }

  JourneyRow CreateJourneyRow(const std::string& journey_id,
                              const std::string& title,
                              base::Time creation_time,
                              std::vector<base::Time> visit_times) {
    std::vector<JourneyHistoryEntry> history_entries;
    history_entries.reserve(visit_times.size());
    for (base::Time visit_time : visit_times) {
      history_entries.emplace_back(visit_time);
    }
    return JourneyRow(journey_id, title, creation_time, /*emoji=*/std::nullopt,
                      /*overview=*/std::nullopt,
                      /*short_overview=*/std::nullopt,
                      std::move(history_entries));
  }

  // Stores `journey` in `db_` and checks that it can be read back. Call with
  // ASSERT_NO_FATAL_FAILURE().
  void StoreJourney(const JourneyRow& journey) {
    ASSERT_TRUE(db_.AddOrUpdateJourneys({journey}));
    ASSERT_TRUE(db_.GetJourney(journey.journey_id).has_value());
  }

  // Stores `journey` in `db_` and resolves it via
  // `GetJourneyWithResolvedVisits()`. Returns std::nullopt if storing fails.
  std::optional<Journey> StoreJourneyAndResolveVisits(
      const JourneyRow& journey) {
    StoreJourney(journey);
    if (HasFatalFailure()) {
      return std::nullopt;
    }
    return GetJourneyWithResolvedVisits(db_, journey.journey_id);
  }

  // Overwrites the URL of `url_id` in the `urls` table with an invalid one.
  void SetInvalidUrl(URLID url_id) {
    sql::Statement update_url(db_.GetDBForTesting().GetUniqueStatement(
        "UPDATE urls SET url = '' WHERE id = ?"));
    update_url.BindInt64(0, url_id);
    ASSERT_TRUE(update_url.Run());
  }

  base::ScopedTempDir temp_dir_;
  base::FilePath db_file_;
  TestHistoryDatabase db_;
};

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_SuccessWithMetadata) {
  URLID url_id1 = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  URLID url_id2 = AddTestURL(GURL("http://www.example.com/page2"), u"Page 2");
  ASSERT_NE(url_id1, 0);
  ASSERT_NE(url_id2, 0);

  base::Time visit_time_1 = TimeFromMicros(1000);
  base::Time visit_time_2 = TimeFromMicros(2000);
  ASSERT_NE(AddTestVisit(url_id1, visit_time_1), 0);
  ASSERT_NE(AddTestVisit(url_id2, visit_time_2), 0);

  base::Time creation_time = TimeFromMicros(5000);
  JourneyRow journey(
      "test_journey", "Example Journey", creation_time, /*emoji=*/"🔍",
      /*overview=*/"Overview text", /*short_overview=*/"Short overview",
      /*history_entries=*/
      {JourneyHistoryEntry(visit_time_1), JourneyHistoryEntry(visit_time_2)},
      /*continuation_queries=*/{JourneyContinuationQuery("query", "prompt")});

  Journey expected_journey(
      "test_journey", "Example Journey", creation_time, /*emoji=*/"🔍",
      /*overview=*/"Overview text", /*short_overview=*/"Short overview",
      /*visits=*/
      {JourneyVisit(GURL("http://www.example.com/page1"), u"Page 1",
                    visit_time_1, /*is_foreign=*/false),
       JourneyVisit(GURL("http://www.example.com/page2"), u"Page 2",
                    visit_time_2, /*is_foreign=*/false)},
      /*continuation_queries=*/{JourneyContinuationQuery("query", "prompt")});
  EXPECT_THAT(StoreJourneyAndResolveVisits(journey),
              Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_ResolvesCollections) {
  URLID url_id1 = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  URLID url_id2 = AddTestURL(GURL("http://www.example.com/page2"), u"Page 2");
  ASSERT_NE(url_id1, 0);
  ASSERT_NE(url_id2, 0);

  base::Time visit_time_1 = TimeFromMicros(1000);
  base::Time visit_time_2 = TimeFromMicros(2000);
  base::Time unknown_visit_time = TimeFromMicros(3000);
  // A local visit that isn't one of the journey's history entries.
  base::Time other_visit_time = TimeFromMicros(4000);
  ASSERT_NE(AddTestVisit(url_id1, visit_time_1), 0);
  ASSERT_NE(AddTestVisit(url_id2, visit_time_2), 0);
  ASSERT_NE(AddTestVisit(url_id1, other_visit_time), 0);

  JourneyRow journey =
      CreateJourneyRow("test_journey", "Example Journey", TimeFromMicros(5000),
                       {visit_time_1, visit_time_2});
  journey.collections = {
      // Items are in display order, which differs from visit order. Items
      // that aren't one of the journey's visits are dropped, even if the
      // visit exists in the history database.
      JourneyHistoryEntryCollection("Pages you've visited",
                                    {JourneyHistoryEntry(visit_time_2),
                                     JourneyHistoryEntry(unknown_visit_time),
                                     JourneyHistoryEntry(other_visit_time),
                                     JourneyHistoryEntry(visit_time_1)}),
      // A collection without any resolvable items is dropped entirely.
      JourneyHistoryEntryCollection("Unresolvable",
                                    {JourneyHistoryEntry(unknown_visit_time)}),
      // Collections after a dropped one keep their display order.
      JourneyHistoryEntryCollection("Last page",
                                    {JourneyHistoryEntry(visit_time_2)})};

  std::optional<Journey> resolved = StoreJourneyAndResolveVisits(journey);
  ASSERT_TRUE(resolved.has_value());
  JourneyVisit visit_1(GURL("http://www.example.com/page1"), u"Page 1",
                       visit_time_1, /*is_foreign=*/false);
  JourneyVisit visit_2(GURL("http://www.example.com/page2"), u"Page 2",
                       visit_time_2, /*is_foreign=*/false);
  EXPECT_THAT(resolved->collections,
              ElementsAre(JourneyVisitCollection("Pages you've visited",
                                                 {visit_2, visit_1}),
                          JourneyVisitCollection("Last page", {visit_2})));
}

// Collections are read from the database and resolved for every journey, and
// a journey without collections is unaffected by another journey's.
TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_ResolvesCollections) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time = TimeFromMicros(1000);
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  JourneyRow with_collection = CreateJourneyRow(
      "with_collection", "With collection", TimeFromMicros(6000), {visit_time});
  // The item that isn't one of the journey's visits is dropped, without
  // dropping the journey.
  with_collection.collections = {JourneyHistoryEntryCollection(
      "Collection", {JourneyHistoryEntry(visit_time),
                     JourneyHistoryEntry(TimeFromMicros(3000))})};
  JourneyRow without_collection =
      CreateJourneyRow("without_collection", "Without collection",
                       TimeFromMicros(5000), {visit_time});
  ASSERT_TRUE(db_.AddOrUpdateJourneys({with_collection, without_collection}));

  base::HistogramTester histogram_tester;
  std::vector<Journey> resolved = GetAllJourneysWithResolvedVisits(db_);
  histogram_tester.ExpectUniqueSample(
      kResolutionResultHistogram, SyncedJourneyResolutionResult::kResolved, 2);
  ASSERT_EQ(resolved.size(), 2u);
  EXPECT_EQ(resolved[0].journey_id, "with_collection");
  EXPECT_THAT(
      resolved[0].collections,
      ElementsAre(JourneyVisitCollection(
          "Collection", {JourneyVisit(GURL("http://www.example.com/page1"),
                                      u"Page 1", visit_time,
                                      /*is_foreign=*/false)})));
  EXPECT_EQ(resolved[1].journey_id, "without_collection");
  EXPECT_THAT(resolved[1].collections, IsEmpty());
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_NoHistoryEntriesReturnsNullopt) {
  JourneyRow journey("empty_journey", "Empty", TimeFromMicros(5000));
  EXPECT_EQ(StoreJourneyAndResolveVisits(journey), std::nullopt);
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_MissingUrlReturnsNullopt) {
  base::Time visit_time = TimeFromMicros(1000);
  // Add visit referencing non-existent URLID 999.
  ASSERT_NE(AddTestVisit(999, visit_time), 0);

  ASSERT_NO_FATAL_FAILURE(StoreJourney(CreateJourneyRow(
      "orphaned_visit_journey", /*title=*/"", /*creation_time=*/base::Time(),
      /*visit_times=*/{visit_time})));

  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "orphaned_visit_journey"),
            std::nullopt);
}

// Like URLDatabase::FillURLRow(), a URL row with an invalid URL counts as
// missing.
TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_InvalidUrlReturnsNullopt) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time = TimeFromMicros(1000);
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);
  ASSERT_NO_FATAL_FAILURE(SetInvalidUrl(url_id));

  ASSERT_NO_FATAL_FAILURE(StoreJourney(CreateJourneyRow(
      "invalid_url_journey", /*title=*/"", /*creation_time=*/base::Time(),
      /*visit_times=*/{visit_time})));

  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "invalid_url_journey"),
            std::nullopt);
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_ResolvesLatestInRedirectChain) {
  URLID source_id =
      AddTestURL(GURL("http://www.example.com/source"), u"Source");
  URLID dest_id =
      AddTestURL(GURL("http://www.example.com/dest"), u"Destination");
  ASSERT_NE(source_id, 0);
  ASSERT_NE(dest_id, 0);

  base::Time shared_visit_time = TimeFromMicros(5000);
  VisitID visit_1 =
      AddTestVisit(source_id, shared_visit_time,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START));
  VisitID visit_2 = AddTestVisit(
      dest_id, shared_visit_time,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END));
  ASSERT_GT(visit_2, visit_1);

  base::Time creation_time = TimeFromMicros(6000);
  JourneyRow journey =
      CreateJourneyRow("journey_redirect", "Redirect Journey", creation_time,
                       /*visit_times=*/{shared_visit_time});

  Journey expected_journey(
      "journey_redirect", "Redirect Journey", creation_time,
      /*emoji=*/std::nullopt, /*overview=*/std::nullopt,
      /*short_overview=*/std::nullopt,
      /*visits=*/
      {JourneyVisit(GURL("http://www.example.com/dest"), u"Destination",
                    shared_visit_time, /*is_foreign=*/false)});
  EXPECT_THAT(StoreJourneyAndResolveVisits(journey),
              Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_EmptyDatabase) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_), IsEmpty());

  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

// Each journey only resolves its own history entries, including when journeys
// share a visit timestamp, and one journey failing to resolve doesn't affect
// the others.
TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_ResolvesEachJourneyIndependently) {
  URLID page_id = AddTestURL(GURL("http://www.example.com/page"), u"Page");
  URLID source_id =
      AddTestURL(GURL("http://www.example.com/source"), u"Source");
  URLID dest_id =
      AddTestURL(GURL("http://www.example.com/dest"), u"Destination");
  URLID invalid_id =
      AddTestURL(GURL("http://www.example.com/invalid"), u"Invalid");
  ASSERT_NE(page_id, 0);
  ASSERT_NE(source_id, 0);
  ASSERT_NE(dest_id, 0);
  ASSERT_NE(invalid_id, 0);
  ASSERT_NO_FATAL_FAILURE(SetInvalidUrl(invalid_id));

  base::Time page_time = TimeFromMicros(1000);
  base::Time redirect_time = TimeFromMicros(2000);
  base::Time invalid_url_time = TimeFromMicros(3000);
  base::Time unknown_time = TimeFromMicros(4000);
  ASSERT_NE(AddTestVisit(page_id, page_time), 0);
  // A redirect chain shares one timestamp; the chain end has the larger ID.
  // Two journeys reference it, to check that a shared timestamp resolves the
  // same for each journey.
  VisitID source_visit_id =
      AddTestVisit(source_id, redirect_time,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START));
  VisitID dest_visit_id = AddTestVisit(
      dest_id, redirect_time,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END));
  ASSERT_NE(source_visit_id, 0);
  ASSERT_GT(dest_visit_id, source_visit_id);
  ASSERT_NE(AddTestVisit(invalid_id, invalid_url_time), 0);

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {CreateJourneyRow("resolvable", "Resolvable", TimeFromMicros(9000),
                        {page_time, redirect_time}),
       CreateJourneyRow("shares_timestamp", "Shares timestamp",
                        TimeFromMicros(8000), {redirect_time}),
       CreateJourneyRow("missing_visit", "Missing visit", TimeFromMicros(7000),
                        {page_time, unknown_time}),
       CreateJourneyRow("invalid_url", "Invalid URL", TimeFromMicros(6000),
                        {page_time, invalid_url_time}),
       CreateJourneyRow("no_entries", "No entries", TimeFromMicros(5000),
                        {})}));

  const JourneyVisit page_visit(GURL("http://www.example.com/page"), u"Page",
                                page_time, /*is_foreign=*/false);
  const JourneyVisit dest_visit(GURL("http://www.example.com/dest"),
                                u"Destination", redirect_time,
                                /*is_foreign=*/false);
  base::HistogramTester histogram_tester;
  EXPECT_THAT(
      GetAllJourneysWithResolvedVisits(db_),
      ElementsAre(
          AllOf(Field(&Journey::journey_id, "resolvable"),
                Field(&Journey::visits, ElementsAre(page_visit, dest_visit))),
          AllOf(Field(&Journey::journey_id, "shares_timestamp"),
                Field(&Journey::visits, ElementsAre(dest_visit)))));
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kResolutionResultHistogram),
      BucketsAre(Bucket(SyncedJourneyResolutionResult::kResolved, 2),
                 Bucket(SyncedJourneyResolutionResult::kMissingVisit, 2),
                 Bucket(SyncedJourneyResolutionResult::kMissingUrl, 1)));
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_OrdersByCreationTimeDesc) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time = TimeFromMicros(1000);
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  // Older resolved journey (creation_time = 5000).
  base::Time creation_time_older = TimeFromMicros(5000);
  JourneyRow older_journey =
      CreateJourneyRow("journey_older", "Older Resolved", creation_time_older,
                       /*visit_times=*/{visit_time});

  // Newer resolved journey (creation_time = 8000).
  base::Time creation_time_newer = TimeFromMicros(8000);
  JourneyRow newer_journey =
      CreateJourneyRow("journey_newer", "Newer Resolved", creation_time_newer,
                       /*visit_times=*/{visit_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys({older_journey, newer_journey}));

  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_),
              ElementsAre(Field(&Journey::journey_id, "journey_newer"),
                          Field(&Journey::journey_id, "journey_older")));

  histogram_tester.ExpectUniqueSample(
      kResolutionResultHistogram, SyncedJourneyResolutionResult::kResolved, 2);
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_FiltersOutUnresolvedJourneys) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time = TimeFromMicros(1000);
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  base::Time creation_time = TimeFromMicros(5000);
  JourneyRow complete_journey =
      CreateJourneyRow("journey_complete", "Complete", creation_time,
                       /*visit_times=*/{visit_time});

  // Incomplete journey (creation_time = 9000, has missing visit timestamp
  // 9999).
  JourneyRow incomplete_journey =
      CreateJourneyRow("journey_incomplete", "Incomplete",
                       /*creation_time=*/
                       TimeFromMicros(9000),
                       /*visit_times=*/
                       {visit_time, TimeFromMicros(9999)});

  // Journey whose visit references non-existent URLID 999.
  base::Time orphaned_time = TimeFromMicros(2000);
  ASSERT_NE(AddTestVisit(999, orphaned_time), 0);
  JourneyRow orphaned_journey =
      CreateJourneyRow("journey_orphaned", "Orphaned",
                       /*creation_time=*/
                       TimeFromMicros(7000),
                       /*visit_times=*/{orphaned_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {complete_journey, incomplete_journey, orphaned_journey}));

  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_),
              ElementsAre(Field(&Journey::journey_id, "journey_complete")));

  EXPECT_THAT(
      histogram_tester.GetAllSamples(kResolutionResultHistogram),
      BucketsAre(Bucket(SyncedJourneyResolutionResult::kResolved, 1),
                 Bucket(SyncedJourneyResolutionResult::kMissingVisit, 1),
                 Bucket(SyncedJourneyResolutionResult::kMissingUrl, 1)));
}

TEST_F(JourneysBackendUtilTest, GetJourneyWithResolvedVisits_Found) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time = TimeFromMicros(1000);
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  base::Time creation_time = TimeFromMicros(5000);
  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {CreateJourneyRow("journey_a", "Journey A", creation_time,
                        /*visit_times=*/{visit_time}),
       CreateJourneyRow("journey_b", "Journey B", creation_time,
                        /*visit_times=*/{visit_time})}));

  Journey expected_journey(
      "journey_b", "Journey B", creation_time,
      /*emoji=*/std::nullopt, /*overview=*/std::nullopt,
      /*short_overview=*/std::nullopt,
      /*visits=*/
      {JourneyVisit(GURL("http://www.example.com/page1"), u"Page 1", visit_time,
                    /*is_foreign=*/false)});
  EXPECT_THAT(GetJourneyWithResolvedVisits(db_, "journey_b"),
              Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_UnknownIdReturnsNullopt) {
  ASSERT_TRUE(db_.AddOrUpdateJourneys({CreateJourneyRow(
      "journey_a", "Journey A", /*creation_time=*/base::Time(),
      /*visit_times=*/{})}));

  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "unknown"), std::nullopt);
}

TEST_F(JourneysBackendUtilTest,
       GetJourneyWithResolvedVisits_UnresolvedVisitReturnsNullopt) {
  ASSERT_NO_FATAL_FAILURE(StoreJourney(CreateJourneyRow(
      "journey_incomplete", "Incomplete", /*creation_time=*/base::Time(),
      /*visit_times=*/{TimeFromMicros(9999)})));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "journey_incomplete"),
            std::nullopt);

  // Only GetAllJourneysWithResolvedVisits() records the resolution histogram.
  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

TEST_F(JourneysBackendUtilTest, GetJourneyWithResolvedVisits_SetsIsForeign) {
  URLID local_url_id =
      AddTestURL(GURL("http://www.example.com/local"), u"Local");
  URLID foreign_url_id =
      AddTestURL(GURL("http://www.example.com/foreign"), u"Foreign");
  ASSERT_NE(local_url_id, 0);
  ASSERT_NE(foreign_url_id, 0);

  base::Time t1 = TimeFromMicros(1000);
  base::Time t2 = TimeFromMicros(2000);
  ASSERT_NE(AddTestVisit(local_url_id, t1), 0);
  ASSERT_NE(AddTestVisit(foreign_url_id, t2, ui::PAGE_TRANSITION_LINK,
                         /*originator_cache_guid=*/"remote_device_guid"),
            0);

  base::Time creation_time = TimeFromMicros(5000);
  JourneyRow row = CreateJourneyRow("journey_foreign", "Mixed Devices",
                                    creation_time, {t1, t2});

  Journey expected_journey(
      "journey_foreign", "Mixed Devices", creation_time,
      /*emoji=*/std::nullopt, /*overview=*/std::nullopt,
      /*short_overview=*/std::nullopt,
      /*visits=*/
      {JourneyVisit(GURL("http://www.example.com/local"), u"Local", t1,
                    /*is_foreign=*/false),
       JourneyVisit(GURL("http://www.example.com/foreign"), u"Foreign", t2,
                    /*is_foreign=*/true)});
  EXPECT_THAT(StoreJourneyAndResolveVisits(row), Optional(expected_journey));
}

// The JourneysDatabase lookup tests live here rather than in
// journeys_database_unittest.cc because the lookups join with the `visits` and
// `urls` tables, which only a full HistoryDatabase has.

// Covers the lookup statuses and the journey ID ordering that resolution relies
// on.
TEST_F(JourneysBackendUtilTest,
       LookUpAllJourneyHistoryEntries_ReportsStatusesInJourneyIdOrder) {
  URLID page_id = AddTestURL(GURL("http://www.example.com/page"), u"Page");
  URLID source_id =
      AddTestURL(GURL("http://www.example.com/source"), u"Source");
  URLID dest_id =
      AddTestURL(GURL("http://www.example.com/dest"), u"Destination");
  URLID invalid_id =
      AddTestURL(GURL("http://www.example.com/invalid"), u"Invalid");
  ASSERT_NE(page_id, 0);
  ASSERT_NE(source_id, 0);
  ASSERT_NE(dest_id, 0);
  ASSERT_NE(invalid_id, 0);
  ASSERT_NO_FATAL_FAILURE(SetInvalidUrl(invalid_id));

  const base::Time foreign_time = TimeFromMicros(1000);
  const base::Time redirect_time = TimeFromMicros(2000);
  const base::Time invalid_url_time = TimeFromMicros(3000);
  const base::Time unknown_time = TimeFromMicros(4000);
  const base::Time orphaned_time = TimeFromMicros(5000);
  ASSERT_NE(AddTestVisit(page_id, foreign_time, ui::PAGE_TRANSITION_LINK,
                         /*originator_cache_guid=*/"remote_device_guid"),
            0);
  // A redirect chain shares one timestamp; the chain end has the larger ID,
  // and the visit with the largest ID wins.
  VisitID source_visit_id =
      AddTestVisit(source_id, redirect_time,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START));
  VisitID dest_visit_id = AddTestVisit(
      dest_id, redirect_time,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END));
  ASSERT_NE(source_visit_id, 0);
  ASSERT_GT(dest_visit_id, source_visit_id);
  ASSERT_NE(AddTestVisit(invalid_id, invalid_url_time), 0);
  // Visit referencing non-existent URLID 999.
  ASSERT_NE(AddTestVisit(999, orphaned_time), 0);

  // Added in reverse journey ID order, and journey_b is newer, so neither
  // insertion nor creation time order matches the expected journey ID order.
  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {CreateJourneyRow("journey_b", "B", TimeFromMicros(9000),
                        {redirect_time, foreign_time}),
       CreateJourneyRow("journey_a", "A", TimeFromMicros(8000),
                        {orphaned_time, unknown_time, invalid_url_time})}));

  EXPECT_THAT(
      db_.LookUpAllJourneyHistoryEntries(),
      Optional(ElementsAre(
          MatchesMissingLookup("journey_a", invalid_url_time,
                               LookupStatus::kMissingUrl),
          MatchesMissingLookup("journey_a", unknown_time,
                               LookupStatus::kMissingVisit),
          MatchesMissingLookup("journey_a", orphaned_time,
                               LookupStatus::kMissingUrl),
          MatchesFoundLookup("journey_b", foreign_time,
                             GURL("http://www.example.com/page"), u"Page",
                             /*is_foreign=*/true),
          MatchesFoundLookup("journey_b", redirect_time,
                             GURL("http://www.example.com/dest"),
                             u"Destination", /*is_foreign=*/false))));
}

TEST_F(JourneysBackendUtilTest,
       LookUpJourneyHistoryEntries_ReturnsOnlyThatJourneyInVisitOrder) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page"), u"Page");
  ASSERT_NE(url_id, 0);
  const base::Time visit_time_1 = TimeFromMicros(1000);
  const base::Time visit_time_2 = TimeFromMicros(2000);
  ASSERT_NE(AddTestVisit(url_id, visit_time_1), 0);

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {CreateJourneyRow("journey_a", "A", TimeFromMicros(9000),
                        {visit_time_2, visit_time_1}),
       CreateJourneyRow("journey_b", "B", TimeFromMicros(8000),
                        {visit_time_1})}));

  // Only the requested journey's entries, ordered by visit time.
  EXPECT_THAT(
      db_.LookUpJourneyHistoryEntries("journey_a"),
      Optional(ElementsAre(
          MatchesFoundLookup("journey_a", visit_time_1,
                             GURL("http://www.example.com/page"), u"Page",
                             /*is_foreign=*/false),
          MatchesMissingLookup("journey_a", visit_time_2,
                               LookupStatus::kMissingVisit))));
  EXPECT_THAT(db_.LookUpJourneyHistoryEntries("unknown"), Optional(IsEmpty()));
}

TEST_F(JourneysBackendUtilTest,
       LookUpAllJourneyHistoryEntries_NoJourneysReturnsEmpty) {
  EXPECT_THAT(db_.LookUpAllJourneyHistoryEntries(), Optional(IsEmpty()));
}

// A lookup statement that fails while stepping returns std::nullopt, and
// resolution drops the journeys, instead of using the rows read so far. This is
// the only test of that partial-read path, so keep each part of the setup:
// - The first row reads fine and the second fails, so rows have already been
//   read when the statement fails.
// - The failure comes from `urls.title`, replaced with a generated column that
//   evaluates abs(INT64_MIN), an integer overflow error, for one URL only. It
//   is evaluated per row, so it fails in Step() rather than when preparing the
//   statement.
// - The column is VIRTUAL because ALTER TABLE can't add STORED generated
//   columns.
// If an ALTER TABLE below fails, check that Chromium's SQLite build still
// supports generated columns (no SQLITE_OMIT_GENERATED_COLUMNS in
// //third_party/sqlite/sqlite_common_configuration_flags.gni).
TEST_F(JourneysBackendUtilTest,
       LookUpJourneyHistoryEntries_ReadErrorReturnsNoJourneys) {
  URLID good_id = AddTestURL(GURL("http://www.example.com/good"), u"Good");
  URLID failing_id =
      AddTestURL(GURL("http://www.example.com/failing"), u"Failing");
  ASSERT_NE(good_id, 0);
  ASSERT_NE(failing_id, 0);
  const base::Time good_time = TimeFromMicros(1000);
  const base::Time failing_time = TimeFromMicros(2000);
  ASSERT_NE(AddTestVisit(good_id, good_time), 0);
  ASSERT_NE(AddTestVisit(failing_id, failing_time), 0);
  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {CreateJourneyRow("journey_a", "A", TimeFromMicros(9000),
                        {good_time, failing_time}),
       CreateJourneyRow("journey_good", "Good", TimeFromMicros(8000),
                        {good_time})}));

  sql::Database& sql_db = db_.GetDBForTesting();
  ASSERT_TRUE(
      sql_db.Execute("ALTER TABLE urls RENAME COLUMN title TO stored_title"));
  ASSERT_TRUE(sql_db.Execute(
      "ALTER TABLE urls ADD COLUMN title GENERATED ALWAYS AS "
      "(CASE WHEN stored_title = 'Failing' "
      "THEN abs(-9223372036854775807 - 1) ELSE stored_title END) VIRTUAL"));
  // The journey itself is still readable, and so are the lookups that don't
  // hit the failing URL, so the statement prepares fine.
  ASSERT_TRUE(db_.GetJourney("journey_a").has_value());
  ASSERT_THAT(
      db_.LookUpJourneyHistoryEntries("journey_good"),
      Optional(ElementsAre(MatchesFoundLookup(
          "journey_good", good_time, GURL("http://www.example.com/good"),
          u"Good", /*is_foreign=*/false))));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(sql::SqliteResultCode::kError);
  EXPECT_EQ(db_.LookUpJourneyHistoryEntries("journey_a"), std::nullopt);
  EXPECT_EQ(db_.LookUpAllJourneyHistoryEntries(), std::nullopt);
  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "journey_a"), std::nullopt);
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_), IsEmpty());
  EXPECT_TRUE(expecter.SawExpectedErrors());
}

TEST_F(JourneysBackendUtilTest, GetUnresolvableJourneysCountForFishfood) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetUnresolvableJourneysCountForFishfood(db_), 0u);

  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time resolved_time = TimeFromMicros(1000);
  base::Time missing_time = TimeFromMicros(9999);
  ASSERT_NE(AddTestVisit(url_id, resolved_time), 0);

  JourneyRow complete_journey =
      CreateJourneyRow("journey_complete", "Complete", TimeFromMicros(7000),
                       /*visit_times=*/{resolved_time});
  JourneyRow unresolvable_journey_1 = CreateJourneyRow(
      "journey_unresolvable_1", "Unresolvable 1", TimeFromMicros(6000),
      /*visit_times=*/{resolved_time, missing_time});
  JourneyRow unresolvable_journey_2 = CreateJourneyRow(
      "journey_unresolvable_2", "Unresolvable 2", TimeFromMicros(5000),
      /*visit_times=*/{missing_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {complete_journey, unresolvable_journey_1, unresolvable_journey_2}));

  EXPECT_EQ(GetUnresolvableJourneysCountForFishfood(db_), 2u);
  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

}  // namespace history::journeys
