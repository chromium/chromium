// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/history/core/browser/journeys/journeys_test_utils.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/init_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history::journeys {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Optional;

constexpr char kResolutionResultHistogram[] =
    "History.SyncedJourneys.Resolution.Result";

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

  base::ScopedTempDir temp_dir_;
  base::FilePath db_file_;
  TestHistoryDatabase db_;
};

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_SuccessWithMetadata) {
  URLID url_id1 = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  URLID url_id2 = AddTestURL(GURL("http://www.example.com/page2"), u"Page 2");
  ASSERT_NE(url_id1, 0);
  ASSERT_NE(url_id2, 0);

  base::Time visit_time_1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  base::Time visit_time_2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  ASSERT_NE(AddTestVisit(url_id1, visit_time_1), 0);
  ASSERT_NE(AddTestVisit(url_id2, visit_time_2), 0);

  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
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
  EXPECT_THAT(ResolveJourneyVisits(db_, journey), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_ResolvesCollections) {
  URLID url_id1 = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  URLID url_id2 = AddTestURL(GURL("http://www.example.com/page2"), u"Page 2");
  ASSERT_NE(url_id1, 0);
  ASSERT_NE(url_id2, 0);

  base::Time visit_time_1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  base::Time visit_time_2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  base::Time unknown_visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(3000));
  // A local visit that isn't one of the journey's history entries.
  base::Time other_visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(4000));
  ASSERT_NE(AddTestVisit(url_id1, visit_time_1), 0);
  ASSERT_NE(AddTestVisit(url_id2, visit_time_2), 0);
  ASSERT_NE(AddTestVisit(url_id1, other_visit_time), 0);

  JourneyRow journey = CreateJourneyRow(
      "test_journey", "Example Journey",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)),
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

  std::optional<Journey> resolved = ResolveJourneyVisits(db_, journey);
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
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  JourneyRow with_collection = CreateJourneyRow(
      "with_collection", "With collection",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(6000)),
      {visit_time});
  // The item that isn't one of the journey's visits is dropped, without
  // dropping the journey.
  with_collection.collections = {JourneyHistoryEntryCollection(
      "Collection", {JourneyHistoryEntry(visit_time),
                     JourneyHistoryEntry(base::Time::FromDeltaSinceWindowsEpoch(
                         base::Microseconds(3000)))})};
  JourneyRow without_collection = CreateJourneyRow(
      "without_collection", "Without collection",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)),
      {visit_time});
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

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_EmptyVisits) {
  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
  JourneyRow journey("empty_journey", "Empty", creation_time);

  Journey expected_journey("empty_journey", "Empty", creation_time);
  EXPECT_THAT(ResolveJourneyVisits(db_, journey), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       ResolveJourneyVisits_MissingVisitReturnsNullopt) {
  base::Time unknown_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(9999));
  JourneyRow missing_journey = CreateJourneyRow("missing_journey", /*title=*/"",
                                                /*creation_time=*/base::Time(),
                                                /*visit_times=*/{unknown_time});

  EXPECT_EQ(ResolveJourneyVisits(db_, missing_journey), std::nullopt);
}

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_MissingUrlReturnsNullopt) {
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  // Add visit referencing non-existent URLID 999.
  ASSERT_NE(AddTestVisit(999, visit_time), 0);

  JourneyRow journey = CreateJourneyRow("orphaned_visit_journey", /*title=*/"",
                                        /*creation_time=*/base::Time(),
                                        /*visit_times=*/{visit_time});

  EXPECT_EQ(ResolveJourneyVisits(db_, journey), std::nullopt);
}

TEST_F(JourneysBackendUtilTest,
       ResolveJourneyVisits_ResolvesLatestInRedirectChain) {
  URLID source_id =
      AddTestURL(GURL("http://www.example.com/source"), u"Source");
  URLID dest_id =
      AddTestURL(GURL("http://www.example.com/dest"), u"Destination");
  ASSERT_NE(source_id, 0);
  ASSERT_NE(dest_id, 0);

  base::Time shared_visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
  VisitID visit_1 =
      AddTestVisit(source_id, shared_visit_time,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START));
  VisitID visit_2 = AddTestVisit(
      dest_id, shared_visit_time,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_CLIENT_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END));
  ASSERT_GT(visit_2, visit_1);

  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(6000));
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
  EXPECT_THAT(ResolveJourneyVisits(db_, journey), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_EmptyDatabase) {
  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_), IsEmpty());

  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_OrdersByCreationTimeDesc) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  // Older resolved journey (creation_time = 5000).
  base::Time creation_time_older =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
  JourneyRow older_journey =
      CreateJourneyRow("journey_older", "Older Resolved", creation_time_older,
                       /*visit_times=*/{visit_time});

  // Newer resolved journey (creation_time = 8000).
  base::Time creation_time_newer =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(8000));
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
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
  JourneyRow complete_journey =
      CreateJourneyRow("journey_complete", "Complete", creation_time,
                       /*visit_times=*/{visit_time});

  // Incomplete journey (creation_time = 9000, has missing visit timestamp
  // 9999).
  JourneyRow incomplete_journey = CreateJourneyRow(
      "journey_incomplete", "Incomplete",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(9000)),
      /*visit_times=*/
      {visit_time,
       base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(9999))});

  // Journey whose visit references non-existent URLID 999.
  base::Time orphaned_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  ASSERT_NE(AddTestVisit(999, orphaned_time), 0);
  JourneyRow orphaned_journey = CreateJourneyRow(
      "journey_orphaned", "Orphaned",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(7000)),
      /*visit_times=*/{orphaned_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {complete_journey, incomplete_journey, orphaned_journey}));

  base::HistogramTester histogram_tester;
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_),
              ElementsAre(Field(&Journey::journey_id, "journey_complete")));

  histogram_tester.ExpectBucketCount(
      kResolutionResultHistogram, SyncedJourneyResolutionResult::kResolved, 1);
  histogram_tester.ExpectBucketCount(
      kResolutionResultHistogram, SyncedJourneyResolutionResult::kMissingVisit,
      1);
  histogram_tester.ExpectBucketCount(kResolutionResultHistogram,
                                     SyncedJourneyResolutionResult::kMissingUrl,
                                     1);
  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 3);
}

TEST_F(JourneysBackendUtilTest, GetJourneyWithResolvedVisits_Found) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
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
  ASSERT_TRUE(db_.AddOrUpdateJourneys({CreateJourneyRow(
      "journey_incomplete", "Incomplete", /*creation_time=*/base::Time(),
      /*visit_times=*/
      {base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(9999))})}));

  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetJourneyWithResolvedVisits(db_, "journey_incomplete"),
            std::nullopt);

  // Only GetAllJourneysWithResolvedVisits() records the resolution histogram.
  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_SetsIsForeign) {
  URLID local_url_id =
      AddTestURL(GURL("http://www.example.com/local"), u"Local");
  URLID foreign_url_id =
      AddTestURL(GURL("http://www.example.com/foreign"), u"Foreign");
  ASSERT_NE(local_url_id, 0);
  ASSERT_NE(foreign_url_id, 0);

  base::Time t1 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  base::Time t2 =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(2000));
  ASSERT_NE(AddTestVisit(local_url_id, t1), 0);
  ASSERT_NE(AddTestVisit(foreign_url_id, t2, ui::PAGE_TRANSITION_LINK,
                         /*originator_cache_guid=*/"remote_device_guid"),
            0);

  base::Time creation_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000));
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
  EXPECT_THAT(ResolveJourneyVisits(db_, row), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest, GetUnresolvableJourneysCountForFishfood) {
  base::HistogramTester histogram_tester;
  EXPECT_EQ(GetUnresolvableJourneysCountForFishfood(db_), 0u);

  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time resolved_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  base::Time missing_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(9999));
  ASSERT_NE(AddTestVisit(url_id, resolved_time), 0);

  JourneyRow complete_journey = CreateJourneyRow(
      "journey_complete", "Complete",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(7000)),
      /*visit_times=*/{resolved_time});
  JourneyRow unresolvable_journey_1 = CreateJourneyRow(
      "journey_unresolvable_1", "Unresolvable 1",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(6000)),
      /*visit_times=*/{resolved_time, missing_time});
  JourneyRow unresolvable_journey_2 = CreateJourneyRow(
      "journey_unresolvable_2", "Unresolvable 2",
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)),
      /*visit_times=*/{missing_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys(
      {complete_journey, unresolvable_journey_1, unresolvable_journey_2}));

  EXPECT_EQ(GetUnresolvableJourneysCountForFishfood(db_), 2u);
  EXPECT_EQ(GetUnresolvableJourneysCountForFishfoodForTesting(
                db_, /*max_journeys=*/2),
            1u);
  histogram_tester.ExpectTotalCount(kResolutionResultHistogram, 0);
}

}  // namespace history::journeys
