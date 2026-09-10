// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/journeys/journeys_backend_util.h"

#include <optional>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/history/core/test/test_history_database.h"
#include "sql/init_status.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace history::journeys {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Optional;
using ::testing::UnorderedElementsAre;

}  // namespace

class JourneysBackendUtilTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_file_ = temp_dir_.GetPath().AppendASCII("JourneysBackendUtilTest.db");
    ASSERT_EQ(sql::INIT_OK, db_.Init(db_file_));
  }

  URLID AddTestURL(const GURL& url, const std::u16string& title) {
    URLRow url_row(url);
    url_row.set_title(title);
    return db_.AddURL(url_row);
  }

  VisitID AddTestVisit(
      URLID url_id,
      base::Time visit_time,
      ui::PageTransition transition = ui::PAGE_TRANSITION_LINK) {
    VisitRow visit_row;
    visit_row.url_id = url_id;
    visit_row.visit_time = visit_time;
    visit_row.transition = transition;
    visit_row.source = SOURCE_BROWSED;
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
      {JourneyVisit(GURL("http://www.example.com/page1"), u"Page 1"),
       JourneyVisit(GURL("http://www.example.com/page2"), u"Page 2")},
      /*continuation_queries=*/{JourneyContinuationQuery("query", "prompt")});
  EXPECT_THAT(ResolveJourneyVisits(db_, journey), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest, ResolveJourneyVisits_EmptyVisits) {
  JourneyRow journey(
      "empty_journey", "Empty",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)));

  std::optional<Journey> resolved = ResolveJourneyVisits(db_, journey);
  ASSERT_TRUE(resolved.has_value());
  EXPECT_EQ(resolved->journey_id, "empty_journey");
  EXPECT_EQ(resolved->title, "Empty");
  EXPECT_THAT(resolved->visits, IsEmpty());
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
      {JourneyVisit(GURL("http://www.example.com/dest"), u"Destination")});
  EXPECT_THAT(ResolveJourneyVisits(db_, journey), Optional(expected_journey));
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_EmptyDatabase) {
  EXPECT_THAT(GetAllJourneysWithResolvedVisits(db_), IsEmpty());
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_OrdersByCreationTimeDesc) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  // Older resolved journey (creation_time = 5000).
  JourneyRow older_journey = CreateJourneyRow(
      "journey_older", "Older Resolved",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)),
      /*visit_times=*/{visit_time});

  // Newer resolved journey (creation_time = 8000).
  JourneyRow newer_journey = CreateJourneyRow(
      "journey_newer", "Newer Resolved",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(8000)),
      /*visit_times=*/{visit_time});

  ASSERT_TRUE(db_.AddOrUpdateJourneys({older_journey, newer_journey}));

  std::vector<Journey> all_journeys = GetAllJourneysWithResolvedVisits(db_);
  ASSERT_EQ(all_journeys.size(), 2u);
  EXPECT_EQ(all_journeys[0].journey_id, "journey_newer");
  EXPECT_EQ(all_journeys[0].title, "Newer Resolved");
  EXPECT_EQ(all_journeys[1].journey_id, "journey_older");
  EXPECT_EQ(all_journeys[1].title, "Older Resolved");
}

TEST_F(JourneysBackendUtilTest,
       GetAllJourneysWithResolvedVisits_FiltersOutUnresolvedJourneys) {
  URLID url_id = AddTestURL(GURL("http://www.example.com/page1"), u"Page 1");
  ASSERT_NE(url_id, 0);
  base::Time visit_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(1000));
  ASSERT_NE(AddTestVisit(url_id, visit_time), 0);

  JourneyRow complete_journey = CreateJourneyRow(
      "journey_complete", "Complete",
      /*creation_time=*/
      base::Time::FromDeltaSinceWindowsEpoch(base::Microseconds(5000)),
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

  ASSERT_TRUE(db_.AddOrUpdateJourneys({complete_journey, incomplete_journey}));

  std::vector<Journey> all_journeys = GetAllJourneysWithResolvedVisits(db_);
  ASSERT_EQ(all_journeys.size(), 1u);
  EXPECT_EQ(all_journeys[0].journey_id, "journey_complete");
  EXPECT_EQ(all_journeys[0].title, "Complete");
}

}  // namespace history::journeys
