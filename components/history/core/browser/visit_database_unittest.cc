// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <set>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/visit_database.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using base::Time;
using base::TimeDelta;
using testing::AllOf;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::Property;

namespace history {

namespace {

bool IsVisitInfoEqual(const VisitRow& a, const VisitRow& b) {
  return a.visit_id == b.visit_id && a.url_id == b.url_id &&
         a.visit_time == b.visit_time &&
         a.referring_visit == b.referring_visit &&
         ui::PageTransitionTypeIncludingQualifiersIs(a.transition,
                                                     b.transition);
}

}  // namespace

class VisitDatabaseTest : public PlatformTest,
                          public URLDatabase,
                          public VisitDatabase {
 public:
  VisitDatabaseTest() {}

 private:
  // Test setup.
  void SetUp() override {
    PlatformTest::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file = temp_dir_.GetPath().AppendASCII("VisitTest.db");

    EXPECT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    CreateURLTable(false);
    CreateMainURLIndex();
    InitVisitTable();
  }
  void TearDown() override {
    db_.Close();
    PlatformTest::TearDown();
  }

  // Provided for URL/VisitDatabase.
  sql::Database& GetDB() override { return db_; }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};

TEST_F(VisitDatabaseTest, Add) {
  // Add one visit.
  VisitRow visit_info1(1, Time::Now(), 0, ui::PAGE_TRANSITION_LINK, 0, false);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  // Add second visit for the same page.
  VisitRow visit_info2(visit_info1.url_id,
                       visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
                       ui::PAGE_TRANSITION_TYPED, 0, true);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  // Add third visit for a different page.
  VisitRow visit_info3(2, visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
                       ui::PAGE_TRANSITION_LINK, 0, false);
  EXPECT_TRUE(AddVisit(&visit_info3, SOURCE_BROWSED));

  // Query the first two.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());

  // Make sure we got both (order in result set is visit time).
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2));
}

TEST_F(VisitDatabaseTest, Delete) {
  // Add three visits that form a chain of navigation, and then delete the
  // middle one. We should be left with the outer two visits, and the chain
  // should link them.
  static const int kTime1 = 1000;
  VisitRow visit_info1(1, Time::FromInternalValue(kTime1), 0,
                       ui::PAGE_TRANSITION_LINK, 0, false);
  EXPECT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  static const int kTime2 = kTime1 + 1;
  VisitRow visit_info2(1, Time::FromInternalValue(kTime2), visit_info1.visit_id,
                       ui::PAGE_TRANSITION_LINK, 0, false);
  EXPECT_TRUE(AddVisit(&visit_info2, SOURCE_BROWSED));

  static const int kTime3 = kTime2 + 1;
  VisitRow visit_info3(1, Time::FromInternalValue(kTime3), visit_info2.visit_id,
                       ui::PAGE_TRANSITION_LINK, 0, false);
  EXPECT_TRUE(AddVisit(&visit_info3, SOURCE_BROWSED));

  // First make sure all the visits are there.
  std::vector<VisitRow> matches;
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(3), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info2) &&
              IsVisitInfoEqual(matches[2], visit_info3));

  // Delete the middle one.
  DeleteVisit(visit_info2);

  // The outer two should be left, and the last one should have the first as
  // the referrer.
  visit_info3.referring_visit = visit_info1.visit_id;
  matches.clear();
  EXPECT_TRUE(GetVisitsForURL(visit_info1.url_id, &matches));
  EXPECT_EQ(static_cast<size_t>(2), matches.size());
  EXPECT_TRUE(IsVisitInfoEqual(matches[0], visit_info1) &&
              IsVisitInfoEqual(matches[1], visit_info3));
}

TEST_F(VisitDatabaseTest, Update) {
  // Make something in the database.
  VisitRow original(1, Time::Now(), 23, ui::PageTransitionFromInt(0), 19,
                    false);
  AddVisit(&original, SOURCE_BROWSED);

  // Mutate that row.
  VisitRow modification(original);
  modification.url_id = 2;
  modification.transition = ui::PAGE_TRANSITION_TYPED;
  modification.visit_time = Time::Now() + TimeDelta::FromDays(1);
  modification.referring_visit = 9292;
  UpdateVisitRow(modification);

  // Check that the mutated version was written.
  VisitRow final;
  GetRowForVisit(original.visit_id, &final);
  EXPECT_TRUE(IsVisitInfoEqual(modification, final));
}

// TODO(brettw) write test for GetMostRecentVisitForURL!

namespace {

std::vector<VisitRow> GetTestVisitRows() {
  // Tests can be sensitive to the local timezone, so use a local time as the
  // basis for all visit times.
  base::Time base_time = Time::UnixEpoch().LocalMidnight();

  // Add one visit.
  VisitRow visit_info1(
      1, base_time + TimeDelta::FromMinutes(1), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false);
  visit_info1.visit_id = 1;

  // Add second visit for the same page.
  VisitRow visit_info2(
      visit_info1.url_id, visit_info1.visit_time + TimeDelta::FromSeconds(1), 1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true);
  visit_info2.visit_id = 2;

  // Add third visit for a different page.
  VisitRow visit_info3(
      2, visit_info1.visit_time + TimeDelta::FromSeconds(2), 0,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_LINK |
                                ui::PAGE_TRANSITION_CHAIN_START),
      0, false);
  visit_info3.visit_id = 3;

  // Add a redirect visit from the last page.
  VisitRow visit_info4(
      3, visit_info1.visit_time + TimeDelta::FromSeconds(3),
      visit_info3.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_SERVER_REDIRECT |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false);
  visit_info4.visit_id = 4;

  // Add a subframe visit.
  VisitRow visit_info5(
      4, visit_info1.visit_time + TimeDelta::FromSeconds(4),
      visit_info4.visit_id,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_SUBFRAME |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, false);
  visit_info5.visit_id = 5;

  // Add third visit for the same URL as visit 1 and 2, but exactly a day
  // later than visit 2.
  VisitRow visit_info6(
      visit_info1.url_id, visit_info2.visit_time + TimeDelta::FromDays(1), 1,
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_CHAIN_START |
                                ui::PAGE_TRANSITION_CHAIN_END),
      0, true);
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
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
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
    ASSERT_EQ(static_cast<size_t>(1), results.size());
    EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[i]));
  }
}

TEST_F(VisitDatabaseTest, GetAllVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for all time.  We should get all visits.
  VisitVector results;
  GetAllVisitsInRange(Time(), Time(), 0, &results);
  ASSERT_EQ(test_visit_rows.size(), results.size());
  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(IsVisitInfoEqual(results[i], test_visit_rows[i]));
  }

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  GetAllVisitsInRange(test_visit_rows[1].visit_time,
                      test_visit_rows[3].visit_time, 0, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[1]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[2]));

  // Query for a max count and make sure we get only that number.
  GetAllVisitsInRange(Time(), Time(), 1, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[0]));
}

TEST_F(VisitDatabaseTest, GetVisibleVisitsInRange) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for all time.  We should not get the first or the second
  // visit (duplicates of the sixth) or the redirect or subframe visits.
  VisitVector results;
  QueryOptions options;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[3]));

  // Now try with only per-day de-duping -- the second visit should appear,
  // since it's a duplicate of visit6 but on a different day.
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(3), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[3]));
  EXPECT_TRUE(IsVisitInfoEqual(results[2], test_visit_rows[1]));

  // Now try without de-duping, expect to see all visible visits.
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(4), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[3]));
  EXPECT_TRUE(IsVisitInfoEqual(results[2], test_visit_rows[1]));
  EXPECT_TRUE(IsVisitInfoEqual(results[3], test_visit_rows[0]));

  // Set the end time to exclude the second visit. The first visit should be
  // returned. Even though the second is a more recent visit, it's not in the
  // query range.
  options.end_time = test_visit_rows[1].visit_time;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[0]));

  options = QueryOptions();  // Reset to options to default.

  // Query for a max count and make sure we get only that number.
  options.max_count = 1;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));

  // Query a time range and make sure beginning is inclusive and ending is
  // exclusive.
  options.begin_time = test_visit_rows[1].visit_time;
  options.end_time = test_visit_rows[3].visit_time;
  options.max_count = 0;
  GetVisibleVisitsInRange(options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[1]));
}

TEST_F(VisitDatabaseTest, GetAllURLIDsForTransition) {
  std::vector<VisitRow> test_visit_rows = GetTestVisitRows();

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }
  std::vector<URLID> url_ids;
  GetAllURLIDsForTransition(ui::PAGE_TRANSITION_TYPED, &url_ids);
  EXPECT_EQ(1U, url_ids.size());
  EXPECT_EQ(test_visit_rows[0].url_id, url_ids[0]);
}

TEST_F(VisitDatabaseTest, VisitSource) {
  // Add visits.
  VisitRow visit_info1(111, Time::Now(), 0, ui::PAGE_TRANSITION_LINK, 0, false);
  ASSERT_TRUE(AddVisit(&visit_info1, SOURCE_BROWSED));

  VisitRow visit_info2(112, Time::Now(), 1, ui::PAGE_TRANSITION_TYPED, 0, true);
  ASSERT_TRUE(AddVisit(&visit_info2, SOURCE_SYNCED));

  VisitRow visit_info3(113, Time::Now(), 0, ui::PAGE_TRANSITION_TYPED, 0, true);
  ASSERT_TRUE(AddVisit(&visit_info3, SOURCE_EXTENSION));

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

  for (size_t i = 0; i < test_visit_rows.size(); ++i) {
    EXPECT_TRUE(AddVisit(&test_visit_rows[i], SOURCE_BROWSED));
  }

  // Query the visits for the first url id.  We should not get the first or the
  // second visit (duplicates of the sixth) or any other urls, redirects or
  // subframe visits.
  VisitVector results;
  QueryOptions options;
  int url_id = test_visit_rows[0].url_id;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(static_cast<size_t>(1), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));

  // Now try with only per-day de-duping -- the second visit should appear,
  // since it's a duplicate of visit6 but on a different day.
  options.duplicate_policy = QueryOptions::REMOVE_DUPLICATES_PER_DAY;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(static_cast<size_t>(2), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[1]));

  // Now try without de-duping, expect to see all visible visits to url id 1.
  options.duplicate_policy = QueryOptions::KEEP_ALL_DUPLICATES;
  GetVisibleVisitsForURL(url_id, options, &results);
  ASSERT_EQ(static_cast<size_t>(3), results.size());
  EXPECT_TRUE(IsVisitInfoEqual(results[0], test_visit_rows[5]));
  EXPECT_TRUE(IsVisitInfoEqual(results[1], test_visit_rows[1]));
  EXPECT_TRUE(IsVisitInfoEqual(results[2], test_visit_rows[0]));
}

TEST_F(VisitDatabaseTest, GetHistoryCount) {
  // Start with a day in the middle of summer, so that we are nowhere near
  // DST shifts.
  Time today;
  ASSERT_TRUE(Time::FromString("2015-07-07", &today));
  Time yesterday = today - TimeDelta::FromDays(1);
  Time two_days_ago = yesterday - TimeDelta::FromDays(1);
  Time now = two_days_ago;

  ui::PageTransition standard_transition = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_CHAIN_START |
      ui::PAGE_TRANSITION_CHAIN_END);

  // Add 5 visits (3 distinct URLs) for the day before yesterday.
  // Whether the URL was browsed on this machine or synced has no effect.
  VisitRow first_day_1(1, now, 0, standard_transition, 0, true);
  first_day_1.visit_id = 1;
  AddVisit(&first_day_1, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  VisitRow first_day_2(2, now, 0, standard_transition, 0, true);
  first_day_2.visit_id = 2;
  AddVisit(&first_day_2, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  VisitRow first_day_3(1, now, 0, standard_transition, 0, true);
  first_day_3.visit_id = 3;
  AddVisit(&first_day_3, SOURCE_SYNCED);
  now += TimeDelta::FromHours(1);

  VisitRow first_day_4(3, now, 0, standard_transition, 0, true);
  first_day_4.visit_id = 4;
  AddVisit(&first_day_4, SOURCE_SYNCED);
  now += TimeDelta::FromHours(1);

  VisitRow first_day_5(2, now, 0, standard_transition, 0, true);
  first_day_5.visit_id = 5;
  AddVisit(&first_day_5, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  // Add 4 more visits for yesterday. One of them is invalid, as it's not
  // a user-visible navigation. Of the remaining 3, only 2 are unique.
  now = yesterday;

  VisitRow second_day_1(1, now, 0, standard_transition, 0, true);
  second_day_1.visit_id = 6;
  AddVisit(&second_day_1, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  VisitRow second_day_2(1, now, 0, standard_transition, 0, true);
  second_day_2.visit_id = 7;
  AddVisit(&second_day_2, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  VisitRow second_day_3(2, now, 0, ui::PAGE_TRANSITION_AUTO_SUBFRAME, 0, false);
  second_day_3.visit_id = 8;
  AddVisit(&second_day_3, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  VisitRow second_day_4(3, now, 0, standard_transition, 0, true);
  second_day_4.visit_id = 9;
  AddVisit(&second_day_4, SOURCE_BROWSED);
  now += TimeDelta::FromHours(1);

  int result;

  // There were 3 distinct URLs two days ago.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, yesterday, &result));
  EXPECT_EQ(3, result);

  // For both previous days, there should be 5 per-day unique URLs.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, today, &result));
  EXPECT_EQ(5, result);

  // Since we only have entries for the two previous days, the infinite time
  // range should yield the same result.
  EXPECT_TRUE(GetHistoryCount(Time(), Time::Max(), &result));
  EXPECT_EQ(5, result);

  // Narrowing the range to exclude |first_day_1| will still return 5,
  // because |first_day_1| is not unique.
  EXPECT_TRUE(
      GetHistoryCount(two_days_ago + TimeDelta::FromHours(2), today, &result));
  EXPECT_EQ(5, result);

  // Narrowing the range to exclude |second_day_4| will return 4,
  // because |second_day_4| is unique.
  EXPECT_TRUE(GetHistoryCount(two_days_ago, yesterday + TimeDelta::FromHours(3),
                              &result));
  EXPECT_EQ(4, result);

  // Narrowing the range to exclude both |first_day_1| and |second_day_4| will
  // still return 4.
  EXPECT_TRUE(GetHistoryCount(two_days_ago + TimeDelta::FromHours(2),
                              yesterday + TimeDelta::FromHours(3), &result));
  EXPECT_EQ(4, result);

  // A range that contains no visits will return 0.
  EXPECT_TRUE(GetHistoryCount(two_days_ago + TimeDelta::FromMicroseconds(1),
                              two_days_ago + TimeDelta::FromHours(1), &result));
  EXPECT_EQ(0, result);

  // If this timezone uses DST, test the behavior on days when the time
  // is shifted forward and backward. Note that in this case we cannot use
  // TimeDelta::FromDays(1) to move one day, as this simply removes 24 hours and
  // thus does not work correctly with DST shifts. Instead, we'll go back
  // 1 second (i.e. somewhere in the middle of the previous day), and use
  // |LocalMidnight()| to round down to the beginning of the day in the local
  // time, taking timezones and DST into account. This is necessary to achieve
  // the same equivalence class on days as the DATE(..., 'localtime') function
  // in SQL.
  Time shift_forward;
  Time shift_backward;
  Time current_day = (two_days_ago - TimeDelta::FromSeconds(1)).LocalMidnight();
  for (int i = 0; i < 366; i++) {
    current_day = (current_day - TimeDelta::FromSeconds(1)).LocalMidnight();
    Time after_24_hours = current_day + TimeDelta::FromHours(24);

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
    VisitRow backward_1(1, shift_backward, 0, standard_transition, 0, true);
    backward_1.visit_id = 10;
    AddVisit(&backward_1, SOURCE_BROWSED);

    VisitRow backward_2(1, shift_backward + TimeDelta::FromHours(24), 0,
                        standard_transition, 0, true);
    backward_2.visit_id = 11;
    AddVisit(&backward_2, SOURCE_BROWSED);

    EXPECT_TRUE(GetHistoryCount(
        shift_backward, shift_backward + TimeDelta::FromHours(25), &result));
    EXPECT_EQ(1, result);
  }

  // Test the forward shift. Add two visits for the same page at midnight and
  // almost 24 hours later. The count should be 2, not 1. The visits would be
  // regarded as duplicate in a normal 24 hour day, but in this case the second
  // visit is already in the next day.
  if (!shift_forward.is_null()) {
    VisitRow forward_1(1, shift_forward, 0, standard_transition, 0, true);
    forward_1.visit_id = 12;
    AddVisit(&forward_1, SOURCE_BROWSED);

    Time almost_24_hours_later = shift_forward + TimeDelta::FromHours(24) -
                                 TimeDelta::FromMicroseconds(1);
    VisitRow forward_2(1, almost_24_hours_later, 0, standard_transition, 0,
                       true);
    forward_2.visit_id = 13;
    AddVisit(&forward_2, SOURCE_BROWSED);

    EXPECT_TRUE(GetHistoryCount(
        shift_forward, shift_forward + TimeDelta::FromHours(24), &result));
    EXPECT_EQ(2, result);
  }
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_BadURL) {
  base::Time last_visit;
  EXPECT_FALSE(GetLastVisitToHost(GURL(), base::Time::Min(), base::Time::Max(),
                                  &last_visit));
  EXPECT_EQ(last_visit, base::Time());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_NonHttpURL) {
  base::Time last_visit;
  EXPECT_FALSE(GetLastVisitToHost(GURL("ftp://host/"), base::Time::Min(),
                                  base::Time::Max(), &last_visit));
  EXPECT_EQ(last_visit, base::Time());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_NoVisits) {
  base::Time last_visit;
  EXPECT_TRUE(GetLastVisitToHost(GURL("https://www.chromium.org"),
                                 base::Time::Min(), base::Time::Max(),
                                 &last_visit));
  EXPECT_EQ(last_visit, base::Time());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_VisitsOutsideRange) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::TimeDelta::FromHours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time - base::TimeDelta::FromHours(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row1, SOURCE_BROWSED);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org"))),
                end_time + base::TimeDelta::FromHours(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row2, SOURCE_BROWSED);

  base::Time last_visit;
  EXPECT_TRUE(GetLastVisitToHost(GURL("https://www.chromium.org"), begin_time,
                                 end_time, &last_visit));
  EXPECT_EQ(last_visit, base::Time());
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_EndTimeNotIncluded) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::TimeDelta::FromHours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://www.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row1, SOURCE_BROWSED);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org"))),
                end_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row2, SOURCE_BROWSED);

  base::Time last_visit;
  EXPECT_TRUE(GetLastVisitToHost(GURL("https://www.chromium.org"), begin_time,
                                 end_time, &last_visit));
  EXPECT_EQ(last_visit, begin_time);
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_SameOriginOnly) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::TimeDelta::FromHours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://other.origin.chromium.org"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row1, SOURCE_BROWSED);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org/path?query=foo"))),
                begin_time + base::TimeDelta::FromMinutes(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row2, SOURCE_BROWSED);

  base::Time last_visit;
  EXPECT_TRUE(GetLastVisitToHost(GURL("https://www.chromium.org"), begin_time,
                                 end_time, &last_visit));
  EXPECT_EQ(last_visit, begin_time + base::TimeDelta::FromMinutes(1));
}

TEST_F(VisitDatabaseTest, GetLastVisitToHost_MostRecentVisitTime) {
  base::Time begin_time = base::Time::Now();
  base::Time end_time = begin_time + base::TimeDelta::FromHours(1);

  VisitRow row1{AddURL(URLRow(GURL("https://chromium.org/"))),
                begin_time,
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row1, SOURCE_BROWSED);
  VisitRow row2{AddURL(URLRow(GURL("https://www.chromium.org/"))),
                begin_time + base::TimeDelta::FromMinutes(1),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row2, SOURCE_BROWSED);
  VisitRow row3{AddURL(URLRow(GURL("https://www.chromium.org/"))),
                begin_time + base::TimeDelta::FromMinutes(2),
                0,
                ui::PageTransitionFromInt(0),
                0,
                false};
  AddVisit(&row3, SOURCE_BROWSED);

  base::Time last_visit;
  EXPECT_TRUE(GetLastVisitToHost(GURL("https://www.chromium.org"), begin_time,
                                 end_time, &last_visit));
  EXPECT_EQ(last_visit, begin_time + base::TimeDelta::FromMinutes(2));
}

TEST_F(VisitDatabaseTest, GetGoogleDomainVisitsFromSearchesInRange_NoVisits) {
  const auto begin_time = base::Time::Now();
  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::TimeDelta::FromDays(1)),
              IsEmpty());
}

TEST_F(VisitDatabaseTest,
       GetGoogleDomainVisitsFromSearchesInRange_TwoVistsInRange) {
  const auto begin_time = base::Time::Now();
  // Out of range, one hour before begin time.
  VisitRow row{AddURL(URLRow(GURL("https://www.google.fr/search?q=foo"))),
               begin_time + base::TimeDelta::FromHours(-1),
               0,
               ui::PageTransitionFromInt(0),
               0,
               false};
  AddVisit(&row, SOURCE_BROWSED);
  // In range, exactly begin time.
  row = {AddURL(URLRow(GURL("https://www.google.com/search?q=foo"))),
         begin_time,
         0,
         ui::PageTransitionFromInt(0),
         0,
         false};
  AddVisit(&row, SOURCE_BROWSED);
  // In range, 23 hours after begin time.
  row = {AddURL(URLRow(GURL("https://www.google.ch/search?q=foo"))),
         begin_time + base::TimeDelta::FromHours(23),
         0,
         ui::PageTransitionFromInt(0),
         0,
         false};
  AddVisit(&row, SOURCE_BROWSED);
  // Out of range, exactly a day after begin time.
  row = {AddURL(URLRow(GURL("https://www.google.de/search?q=foo"))),
         begin_time + base::TimeDelta::FromHours(24),
         0,
         ui::PageTransitionFromInt(0),
         0,
         false};
  AddVisit(&row, SOURCE_BROWSED);

  EXPECT_THAT(
      GetGoogleDomainVisitsFromSearchesInRange(
          begin_time, begin_time + base::TimeDelta::FromDays(1)),
      ElementsAre(
          AllOf(Property(&DomainVisit::domain, "www.google.com"),
                Property(&DomainVisit::visit_time, begin_time)),
          AllOf(Property(&DomainVisit::domain, "www.google.ch"),
                Property(&DomainVisit::visit_time,
                         begin_time + base::TimeDelta::FromHours(23)))));
}

TEST_F(VisitDatabaseTest, GetGoogleDomainVisitsFromSearchesInRange_NotSearch) {
  const auto begin_time = base::Time::Now();
  VisitRow row{AddURL(URLRow(GURL("https://www.google.fr/searchin"))),
               begin_time,
               0,
               ui::PageTransitionFromInt(0),
               0,
               false};
  AddVisit(&row, SOURCE_BROWSED);

  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::TimeDelta::FromDays(1)),
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
               false};
  AddVisit(&row, SOURCE_BROWSED);

  EXPECT_THAT(GetGoogleDomainVisitsFromSearchesInRange(
                  begin_time, begin_time + base::TimeDelta::FromDays(1)),
              IsEmpty());
}

}  // namespace history
