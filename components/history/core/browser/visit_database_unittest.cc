// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_database.h"

#include <stddef.h>

#include <set>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/visit_annotations_database.h"
#include "components/history/core/browser/visited_link_database.h"
#include "history_types.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "url/origin.h"

using base::Time;
using ::testing::AllOf;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Property;

namespace history {

namespace {

::testing::Matcher<const VisitRow&> MatchesVisitInfo(const VisitRow& expected) {
  return AllOf(
      Field("visit_id", &VisitRow::visit_id, Eq(expected.visit_id)),
      Field("url_id", &VisitRow::url_id, Eq(expected.url_id)),
      Field("visit_time", &VisitRow::visit_time, Eq(expected.visit_time)),
      Field("referring_visit", &VisitRow::referring_visit,
            Eq(expected.referring_visit)),
      Field("transition", &VisitRow::transition,
            ::testing::Truly([&](const auto& actual_transition) {
              return ui::PageTransitionTypeIncludingQualifiersIs(
                  actual_transition, expected.transition);
            })),
      Field("originator_cache_guid", &VisitRow::originator_cache_guid,
            Eq(expected.originator_cache_guid)),
      Field("originator_visit_id", &VisitRow::originator_visit_id,
            Eq(expected.originator_visit_id)),
      Field("is_known_to_sync", &VisitRow::is_known_to_sync,
            Eq(expected.is_known_to_sync)),
      Field("consider_for_ntp_most_visited",
            &VisitRow::consider_for_ntp_most_visited,
            Eq(expected.consider_for_ntp_most_visited)),
      Field("source", &VisitRow::source, Eq(expected.source)),
      Field("app_id", &VisitRow::app_id, Eq(expected.app_id)));
}

}  // namespace

class VisitDatabaseTest : public PlatformTest,
                          public URLDatabase,
                          public VisitDatabase,
                          public VisitedLinkDatabase,
                          public VisitAnnotationsDatabase {
 protected:
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(db_.OpenInMemory());

    // Initialize the tables for this test.
    ASSERT_TRUE(CreateURLTable(false));
    ASSERT_TRUE(CreateMainURLIndex());
    ASSERT_TRUE(CreateVisitedLinkTable());
    ASSERT_TRUE(InitVisitTable());
    ASSERT_TRUE(InitVisitAnnotationsTables());
  }

  void TearDown() override {
    db_.Close();
    PlatformTest::TearDown();
  }

  // Provided for URL/VisitDatabase.
  sql::Database& GetDB() override { return db_; }

  sql::Database db_{sql::test::kTestTag};
};

TEST_F(VisitDatabaseTest, Add) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0, ui::PAGE_TRANSITION_LINK, 0, false,
                       0);
  EXPECT_TRUE(AddVisit(&visit_info1));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
                       visit_info1.visit_time + base::Seconds(1), 1,
                       ui::PAGE_TRANSITION_TYPED, 0, true, 0);
  // Verify we can fetch originator data too.
  visit_info2.originator_cache_guid = "foobar_client";
  visit_info2.originator_visit_id = 42;
  EXPECT_TRUE(AddVisit(&visit_info2));

  // Add third visit for a different page.
  VisitRow visit_info3(2, visit_info1.visit_time + base::Seconds(2), 0,
                       ui::PAGE_TRANSITION_LINK, 0, false, 0);
  // Verify we can add a corresponding VisitedLinkID.
  visit_info3.visited_link_id = 10000;
  EXPECT_TRUE(AddVisit(&visit_info3));

  // Query the first two.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(2U, matches.size());

  // Make sure we got both (order in result set is visit time).
  EXPECT_THAT(matches[0], MatchesVisitInfo(visit_info1));
  EXPECT_THAT(matches[1], MatchesVisitInfo(visit_info2));
}

TEST_F(VisitDatabaseTest, Delete) {
  // Add three visits that form a chain of navigation, and then delete the
  // middle one. We should be left with the outer two visits, and the chain
  // should link them.
  static const int kTime1 = 1000;
  VisitRow visit_info1(1, Time::FromInternalValue(kTime1), 0,
                       ui::PAGE_TRANSITION_LINK, 0, false, 0);
  EXPECT_TRUE(AddVisit(&visit_info1));

  static const int kTime2 = kTime1 + 1;
  VisitRow visit_info2(1, Time::FromInternalValue(kTime2), visit_info1.visit_id,
                       ui::PAGE_TRANSITION_LINK, 0, false, 0);
  EXPECT_TRUE(AddVisit(&visit_info2));

  static const int kTime3 = kTime2 + 1;
  VisitRow visit_info3(1, Time::FromInternalValue(kTime3), visit_info2.visit_id,
                       ui::PAGE_TRANSITION_LINK, 0, false, 0);
  EXPECT_TRUE(AddVisit(&visit_info3));

  // First make sure all the visits are there.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(3U, matches.size());
  EXPECT_THAT(matches[0], MatchesVisitInfo(visit_info1));
  EXPECT_THAT(matches[1], MatchesVisitInfo(visit_info2));
  EXPECT_THAT(matches[2], MatchesVisitInfo(visit_info3));

  // Delete the middle one.
  DeleteVisit(visit_info2);

  // The outer two should be left, and the last one should have the first as
  // the referrer.
  visit_info3.referring_visit = visit_info1.visit_id;
  matches.clear();
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(2U, matches.size());
  EXPECT_THAT(matches[0], MatchesVisitInfo(visit_info1));
  EXPECT_THAT(matches[1], MatchesVisitInfo(visit_info3));
}

TEST_F(VisitDatabaseTest, Update) {
  // Make something in the database.
  VisitRow original(1, Time::Now(), 23, ui::PageTransitionFromInt(0), 19, false,
                    0);
  AddVisit(&original);

  // Mutate that row.
  VisitRow modification(original);
  modification.url_id = 2;
  modification.transition = ui::PAGE_TRANSITION_TYPED;
  modification.visit_time = Time::Now() + base::Days(1);
  modification.referring_visit = 9292;
  modification.originator_cache_guid = "foobar_client";
  modification.originator_visit_id = 42;
  UpdateVisitRow(modification);

  // Check that the mutated version was written.
  VisitRow final;
  GetRowForVisit(original.visit_id, &final);
  EXPECT_THAT(final, MatchesVisitInfo(modification));
}

TEST_F(VisitDatabaseTest, IsKnownToSync) {
  // Insert three rows, VisitIDs 1, 2, and 3.
  for (VisitID i = 1; i <= 3; i++) {
    VisitRow original(i, Time::Now(), 23, ui::PageTransitionFromInt(0), 19,
                      false, 0);
    AddVisit(&original);
    ASSERT_EQ(i, original.visit_id);  // Verifies that we added 1, 2, and 3
  }

  // Set 2 and 3 to be `is_known_to_sync`.
  {
    VisitRow visit2;
    ASSERT_TRUE(GetRowForVisit(2, &visit2));
    EXPECT_FALSE(visit2.is_known_to_sync);
    visit2.is_known_to_sync = true;
    ASSERT_TRUE(UpdateVisitRow(visit2));

    VisitRow visit3;
    ASSERT_TRUE(GetRowForVisit(3, &visit3));
    EXPECT_FALSE(visit3.is_known_to_sync);
    visit3.is_known_to_sync = true;
    ASSERT_TRUE(UpdateVisitRow(visit3));
  }

  // Verify the new expected values for all visits.
  {
    VisitRow visit1;
    ASSERT_TRUE(GetRowForVisit(1, &visit1));
    EXPECT_FALSE(visit1.is_known_to_sync);
    VisitRow visit2;
    ASSERT_TRUE(GetRowForVisit(2, &visit2));
    EXPECT_TRUE(visit2.is_known_to_sync);
    VisitRow visit3;
    ASSERT_TRUE(GetRowForVisit(3, &visit3));
    EXPECT_TRUE(visit3.is_known_to_sync);
  }

  // Now clear out all `is_known_to_sync` bits and verify that worked.
  {
    SetAllVisitsAsNotKnownToSync();

    VisitRow visit1;
    ASSERT_TRUE(GetRowForVisit(2, &visit1));
    EXPECT_FALSE(visit1.is_known_to_sync);
    VisitRow visit2;
    ASSERT_TRUE(GetRowForVisit(2, &visit2));
    EXPECT_FALSE(visit2.is_known_to_sync);
    VisitRow visit3;
    ASSERT_TRUE(GetRowForVisit(3, &visit3));
    EXPECT_FALSE(visit3.is_known_to_sync);
  }
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitForURL_NoVisits) {
  const URLID kUrlId = 1U;

  // Should return 0 when there are no visits.
  VisitRow out_visit;
  EXPECT_EQ(GetMostRecentVisitForURL(kUrlId, &out_visit,
                                     VisitQuery404sPolicy::kInclude404s),
            0U);
  EXPECT_EQ(out_visit.visit_id, 0U);
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitForURL_Simple) {
  const URLID kUrlId = 1U;
  const base::Time kNow = Time::Now();

  // Add two visits for the same URL ID with different visit times.
  for (int visit_number = 1; visit_number <= 2; ++visit_number) {
    VisitRow visit;
    visit.url_id = kUrlId;
    visit.visit_id = visit_number;
    visit.visit_time = kNow - base::Days(visit_number);
    ASSERT_TRUE(AddVisit(&visit));
    ASSERT_EQ(visit_number, visit.visit_id);
  }

  // The more recent visit should be returned.
  VisitRow out_visit;
  EXPECT_EQ(GetMostRecentVisitForURL(kUrlId, &out_visit,
                                     VisitQuery404sPolicy::kInclude404s),
            1U);
  EXPECT_EQ(out_visit.visit_time, kNow - base::Days(1));
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitForURL_Tied) {
  const URLID kUrlId = 1U;
  const base::Time kNow = Time::Now();

  // Add two visits for the same URL with the same visit time.
  for (int visit_number = 1; visit_number <= 2; ++visit_number) {
    VisitRow visit;
    visit.url_id = kUrlId;
    visit.visit_id = visit_number;
    visit.visit_time = kNow;
    ASSERT_TRUE(AddVisit(&visit));
    ASSERT_EQ(visit_number, visit.visit_id);
  }

  // When more than one visit is tied for most recent, expect the highest visit
  // ID among the tied visits to be returned consistently. (These expectations
  // will flake if the tiebreaker isn't consistent.)
  VisitRow out_visit;
  EXPECT_EQ(GetMostRecentVisitForURL(kUrlId, &out_visit,
                                     VisitQuery404sPolicy::kInclude404s),
            2U);
  EXPECT_EQ(out_visit.visit_time, kNow);
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitsForURL_NoVisits) {
  const URLID kUrlId = 1U;

  // Should return an empty vector when there are no visits.
  VisitVector out_visits;
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 1, VisitQuery404sPolicy::kInclude404s, &out_visits));
  EXPECT_EQ(out_visits.size(), 0U);
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 1, VisitQuery404sPolicy::kExclude404s, &out_visits));
  EXPECT_EQ(out_visits.size(), 0U);
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitForURL_404Policy) {
  const URLID kUrlId = 1U;
  const base::Time kNow = Time::Now();
  VisitContextAnnotations context_annotations_non_404;
  context_annotations_non_404.on_visit = {.response_code = 500};
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};

  // Add a non-404 visit for the URL.
  VisitRow visit;
  visit.url_id = kUrlId;
  visit.visit_id = 1;
  visit.visit_time = kNow - base::Days(2);
  ASSERT_TRUE(AddVisit(&visit));
  ASSERT_EQ(1, visit.visit_id);

  // Add a visit with a 404 response code for the URL.
  VisitRow visit_404;
  visit_404.url_id = kUrlId;
  visit_404.visit_id = 2;
  visit_404.visit_time = kNow - base::Days(1);
  ASSERT_TRUE(AddVisit(&visit_404));
  AddContextAnnotationsForVisit(visit_404.visit_id, context_annotations_404);

  // When including 404s, the 404 visit should be returned as the recent visit.
  VisitRow out_visit;
  EXPECT_EQ(GetMostRecentVisitForURL(kUrlId, &out_visit,
                                     VisitQuery404sPolicy::kInclude404s),
            2U);
  EXPECT_EQ(out_visit.visit_time, kNow - base::Days(1));
  EXPECT_EQ(GetMostRecentVisitForURL(kUrlId, &out_visit,
                                     VisitQuery404sPolicy::kExclude404s),
            1U);
  EXPECT_EQ(out_visit.visit_time, kNow - base::Days(2));
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitsForURL_Simple) {
  const URLID kUrlId = 1U;
  const base::Time kNow = Time::Now();

  // Add two visits for the same URL ID with different visit times.
  for (int visit_number = 1; visit_number <= 2; ++visit_number) {
    VisitRow visit;
    visit.url_id = kUrlId;
    visit.visit_id = visit_number;
    visit.visit_time = kNow - base::Days(visit_number);
    ASSERT_TRUE(AddVisit(&visit));
    ASSERT_EQ(visit_number, visit.visit_id);
  }

  // Should return both visits in recency order, regardless of
  // `policy_for_404_visits`.
  VisitVector out_visits;
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 100, VisitQuery404sPolicy::kInclude404s, &out_visits));
  ASSERT_EQ(out_visits.size(), 2U);
  EXPECT_EQ(out_visits.front().visit_id, 1);
  EXPECT_EQ(out_visits.back().visit_id, 2);
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 100, VisitQuery404sPolicy::kExclude404s, &out_visits));
  ASSERT_EQ(out_visits.size(), 2U);
  EXPECT_EQ(out_visits.front().visit_id, 1);
  EXPECT_EQ(out_visits.back().visit_id, 2);
}

TEST_F(VisitDatabaseTest, GetMostRecentVisitsForURL_404Policy) {
  const URLID kUrlId = 1U;
  const base::Time kNow = Time::Now();
  VisitContextAnnotations context_annotations_non_404;
  context_annotations_non_404.on_visit = {.response_code = 500};
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};

  // Add a non-404 visit for the URL.
  VisitRow visit_non_404;
  visit_non_404.url_id = kUrlId;
  visit_non_404.visit_id = 1;
  visit_non_404.visit_time = kNow - base::Days(2);
  ASSERT_TRUE(AddVisit(&visit_non_404));
  ASSERT_EQ(visit_non_404.visit_id, 1);
  AddContextAnnotationsForVisit(visit_non_404.visit_id,
                                context_annotations_non_404);

  // Add a more recent 404 visit for the URL.
  VisitRow visit_404;
  visit_404.url_id = kUrlId;
  visit_404.visit_id = 2;
  visit_404.visit_time = kNow - base::Days(1);
  ASSERT_TRUE(AddVisit(&visit_404));
  ASSERT_EQ(visit_404.visit_id, 2);
  AddContextAnnotationsForVisit(visit_404.visit_id, context_annotations_404);

  // When including 404 visits, we should get both visits back with the 404
  // recent first.
  VisitVector out_visits;
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 100, VisitQuery404sPolicy::kInclude404s, &out_visits));
  ASSERT_EQ(out_visits.size(), 2U);
  EXPECT_THAT(out_visits.front(), MatchesVisitInfo(visit_404));
  EXPECT_THAT(out_visits.back(), MatchesVisitInfo(visit_non_404));
  ASSERT_TRUE(GetMostRecentVisitsForURL(
      kUrlId, 100, VisitQuery404sPolicy::kExclude404s, &out_visits));
  ASSERT_EQ(out_visits.size(), 1U);
  EXPECT_THAT(out_visits.front(), MatchesVisitInfo(visit_non_404));
}

TEST_F(VisitDatabaseTest, GetRedirectFromVisit) {
  // Add a visit chain: 1 -> 2 -> 3, where -> is a redirect.
  // Within a redirect chain, all visits have the same timestamp.
  GURL url1("http://www.google.com/url1");
  URLRow url_row1(url1);
  URLID url_id1 = AddURL(url_row1);
  ASSERT_NE(0, url_id1);
  VisitRow visit1(url_id1, base::Time::Now(), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit1));

  GURL url2("http://www.google.com/url2");
  URLRow url_row2(url2);
  URLID url_id2 = AddURL(url_row2);
  ASSERT_NE(0, url_id2);
  VisitRow visit2(
      url_id2, base::Time::Now(), visit1.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT),
      0, false, 0);
  ASSERT_TRUE(AddVisit(&visit2));

  GURL url3("http://www.google.com/url3");
  URLRow url_row3(url3);
  URLID url_id3 = AddURL(url_row3);
  ASSERT_NE(0, url_id3);
  VisitRow visit3(
      url_id3, base::Time::Now(), visit2.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  ASSERT_TRUE(AddVisit(&visit3));

  // Get redirect from visit2.
  VisitID to_visit_id = 0;
  GURL to_url;
  EXPECT_TRUE(GetRedirectFromVisit(visit2.visit_id, &to_visit_id, &to_url,
                                   VisitQuery404sPolicy::kInclude404s));
  EXPECT_EQ(visit3.visit_id, to_visit_id);
  EXPECT_EQ(GURL("http://www.google.com/url3"), to_url);

  // Get redirect from visit1.
  to_visit_id = 0;
  to_url = GURL();
  EXPECT_TRUE(GetRedirectFromVisit(visit1.visit_id, &to_visit_id, &to_url,
                                   VisitQuery404sPolicy::kInclude404s));
  EXPECT_EQ(visit2.visit_id, to_visit_id);
  EXPECT_EQ(GURL("http://www.google.com/url2"), to_url);

  // Get redirect from visit3 (no referrer)
  to_visit_id = 0;
  to_url = GURL();
  EXPECT_FALSE(GetRedirectFromVisit(visit3.visit_id, &to_visit_id, &to_url,
                                    VisitQuery404sPolicy::kInclude404s));
  EXPECT_EQ(0, to_visit_id);

  // Non-redirect case.
  VisitRow visit4(visit1.url_id, base::Time::Now(), 0, ui::PAGE_TRANSITION_LINK,
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit4));

  VisitRow visit5(visit2.url_id, base::Time::Now(), visit4.visit_id,
                  ui::PAGE_TRANSITION_LINK, 0, false, 0);
  ASSERT_TRUE(AddVisit(&visit5));

  // Get redirect from visit4. The referrer (visit5) is not a redirect.
  // The from_url part should fail.
  to_visit_id = 0;
  to_url = GURL();
  EXPECT_FALSE(GetRedirectFromVisit(visit4.visit_id, &to_visit_id, &to_url,
                                    VisitQuery404sPolicy::kInclude404s));
  EXPECT_EQ(to_visit_id, 0);
  EXPECT_TRUE(to_url.is_empty());
}

TEST_F(VisitDatabaseTest, GetRedirectToVisit_404Policy) {
  // Within a redirect chain, all visits have the same timestamp.
  GURL url1("http://www.google.com/url1");
  URLRow url_row1(url1);
  URLID url_id1 = AddURL(url_row1);
  ASSERT_NE(0, url_id1);
  VisitRow visit1(url_id1, base::Time::Now(), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit1));

  // Add a 404 visit
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};
  GURL url2("http://www.google.com/404");
  URLRow url_row2(url2);
  URLID url_id2 = AddURL(url_row2);
  ASSERT_NE(0, url_id2);
  VisitRow visit404(
      url_id2, base::Time::Now(), visit1.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  ASSERT_TRUE(AddVisit(&visit404));
  AddContextAnnotationsForVisit(visit404.visit_id, context_annotations_404);

  VisitID to_visit_id = 0;
  GURL to_url;
  EXPECT_TRUE(GetRedirectFromVisit(visit1.visit_id, &to_visit_id, &to_url,
                                   VisitQuery404sPolicy::kInclude404s));
  EXPECT_EQ(visit404.visit_id, to_visit_id);
  EXPECT_EQ(GURL("http://www.google.com/404"), to_url);

  // When 404s are disabled, redirects from visit1 should return false.
  to_visit_id = 0;
  to_url = GURL();
  EXPECT_FALSE(GetRedirectFromVisit(visit1.visit_id, &to_visit_id, &to_url,
                                    VisitQuery404sPolicy::kExclude404s));
  EXPECT_EQ(to_visit_id, 0);
  EXPECT_EQ(to_url.is_empty(), true);
}

TEST_F(VisitDatabaseTest, GetVisibleVisitCountToHost) {
  // Add a primary main frame non-redirect visit to a URL.
  GURL url("http://www.google.com/");
  URLRow url_row(url);
  URLID url_id = AddURL(url_row);
  ASSERT_NE(0, url_id);

  VisitRow visit1(url_id, base::Time::Now(), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit1));

  // Check that we have one visit.
  int count = 0;
  base::Time first_visit_time;
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(1, count);
  EXPECT_EQ(visit1.visit_time, first_visit_time);

  // Add a later visit to the same origin.
  VisitRow visit2(url_id, base::Time::Now() + base::Seconds(1), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit2));

  // The count should be updated, but the first visit time should stay the same.
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(2, count);
  EXPECT_EQ(visit1.visit_time, first_visit_time);

  // Add a visit with a 404 response code.
  GURL url2("http://www.google.com/foo");
  URLRow url_row2(url2);
  URLID url_id2 = AddURL(url_row2);
  ASSERT_NE(0, url_id2);

  VisitRow visit3(url_id2, base::Time::Now() + base::Seconds(2), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit3));
  VisitContextAnnotations annotations404;
  annotations404.on_visit.response_code = 404;
  AddContextAnnotationsForVisit(visit3.visit_id, annotations404);

  // Check that the 404 visit is not counted.
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(2, count);

  // Add a visit with a 403 response code.
  GURL url3("http://www.google.com/bar");
  URLRow url_row3(url3);
  URLID url_id3 = AddURL(url_row3);
  ASSERT_NE(0, url_id3);

  VisitRow visit4(url_id3, base::Time::Now() + base::Seconds(3), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit4));
  VisitContextAnnotations annotations403;
  annotations403.on_visit.response_code = 403;
  AddContextAnnotationsForVisit(visit4.visit_id, annotations403);

  // Check that the 200 visit is counted.
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(3, count);

  // Add a redirect visit to the same origin and verify it isn't counted.
  VisitRow visit5(url_id, base::Time::Now() + base::Seconds(4), 0,
                  ui::PAGE_TRANSITION_SERVER_REDIRECT, 0, false, 0);
  ASSERT_TRUE(AddVisit(&visit5));
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(3, count);

  // Add a subframe visit, which should not be counted.
  VisitRow visit6(url_id, base::Time::Now() + base::Seconds(5), 0,
                  ui::PAGE_TRANSITION_AUTO_SUBFRAME, 0, false, 0);
  ASSERT_TRUE(AddVisit(&visit6));
  EXPECT_TRUE(GetVisibleVisitCountToHost(url, &count, &first_visit_time));
  EXPECT_EQ(3, count);

  // Add a visit for a different origin (this one is HTTPS instead of HTTP).
  GURL url4("https://www.google.com/");
  URLRow url_row4(url4);
  URLID url_id4 = AddURL(url_row4);
  ASSERT_NE(0, url_id4);
  VisitRow visit7(url_id4, base::Time::Now() + base::Seconds(6), 0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  ASSERT_TRUE(AddVisit(&visit7));
  // We should only get visits for the specified origin.
  EXPECT_TRUE(GetVisibleVisitCountToHost(url4, &count, &first_visit_time));
  EXPECT_EQ(1, count);

  // We should succeed with a count of 0 for an origin with no visits.
  GURL url5("http://www.nevervisited.com/");
  EXPECT_TRUE(GetVisibleVisitCountToHost(url5, &count, &first_visit_time));
  EXPECT_EQ(0, count);

  // We should fail for non-HTTP / HTTPS URLs.
  GURL url6("ftp://ftp.example.com/");
  EXPECT_FALSE(GetVisibleVisitCountToHost(url6, &count, &first_visit_time));
}

namespace {

std::vector<VisitRow> GetTestVisitRows() {
  // Tests can be sensitive to the local timezone, so use a local time as the
  // basis for all visit times.
  base::Time base_time = Time::UnixEpoch().LocalMidnight();

  // Add one visit.
  VisitRow visit_info1(
      1, base_time + base::Minutes(1), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_info1.visit_id = 1;

  // Add second visit for the same page.
  VisitRow visit_info2(
      visit_info1.url_id, visit_info1.visit_time + base::Seconds(1), 1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true, 0);
  visit_info2.visit_id = 2;

  // Add third visit for a different page.
  VisitRow visit_info3(
      2, visit_info1.visit_time + base::Seconds(2), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START),
      0, false, 0);
  visit_info3.visit_id = 3;

  // Add a redirect visit from the last page.
  VisitRow visit_info4(
      3, visit_info1.visit_time + base::Seconds(3), visit_info3.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_info4.visit_id = 4;

  // Add a subframe visit.
  VisitRow visit_info5(
      4, visit_info1.visit_time + base::Seconds(4), visit_info4.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_info5.visit_id = 5;

  // Add third visit for the same URL as visit 1 and 2, but exactly a day
  // later than visit 2.
  VisitRow visit_info6(
      visit_info1.url_id, visit_info2.visit_time + base::Days(1), 1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true, 0);
  visit_info6.visit_id = 6;

  std::vector<VisitRow> test_visit_rows;
  test_visit_rows.push_back(visit_info1);
  test_visit_rows.push_back(visit_info2);
  test_visit_rows.push_back(visit_info3);
  test_visit_rows.push_back(visit_info4);
  test_visit_rows.push_back(visit_info5);
  test_visit_rows.push_back(visit_info6);
  return test_visit_rows;
}

}  // namespace

TEST_F(VisitDatabaseTest, GetVisitsForTimes) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i]));
  }

  // Query the visits for all our times.  We should get all visits.
  {
    std::vector<base::Time> times;
    for (size_t i = 0; i < test_visit_rows.size(); ++i) {
      times.push_back(test_visit_rows[i].visit_time);
    }
    VisitVector results;
    GetVisitsForTimes(times, &results);
    EXPECT_EQ(test_visit_rows.size(), results.size());
  }

  // Query the visits for a single time.
  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    std::vector<base::Time> times;
    times.push_back(test_visit_rows[i].visit_time);
    VisitVector results;
    GetVisitsForTimes(times, &results);
    ASSERT_EQ(1U, results.size());
    EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[i]));
  }
}

TEST_F(VisitDatabaseTest, GetAllAppIds) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  test_visit_rows[1].app_id = "org.chromium.dino";
  test_visit_rows[2].app_id = "org.chromium.cactus";
  test_visit_rows[3].app_id = "org.chromium.cactus";

  for (VisitRow visit_row : test_visit_rows) {
    EXPECT_TRUE(AddVisit(&visit_row));
  }

  // Query all the app IDS in the database after deduplicated,
  // the latest entries first.
  GetAllAppIdsResult result;
  result = GetAllAppIds();
  ASSERT_EQ(2U, result.app_ids.size());
  ASSERT_EQ(test_visit_rows[1].app_id, result.app_ids[1]);
  ASSERT_EQ(test_visit_rows[2].app_id, result.app_ids[0]);
}

TEST_F(VisitDatabaseTest, GetAllVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  test_visit_rows[1].app_id = "org.chromium.dino";
  test_visit_rows[2].app_id = "org.chromium.dino";

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i]));
  }

  // Query the visits for all time.  We should get all visits.
  VisitVector results;
  GetAllVisitsInRange(Time(), Time(), kNoAppIdFilter, 0, &results);
  ASSERT_EQ(6U, test_visit_rows.size());
  ASSERT_EQ(test_visit_rows.size(), results.size());
  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_THAT(results[i], MatchesVisitInfo(test_visit_rows[i]));
  }

  // Query the visits with an app ID. Only those with a given ID are returned.
  GetAllVisitsInRange(Time(), Time(), "org.chromium.dino", 0, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[2]));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  GetAllVisitsInRange(test_visit_rows[1].visit_time,
                      test_visit_rows[3].visit_time, kNoAppIdFilter, 0,
                      &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[2]));

  // Query for a max count and make sure we get only that number.
  GetAllVisitsInRange(Time(), Time(), kNoAppIdFilter, 1, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[0]));
}

TEST_F(VisitDatabaseTest, GetVisibleVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();
  // Add a 404 visit to the test visits.
  VisitRow visit_404(
      /*arg_url_id=*/100, test_visit_rows.front().visit_time,
      /*arg_referring_visit=*/0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      /*arg_segment_id=*/0, /*arg_incremented_omnibox_typed_score=*/false,
      /*arg_opener_visit=*/0);
  visit_404.visit_id = test_visit_rows.back().visit_id + 1;
  visit_404.app_id = "org.chromium.dino";
  test_visit_rows.push_back(visit_404);
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};

  test_visit_rows[1].app_id = "org.chromium.dino";
  test_visit_rows[2].app_id = "org.chromium.dino";
  test_visit_rows[3].app_id = "org.chromium.dino";

  for (auto& test_visit_row : test_visit_rows) {
    EXPECT_TRUE(AddVisit(&test_visit_row));
  }
  AddContextAnnotationsForVisit(visit_404.visit_id, context_annotations_404);

  // Query the visits for all time.
  VisitVector results;
  QueryOptions options;
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(3U, results.size());
#if !defined(ANDROID)
  // We should not get the first or the second visit (duplicates of the sixth)
  // or the redirect or subframe visits.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));
#else
  // On Android, the one with app_id is chosen among the duplicates.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
#endif

  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[3]));
  // Based on `options.policy_for_404_visits`, we should get the 404 visit.
  EXPECT_THAT(results[2], MatchesVisitInfo(visit_404));

  // Retry the query, but exclude 404s.
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  GetVisibleVisitsInRange(options, &results);
  // We shouldn't get the 404 visit anymore.
  ASSERT_EQ(2U, results.size());
#if !defined(ANDROID)
  // We should not get the first or the second visit (duplicates of the sixth)
  // or the redirect or subframe visits.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));
#else
  // On Android, the one with app_id is chosen among the duplicates.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
#endif

  // Query the visits with app_id. Only those with the matching app_id will be
  // returned. With app_id, the second visit is not a duplicate of the sixth.
  // Therefore the second and the fourth are returned. We include 404s, so the
  // 404 visit should also be returned.
  options.app_id = "org.chromium.dino";
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(3U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[3]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[2], MatchesVisitInfo(visit_404));

  // Query the visits with app_id, excluding 404s. The results should be the
  // same as above, but without the 404 visit.
  options.app_id = "org.chromium.dino";
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[3]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[1]));

  // Test the query with app_id including 404s, but in the reverse order.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(3U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_404));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[2], MatchesVisitInfo(test_visit_rows[3]));

  // Query with app_id but without 404s, in reverse order.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[3]));

  options = QueryOptions();  // Reset options to default.

  // Now try with only per-day de-duping -- the second visit should appear,
  // since it's a duplicate of visit6 but on a different day.
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(4U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[3]));
  EXPECT_THAT(results[2], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[3], MatchesVisitInfo(visit_404));

  // Now try without de-duping, expect to see all visible visits.
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(5U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[3]));
  EXPECT_THAT(results[2], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[3], MatchesVisitInfo(visit_404));
  EXPECT_THAT(results[4], MatchesVisitInfo(test_visit_rows[0]));

  // Set the end time to exclude the second visit. The first visit should be
  // returned. Even though the second is a more recent visit, it's not in the
  // query range.
  options.end_time = test_visit_rows[1].visit_time;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_404));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[0]));

  options = QueryOptions();  // Reset options to default.

  // Query for a max count and make sure we get only that number.
  options.max_count = 1;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  options.begin_time = test_visit_rows[1].visit_time;
  options.end_time = test_visit_rows[3].visit_time;
  options.max_count = 0;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));

  // Query oldest visits in a time range and make sure beginning is exclusive
  // and ending is inclusive.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[3]));
}

TEST_F(VisitDatabaseTest, VisitSource) {
  // Add visits.
  VisitRow visit_info1(111, Time::Now(), 0, ui::PAGE_TRANSITION_LINK, 0, false,
                       0);
  ASSERT_TRUE(AddVisit(&visit_info1));

  VisitRow visit_info2(112, Time::Now(), 1, ui::PAGE_TRANSITION_TYPED, 0, true,
                       0);
  visit_info2.source = SOURCE_SYNCED;
  ASSERT_TRUE(AddVisit(&visit_info2));

  VisitRow visit_info3(113, Time::Now(), 0, ui::PAGE_TRANSITION_TYPED, 0, true,
                       0);
  visit_info3.source = SOURCE_EXTENSION;
  ASSERT_TRUE(AddVisit(&visit_info3));

  // Query each visit.
  std::vector<VisitRow> matches;
  ASSERT_TRUE(GetVisitsForURL(111, &matches));
  ASSERT_EQ(1U, matches.size());
  VisitSourceMap sources;
  GetVisitsSource(matches, &sources);
  EXPECT_EQ(0U, sources.size());

  ASSERT_TRUE(GetVisitsForURL(112, &matches));
  ASSERT_EQ(1U, matches.size());
  GetVisitsSource(matches, &sources);
  ASSERT_EQ(1U, sources.size());
  EXPECT_EQ(SOURCE_SYNCED, sources[matches[0].visit_id]);

  ASSERT_TRUE(GetVisitsForURL(113, &matches));
  ASSERT_EQ(1U, matches.size());
  GetVisitsSource(matches, &sources);
  ASSERT_EQ(1U, sources.size());
  EXPECT_EQ(SOURCE_EXTENSION, sources[matches[0].visit_id]);
}

TEST_F(VisitDatabaseTest, GetVisibleVisitsForURL) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  test_visit_rows[1].app_id = "org.chromium.dino";
  test_visit_rows[2].app_id = "org.chromium.dino";
  test_visit_rows[3].app_id = "org.chromium.dino";
  test_visit_rows[5].app_id = "org.chromium.dino";

  VisitRow visit_info6 = test_visit_rows[5];

  // Add another visit for the same URL as visit 1, 2 and 6, but exactly a day
  // later than visit 6.
  VisitRow visit_info7(
      visit_info6.url_id, visit_info6.visit_time + base::Days(1), 1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true, 0);
  visit_info6.visit_id = 7;
  test_visit_rows.push_back(visit_info7);

  // Add another visit for the same URL as visits 1, 2, 6, and 7, with an app
  // id. We'll make this visit a 404 later.
  VisitRow visit_info8(
      visit_info7.url_id, visit_info7.visit_time + base::Seconds(1), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true, 0);
  visit_info8.visit_id = 8;
  visit_info8.app_id = "org.chromium.dino";
  test_visit_rows.push_back(visit_info8);

  // Add another visit for the same URL as visits 1, 2, 6, 7, and 8, with no app
  // id, that's more recent than visit 8. We'll make this visit a 404 later.
  VisitRow visit_info9(
      visit_info8.url_id, visit_info8.visit_time + base::Seconds(1), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true, 0);
  visit_info9.visit_id = 9;
  test_visit_rows.push_back(visit_info9);

  for (auto& test_visit_row : test_visit_rows) {
    ASSERT_TRUE(AddVisit(&test_visit_row));
  }
  // Make `visit_info8` a 404 visit.
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};
  AddContextAnnotationsForVisit(visit_info8.visit_id, context_annotations_404);
  // Make `visit_info9` a 404 visit.
  AddContextAnnotationsForVisit(visit_info9.visit_id, context_annotations_404);

  // Query the visits for the first url id, excluding 404s.
  VisitVector results;
  QueryOptions options;
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  int url_id = test_visit_rows[0].url_id;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(1U, results.size());
#if !defined(ANDROID)
  // We should not get the first, the second or the sixth (duplicates of the
  // seventh), the eighth or ninth (404), or any other urls, redirects or
  // subframe visits.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[6]));
#else
  // On Android, the one with app_id is chosen among the duplicates.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));
#endif

  // Repeat the same query, but include 404s.
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(1U, results.size());
#if !defined(ANDROID)
  // We should not get the first, the second, the sixth, the seventh, or the
  // eighth (duplicates of the ninth) or any other urls, redirects or subframe
  // visits.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[8]));
#else
  // On Android, the ones with an app_id are chosen among the duplicates. Visit
  // 8 is the most recent of those.
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[7]));
#endif

  // Query the visits with app_id, excluding 404s. Only non-404 visits matching
  // both url id (1,2,6,7) and app id(2,3,4,6) will be returned(2, 6) -> 6
  // (deduped).
  options.app_id = "org.chromium.dino";
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[5]));

  // Query the visits with app_id again, including 404s this time. All visits
  // matching both url id (1,2,6,7,8,9) and app id(2,3,4,6,8) will be
  // returned(2,6,8) -> 8 (deduped).
  options.app_id = "org.chromium.dino";
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[7]));

  // Test the query with app_id, but in the reverse order.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  options.policy_for_404_visits = VisitQuery404sPolicy::kExclude404s;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));

  options = QueryOptions();  // Reset options to default.

  // Now try with only per-day de-duping -- the second visit should appear,
  // since it's a duplicate of visit6 but on a different day.
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(3U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[6]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[5]));
  EXPECT_THAT(results[2], MatchesVisitInfo(test_visit_rows[1]));

  // Now try without de-duping, expect to see all visible visits to url id 1.
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(4U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[6]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[5]));
  EXPECT_THAT(results[2], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[3], MatchesVisitInfo(test_visit_rows[0]));

  // Now try with a `max_count` limit to get the newest 2 visits only.
  options.max_count = 2;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[6]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[5]));

  // Try `max_count` again, including 404s this time.
  options.policy_for_404_visits = VisitQuery404sPolicy::kInclude404s;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[8]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[7]));

  // Now try getting the oldest 2 visits and make sure they're ordered oldest
  // first.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[0]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[1]));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  options.begin_time = test_visit_rows[0].visit_time;
  options.end_time = test_visit_rows[5].visit_time;
  options.visit_order = QueryOptions::RECENT_FIRST;
  options.max_count = 0;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[0]));

  // Query oldest visits in a time range and make sure beginning is exclusive
  // and ending is inclusive.
  options.visit_order = QueryOptions::OLDEST_FIRST;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(test_visit_rows[1]));
  EXPECT_THAT(results[1], MatchesVisitInfo(test_visit_rows[5]));
}

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(VisitDatabaseTest, GetVisibleVisits_ActorVisits) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistoryActorIntegrationM2);

  const URLID kUrlId1 = 1U;
  VisitRow visit_browsed(
      kUrlId1, Time::Now(), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  ASSERT_TRUE(AddVisit(&visit_browsed));
  visit_browsed.source = SOURCE_BROWSED;

  VisitRow visit_actor(
      kUrlId1, Time::Now() + base::Seconds(1), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_actor.source = SOURCE_ACTOR;
  EXPECT_TRUE(AddVisit(&visit_actor));

  QueryOptions options;
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  VisitVector results;

  // By default, actor visits should be excluded from GetVisibleVisitsForURL.
  GetVisibleVisitsForURL(kUrlId1, options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_browsed));

  // When explicitly requested, they should be included.
  options.include_actor_visits = true;
  GetVisibleVisitsForURL(kUrlId1, options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_actor));
  EXPECT_THAT(results[1], MatchesVisitInfo(visit_browsed));

  options = QueryOptions();
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;

  // By default, actor visits should be excluded from GetVisibleVisitsInRange.
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(1U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_browsed));

  // When explicitly requested, they should be included.
  options.include_actor_visits = true;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(2U, results.size());
  EXPECT_THAT(results[0], MatchesVisitInfo(visit_actor));
  EXPECT_THAT(results[1], MatchesVisitInfo(visit_browsed));
}

TEST_F(VisitDatabaseTest, GetVisibleVisits_SeparateBySource) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kBrowsingHistoryActorIntegrationM2);

  const URLID kUrlId = 1U;
  const Time kDay = Time::Now().LocalMidnight() + base::Hours(1);

  // 1. Add first user visit.
  VisitRow visit_user1(kUrlId, kDay + base::Seconds(1), 0,
                       ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                                 ui::PAGE_TRANSITION_CHAIN_END),
                       0, false, 0);
  ASSERT_TRUE(AddVisit(&visit_user1));

  // 2. Add second user visit (duplicate for the day). Should be dropped.
  VisitRow visit_user2(kUrlId, kDay + base::Seconds(2), 0,
                       ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                                 ui::PAGE_TRANSITION_CHAIN_END),
                       0, false, 0);
  ASSERT_TRUE(AddVisit(&visit_user2));

  // 3. Add first actor visit.
  VisitRow visit_actor1(
      kUrlId, kDay + base::Seconds(3), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_actor1.source = SOURCE_ACTOR;
  ASSERT_TRUE(AddVisit(&visit_actor1));

  // 4. Add second actor visit (duplicate for the day). Should be dropped.
  VisitRow visit_actor2(
      kUrlId, kDay + base::Seconds(4), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  visit_actor2.source = SOURCE_ACTOR;
  ASSERT_TRUE(AddVisit(&visit_actor2));

  // 5. Add a different user visit on a different URL on the same day.
  VisitRow visit_user_other_url(
      2U, kDay + base::Seconds(5), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  ASSERT_TRUE(AddVisit(&visit_user_other_url));

  QueryOptions options;
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  options.include_actor_visits = true;
  options.begin_time = kDay;
  options.end_time = kDay + base::Days(1);

  VisitVector results;
  GetVisibleVisitsInRange(options, &results);

  // Expected result count:
  // - 1 unique actor visit (visit_actor2, oldest one dropped)
  // - 1 unique user visit (visit_user2, oldest one dropped)
  // - 1 unique other URL user visit.
  // Total: 3 visits.
  ASSERT_EQ(3U, results.size());
  //
  // Ordering of Kept Visits in output (Time DESC):
  // 1. ID 5 (Time + 5s) - visit_user_other_url
  // 2. ID 4 (Time + 4s) - visit_actor2
  // 3. ID 2 (Time + 2s) - visit_user2
  EXPECT_THAT(results, ElementsAre(MatchesVisitInfo(visit_user_other_url),
                                   MatchesVisitInfo(visit_actor2),
                                   MatchesVisitInfo(visit_user2)));
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

TEST_F(VisitDatabaseTest, GetHistoryCount) {
  // Start with a day in the middle of summer, so that we are nowhere near
  // DST shifts.
  Time today;
  ASSERT_TRUE(Time::FromString("2015-07-07", &today));
  Time yesterday = today - base::Days(1);
  Time two_days_ago = yesterday - base::Days(1);
  Time now = two_days_ago;

  ui::PageTransition standard_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  VisitContextAnnotations context_annotations_401;
  context_annotations_401.on_visit = {.response_code = 401};
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};

  // Add 5 visits (3 distinct URLs) for the day before yesterday.
  // One of the URLs has only 404 visits, and the others have non-404 visits.
  // Whether the URL was browsed on this machine or synced has no effect.
  VisitRow first_day_1(1, now, 0, standard_transition, 0, true, 0);
  first_day_1.visit_id = 1;
  AddVisit(&first_day_1);
  now += base::Hours(1);

  VisitRow first_day_2(2, now, 0, standard_transition, 0, true, 0);
  first_day_2.visit_id = 2;
  AddVisit(&first_day_2);
  AddContextAnnotationsForVisit(first_day_2.visit_id, context_annotations_401);
  now += base::Hours(1);

  VisitRow first_day_3(1, now, 0, standard_transition, 0, true, 0);
  first_day_3.visit_id = 3;
  first_day_3.source = SOURCE_SYNCED;
  AddVisit(&first_day_3);
  now += base::Hours(1);

  VisitRow first_day_4(3, now, 0, standard_transition, 0, true, 0);
  first_day_4.visit_id = 4;
  first_day_4.source = SOURCE_SYNCED;
  AddVisit(&first_day_4);
  AddContextAnnotationsForVisit(first_day_4.visit_id, context_annotations_404);
  now += base::Hours(1);

  VisitRow first_day_5(2, now, 0, standard_transition, 0, true, 0);
  first_day_5.visit_id = 5;
  AddVisit(&first_day_5);
  AddContextAnnotationsForVisit(first_day_5.visit_id, context_annotations_401);
  now += base::Hours(1);

  // Add 4 more visits for yesterday. One of them is invalid, as it's not a
  // user-visible navigation. Of the remaining 3, only 2 are unique, and only 1
  // of those has a non-404 visit.
  now = yesterday;

  VisitRow second_day_1(1, now, 0, standard_transition, 0, true, 0);
  second_day_1.visit_id = 6;
  AddVisit(&second_day_1);
  AddContextAnnotationsForVisit(second_day_1.visit_id, context_annotations_401);
  now += base::Hours(1);

  VisitRow second_day_2(1, now, 0, standard_transition, 0, true, 0);
  second_day_2.visit_id = 7;
  AddVisit(&second_day_2);
  AddContextAnnotationsForVisit(second_day_2.visit_id, context_annotations_401);
  now += base::Hours(1);

  VisitRow second_day_3(2, now, 0, ui::PAGE_TRANSITION_AUTO_SUBFRAME, 0, false,
                        0);
  second_day_3.visit_id = 8;
  AddVisit(&second_day_3);
  now += base::Hours(1);

  VisitRow second_day_4(3, now, 0, standard_transition, 0, true, 0);
  second_day_4.visit_id = 9;
  AddVisit(&second_day_4);
  AddContextAnnotationsForVisit(second_day_4.visit_id, context_annotations_404);
  now += base::Hours(1);

  int result;

  // There were 3 distinct URLs two days ago.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, yesterday,
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(3, result);

  // But only two if we exclude 404s.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, yesterday,
                              VisitQuery404sPolicy::kExclude404s, &result));
  EXPECT_EQ(2, result);

  // For both previous days, there should be 5 per-day unique URLs.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, today,
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(5, result);

  // But only 3 if we exclude 404s.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, today,
                              VisitQuery404sPolicy::kExclude404s, &result));
  EXPECT_EQ(3, result);

  // Since we only have entries for the two previous days, the infinite time
  // range should yield the same result.
  EXPECT_TRUE(GetHistoryCount(Time(), Time::Max(),
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(5, result);

  // Narrowing the range to exclude `first_day_1` will still return 5,
  // because `first_day_1` is not unique.
  EXPECT_TRUE(GetHistoryCount(two_days_ago + base::Hours(2), today,
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(5, result);

  // Narrowing the range to exclude `second_day_4` will return 4,
  // because `second_day_4` is unique.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, yesterday + base::Hours(3),
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(4, result);

  // Narrowing the range to exclude both `first_day_1` and `second_day_4` will
  // still return 4.
  EXPECT_TRUE(GetHistoryCount(two_days_ago + base::Hours(2),
                              yesterday + base::Hours(3),
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(4, result);

  // A range that contains no visits will return 0.
  EXPECT_TRUE(GetHistoryCount(two_days_ago + base::Microseconds(1),
                              two_days_ago + base::Hours(1),
                              VisitQuery404sPolicy::kInclude404s, &result));
  EXPECT_EQ(0, result);

  // If this timezone uses DST, test the behavior on days when the time
  // is shifted forward and backward. Note that in this case we cannot use
  // base::Days(1) to move one day, as this simply removes 24 hours and
  // thus does not work correctly with DST shifts. Instead, we'll go back
  // 1 second (i.e. somewhere in the middle of the previous day), and use
  // `LocalMidnight()` to round down to the beginning of the day in the local
  // time, taking timezones and DST into account. This is necessary to achieve
  // the same equivalence class on days as the DATE(..., 'localtime') function
  // in SQL.
  Time shift_forward;
  Time shift_backward;
  Time current_day = (two_days_ago - base::Seconds(1)).LocalMidnight();
  for (int i = 0; i < 366; i++) {
    current_day = (current_day - base::Seconds(1)).LocalMidnight();
    Time after_24_hours = current_day + base::Hours(24);

    if (current_day == after_24_hours.LocalMidnight()) {
      // More than 24 hours. Shift backward.
      shift_backward = current_day;
    } else if (after_24_hours > after_24_hours.LocalMidnight()) {
      // Less than 24 hours. Shift forward.
      shift_forward = current_day;
    }

    if (!shift_backward.is_null() && !shift_forward.is_null())
      break;
  }

  // Test the backward shift. Add two visits for the same page on midnight and
  // 24 hours later. The count should be 1, not 2, because the day is longer
  // than 24 hours, and the two visits will be regarded as duplicate.
  if (!shift_backward.is_null()) {
    VisitRow backward_1(1, shift_backward, 0, standard_transition, 0, true, 0);
    backward_1.visit_id = 10;
    AddVisit(&backward_1);

    VisitRow backward_2(1, shift_backward + base::Hours(24), 0,
                        standard_transition, 0, true, 0);
    backward_2.visit_id = 11;
    AddVisit(&backward_2);

    EXPECT_TRUE(GetHistoryCount(shift_backward,
                                shift_backward + base::Hours(25),
                                VisitQuery404sPolicy::kInclude404s, &result));
    EXPECT_EQ(1, result);
  }

  // Test the forward shift. Add two visits for the same page at midnight and
  // almost 24 hours later. The count should be 2, not 1. The visits would be
  // regarded as duplicate in a normal 24 hour day, but in this case the second
  // visit is already in the next day.
  if (!shift_forward.is_null()) {
    VisitRow forward_1(1, shift_forward, 0, standard_transition, 0, true, 0);
    forward_1.visit_id = 12;
    AddVisit(&forward_1);

    Time almost_24_hours_later =
        shift_forward + base::Hours(24) - base::Microseconds(1);
    VisitRow forward_2(1, almost_24_hours_later, 0, standard_transition, 0,
                       true, 0);
    forward_2.visit_id = 13;
    AddVisit(&forward_2);

    EXPECT_TRUE(GetHistoryCount(shift_forward, shift_forward + base::Hours(24),
                                VisitQuery404sPolicy::kInclude404s, &result));
    EXPECT_EQ(2, result);
  }
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_BadURL) {
  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_FALSE(GetLastVisitToOrigin(
      url::Origin(), base::Time::Min(), base::Time::Max(),
      VisitQuery404sPolicy::kInclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_NonHttpURL) {
  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_FALSE(GetLastVisitToOrigin(url::Origin::Create(GURL("ftp://host/")),
                                    base::Time::Min(), base::Time::Max(),
                                    VisitQuery404sPolicy::kInclude404s,
                                    &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_NoVisits) {
  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), base::Time::Min(),
      base::Time::Max(), VisitQuery404sPolicy::kInclude404s, &last_visit,
      &last_visited_url));
  EXPECT_EQ(last_visit, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_VisitsOutsideRange) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time - base::Hours(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org"))),
                end_time + base::Hours(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), begin_time,
      end_time, VisitQuery404sPolicy::kInclude404s, &last_visit,
      &last_visited_url));
  EXPECT_EQ(last_visit, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_EndTimeNotIncluded) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org"))),
                end_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), begin_time,
      end_time, VisitQuery404sPolicy::kInclude404s, &last_visit,
      &last_visited_url));
  EXPECT_EQ(last_visit, begin_time);
  EXPECT_EQ(last_visited_url, GURL("https://www.chromium.org"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_SameOriginOnly) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://other.origin.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org/path?query=foo"))),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), begin_time,
      end_time, VisitQuery404sPolicy::kInclude404s, &last_visit,
      &last_visited_url));
  EXPECT_EQ(last_visit, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, GURL("https://www.chromium.org/path?query=foo"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_MostRecentVisitTime) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://chromium.org/"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org/"))),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row2);
  VisitRow row3{AddURL(URLRow(GURL("https://www.chromium.org/"))),
                begin_time + base::Minutes(2),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  AddVisit(&row3);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), begin_time,
      end_time, VisitQuery404sPolicy::kInclude404s, &last_visit,
      &last_visited_url));
  EXPECT_EQ(last_visit, begin_time + base::Minutes(2));
  EXPECT_EQ(last_visited_url, GURL("https://www.chromium.org"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToOrigin_PolicyFor404Visits) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);
  VisitContextAnnotations context_annotations_200;
  context_annotations_200.on_visit = {.response_code = 200};
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};

  const GURL kChromiumUrl("https://chromium.org/");

  // Add two visits to the same origin. The more recent one is a 404, and the
  // older one is a 200.
  VisitRow row1{AddURL(URLRow(kChromiumUrl)),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  row1.visit_id = AddVisit(&row1);
  AddContextAnnotationsForVisit(row1.visit_id, context_annotations_200);
  VisitRow row2{AddURL(URLRow(kChromiumUrl)),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  row2.visit_id = AddVisit(&row2);
  AddContextAnnotationsForVisit(row2.visit_id, context_annotations_404);

  base::Time last_visit;
  GURL last_visited_url;
  // When including 404s, the most recent visit to the origin is the 404 visit.
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(kChromiumUrl), begin_time, end_time,
      VisitQuery404sPolicy::kInclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, row2.visit_time);
  EXPECT_EQ(last_visited_url, kChromiumUrl);
  // When excluding 404s, the most recent visit to the origin is the 200 visit.
  EXPECT_TRUE(GetLastVisitToOrigin(
      url::Origin::Create(kChromiumUrl), begin_time, end_time,
      VisitQuery404sPolicy::kExclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, row1.visit_time);
  EXPECT_EQ(last_visited_url, kChromiumUrl);
  // When excluding 404s with a time window that includes only the 404 visit,
  // the call succeeds but returns no timestamp.
  EXPECT_TRUE(GetLastVisitToOrigin(url::Origin::Create(kChromiumUrl),
                                   begin_time + base::Seconds(1), end_time,
                                   VisitQuery404sPolicy::kExclude404s,
                                   &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_DifferentScheme) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("http://www.chromium.org"))),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  VisitRow row;
  EXPECT_TRUE(GetLastVisitToHost(
      GURL("https://www.chromium.org").GetHost(), begin_time, end_time,
      VisitQuery404sPolicy::kInclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, GURL("http://www.chromium.org"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_IncludePort) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org:8080"))),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToHost(
      GURL("https://www.chromium.org").GetHost(), begin_time, end_time,
      VisitQuery404sPolicy::kInclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, GURL("https://www.chromium.org:8080"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_DifferentPorts) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org:8080"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row1);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org:32256"))),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  AddVisit(&row2);

  base::Time last_visit;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToHost(
      GURL("https://www.chromium.org:8080").GetHost(), begin_time, end_time,
      VisitQuery404sPolicy::kInclude404s, &last_visit, &last_visited_url));
  EXPECT_EQ(last_visit, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, GURL("https://www.chromium.org:32256"));
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_Only404Entry) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  // Add a 404 visit.
  const GURL k404URL("https://www.chromium.org");
  VisitRow row1{AddURL(URLRow(k404URL)),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  row1.visit_id = AddVisit(&row1);
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};
  AddContextAnnotationsForVisit(row1.visit_id, context_annotations_404);

  // That visit should appear as the most recent visit when including 404s.
  base::Time last_visit_time;
  GURL last_visited_url;
  EXPECT_TRUE(GetLastVisitToHost(k404URL.GetHost(), begin_time, end_time,
                                 VisitQuery404sPolicy::kInclude404s,
                                 &last_visit_time, &last_visited_url));
  EXPECT_EQ(last_visit_time, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, k404URL);

  // No visit should appear when excluding 404s, but the call should succeed.
  EXPECT_TRUE(GetLastVisitToHost(k404URL.GetHost(), begin_time, end_time,
                                 VisitQuery404sPolicy::kExclude404s,
                                 &last_visit_time, &last_visited_url));
  EXPECT_EQ(last_visit_time, base::Time());
  EXPECT_EQ(last_visited_url, GURL());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_404) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Hours(1);

  // Add a non-404 visit.
  const GURL kEarlierVisitNon404Url("https://www.chromium.org/path?query=foo");
  VisitRow row1{AddURL(URLRow(kEarlierVisitNon404Url)),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false,
                0};
  row1.visit_id = AddVisit(&row1);

  // Add a later 404 visit to the same host.
  const GURL kLaterVisit404Url("https://www.chromium.org");
  VisitRow row2{AddURL(URLRow(kLaterVisit404Url)),
                begin_time + base::Minutes(1),
                0,
                ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                          ui::PAGE_TRANSITION_CHAIN_START |
                                          ui::PAGE_TRANSITION_CHAIN_END),
                0,
                false,
                0};
  row2.visit_id = AddVisit(&row2);
  VisitContextAnnotations context_annotations_404;
  context_annotations_404.on_visit = {.response_code = 404};
  AddContextAnnotationsForVisit(row2.visit_id, context_annotations_404);

  EXPECT_EQ(kEarlierVisitNon404Url.GetHost(), kLaterVisit404Url.GetHost());

  base::Time last_visit_time;
  GURL last_visited_url;
  // The 404 visit should still appear as the most recent visit when including
  // 404s.
  EXPECT_TRUE(GetLastVisitToHost(kLaterVisit404Url.GetHost(), begin_time,
                                 end_time, VisitQuery404sPolicy::kInclude404s,
                                 &last_visit_time, &last_visited_url));
  EXPECT_EQ(last_visit_time, begin_time + base::Minutes(1));
  EXPECT_EQ(last_visited_url, kLaterVisit404Url);

  // The older, non-404 should now appear when excluding 404s.
  EXPECT_TRUE(GetLastVisitToHost(kLaterVisit404Url.GetHost(), begin_time,
                                 end_time, VisitQuery404sPolicy::kExclude404s,
                                 &last_visit_time, &last_visited_url));
  EXPECT_EQ(last_visit_time, begin_time);
  EXPECT_EQ(last_visited_url, kEarlierVisitNon404Url);
}

TEST_F(VisitDatabaseTest, GetDailyVisitsToOrigin_WithVisits) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Days(10);

  base::Time day1_time = begin_time.LocalMidnight() + base::Hours(24);
  base::Time day2_time = day1_time + base::Hours(24);

  auto add_visit = [&](const GURL& url, base::Time visit_time) {
    VisitRow row{AddURL(URLRow(url)),
                 visit_time,
                 0,
                 ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                           ui::PAGE_TRANSITION_CHAIN_START |
                                           ui::PAGE_TRANSITION_CHAIN_END),
                 0,
                 false,
                 0};
    AddVisit(&row);
  };
  // One visit before time range.
  add_visit(GURL("https://foo.com/"), begin_time - base::Hours(1));
  // Two visits on first day.
  add_visit(GURL("https://foo.com/bar"), day1_time);
  add_visit(GURL("https://foo.com/baz"),
            day1_time + base::Hours(24) - base::Seconds(1));
  // Five visits on the next day.
  for (int i = 0; i < 5; ++i) {
    add_visit(GURL("https://foo.com/bar"), day2_time);
  }
  // These visits are for different origins (different scheme / host / port).
  add_visit(GURL("http://foo.com/bar"), day2_time);
  add_visit(GURL("https://fun.foo.com"), day2_time);
  add_visit(GURL("https://foo.com:123/bar"), day2_time);

  // One visit after end_time.
  add_visit(GURL("https://foo.com/bar"), end_time + base::Seconds(1));

  DailyVisitsResult result = GetDailyVisitsToOrigin(
      url::Origin::Create(GURL("https://foo.com")), begin_time, end_time,
      VisitQuery404sPolicy::kInclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(2, result.days_with_visits);
  EXPECT_EQ(7, result.total_visits);
}

TEST_F(VisitDatabaseTest, GetDailyVisitsToOrigin_NoVisits) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::Days(10);

  // A non-user visible visit.
  VisitRow row{AddURL(URLRow(GURL("https://www.chromium.org"))),
               begin_time,
               0,
               ui::PageTransitionFromInt(0),
               0,
               false,
               0};
  AddVisit(&row);

  DailyVisitsResult result = GetDailyVisitsToOrigin(
      url::Origin::Create(GURL("https://www.chromium.org")), begin_time,
      end_time, VisitQuery404sPolicy::kInclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(0, result.days_with_visits);
  EXPECT_EQ(0, result.total_visits);
}

TEST_F(VisitDatabaseTest, GetDailyVisitsToOrigin_404s) {
  // Use a fixed time of day to prevent flakes when run near day boundaries.
  base::Time begin_time = base::Time::Now().LocalMidnight();
  base::Time end_time = begin_time + base::Days(10);

  auto add_visit = [&](const GURL& url, base::Time visit_time,
                       int response_code) {
    VisitRow row{AddURL(URLRow(url)),
                 visit_time,
                 0,
                 ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                           ui::PAGE_TRANSITION_CHAIN_START |
                                           ui::PAGE_TRANSITION_CHAIN_END),
                 0,
                 false,
                 0};
    AddVisit(&row);
    VisitContextAnnotations annotations;
    annotations.on_visit.response_code = response_code;
    AddContextAnnotationsForVisit(row.visit_id, annotations);
  };

  // `origin1` has only a single visit in the time range, and it's a 404.
  url::Origin origin1 = url::Origin::Create(GURL("https://foo.com"));
  add_visit(GURL("https://foo.com/404"), begin_time, 404);

  // When including 404s for `origin1`, we should get 1 day with 1 visit.
  DailyVisitsResult result = GetDailyVisitsToOrigin(
      origin1, begin_time, end_time, VisitQuery404sPolicy::kInclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(1, result.days_with_visits);
  EXPECT_EQ(1, result.total_visits);

  // When excluding 404s for `origin1`, we should get 0 days with visits, 0
  // visits total.
  result = GetDailyVisitsToOrigin(origin1, begin_time, end_time,
                                  VisitQuery404sPolicy::kExclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(0, result.days_with_visits);
  EXPECT_EQ(0, result.total_visits);

  // `origin2` has two visits on a single day in the time range: one is a 404
  // and the other is not.
  url::Origin origin2 = url::Origin::Create(GURL("https://bar.com"));
  add_visit(GURL("https://bar.com/404"), begin_time, 404);
  add_visit(GURL("https://bar.com/200"), begin_time + base::Hours(1), 200);

  // When including 404s for `origin2`, we should get 1 day with 2 visits.
  result = GetDailyVisitsToOrigin(origin2, begin_time, end_time,
                                  VisitQuery404sPolicy::kInclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(1, result.days_with_visits);
  EXPECT_EQ(2, result.total_visits);

  // When excluding 404s for `origin2`, we should still get 1 day, but with only
  // 1 visit.
  result = GetDailyVisitsToOrigin(origin2, begin_time, end_time,
                                  VisitQuery404sPolicy::kExclude404s);
  EXPECT_TRUE(result.success);
  EXPECT_EQ(1, result.days_with_visits);
  EXPECT_EQ(1, result.total_visits);
}

TEST_F(VisitDatabaseTest, GetGoogleDomainVisitsFromSearchesInRange_NoVisits) {
  const auto begin_time = base::Time::Now();
  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::Days(1)),
              IsEmpty());
}

TEST_F(VisitDatabaseTest,
       GetGoogleDomainVisitsFromSearchesInRange_TwoVistsInRange) {
  const auto begin_time = base::Time::Now();
  // Out of range, one hour before begin time.
  VisitRow row{AddURL(URLRow(GURL("https://www.google.fr/search?q=foo"))),
               begin_time + base::Hours(-1),
               0,
               ui::PageTransitionFromInt(0),
               0,
               false,
               0};
  AddVisit(&row);
  // In range, exactly begin time.
  row = {AddURL(URLRow(GURL("https://www.google.com/search?q=foo"))),
         begin_time,
         0,
         ui::PageTransitionFromInt(0),
         0,
         false,
         false};
  AddVisit(&row);
  // In range, 23 hours after begin time.
  row = {AddURL(URLRow(GURL("https://www.google.ch/search?q=foo"))),
         begin_time + base::Hours(23),
         0,
         ui::PageTransitionFromInt(0),
         0,
         false,
         false};
  AddVisit(&row);
  // Out of range, exactly a day after begin time.
  row = {AddURL(URLRow(GURL("https://www.google.de/search?q=foo"))),
         begin_time + base::Hours(24),
         0,
         ui::PageTransitionFromInt(0),
         0,
         false,
         false};
  AddVisit(&row);

  EXPECT_THAT(
      GetGoogleDomainVisitsFromSearchesInRange(begin_time,
                                               begin_time + base::Days(1)),
      ElementsAre(AllOf(Property(&DomainVisit::domain, "www.google.com"),
                        Property(&DomainVisit::visit_time, begin_time)),
                  AllOf(Property(&DomainVisit::domain, "www.google.ch"),
                        Property(&DomainVisit::visit_time,
                                 begin_time + base::Hours(23)))));
}

TEST_F(VisitDatabaseTest, GetGoogleDomainVisitsFromSearchesInRange_NotSearch) {
  const auto begin_time = base::Time::Now();
  VisitRow row{AddURL(URLRow(GURL("https://www.google.fr/searchin"))),
               begin_time,
               0,
               ui::PageTransitionFromInt(0),
               0,
               false,
               0};
  AddVisit(&row);

  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::Days(1)),
              IsEmpty());
}

TEST_F(VisitDatabaseTest,
       GetGoogleDomainVisitsFromSearchesInRange_InvalidGoogleDomain) {
  const auto begin_time = base::Time::Now();
  VisitRow row{AddURL(URLRow(GURL("https://www.google.foo/search?q=foo"))),
               begin_time,
               0,
               ui::PageTransitionFromInt(0),
               0,
               false,
               0};
  AddVisit(&row);

  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::Days(1)),
              IsEmpty());
}

TEST_F(VisitDatabaseTest, GetLastRowForVisitByVisitTime) {
  const base::Time kVisitTime1 = base::Time::Now();
  const base::Time kVisitTime2 = base::Time::Now() - base::Minutes(2);
  const base::Time kVisitTime3 = base::Time::Now() + base::Minutes(3);

  // Add some visits including redirect chains. Within a redirect chain, all
  // visits have the same timestamp.
  URLID url_id = 0;

  VisitRow visit1(++url_id, kVisitTime1, /*arg_referring_visit=*/0,
                  ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                            ui::PAGE_TRANSITION_CHAIN_START |
                                            ui::PAGE_TRANSITION_CHAIN_END),
                  0, false, 0);
  EXPECT_TRUE(AddVisit(&visit1));

  VisitRow visit2a(++url_id, kVisitTime2, /*arg_referring_visit=*/0,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START),
                   0, false, 0);
  EXPECT_TRUE(AddVisit(&visit2a));
  VisitRow visit2b(
      ++url_id, kVisitTime2, /*arg_referring_visit=*/visit2a.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  EXPECT_TRUE(AddVisit(&visit2b));

  VisitRow visit3a(++url_id, kVisitTime3, /*arg_referring_visit=*/0,
                   ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                             ui::PAGE_TRANSITION_CHAIN_START),
                   0, false, 0);
  EXPECT_TRUE(AddVisit(&visit3a));
  VisitRow visit3b(
      ++url_id, kVisitTime3, /*arg_referring_visit=*/visit3a.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CLIENT_REDIRECT),
      0, false, 0);
  EXPECT_TRUE(AddVisit(&visit3b));
  VisitRow visit3c(
      ++url_id, kVisitTime3, /*arg_referring_visit=*/visit3b.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false, 0);
  EXPECT_TRUE(AddVisit(&visit3c));

  // In all cases, GetLastRowForVisitByVisitTime should return the last entry of
  // the chain (because that one was added last).
  VisitRow result1;
  GetLastRowForVisitByVisitTime(kVisitTime1, &result1);
  EXPECT_THAT(result1, MatchesVisitInfo(visit1));
  VisitRow result2;
  GetLastRowForVisitByVisitTime(kVisitTime2, &result2);
  EXPECT_THAT(result2, MatchesVisitInfo(visit2b));
  VisitRow result3;
  GetLastRowForVisitByVisitTime(kVisitTime3, &result3);
  EXPECT_THAT(result3, MatchesVisitInfo(visit3c));
}

}  // namespace history
