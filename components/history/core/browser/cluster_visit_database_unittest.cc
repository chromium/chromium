// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/cluster_visit_database.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/visit_database.h"
#include "sql/database.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Returns a Time that's `milliseconds` milliseconds after Windows epoch.
base::Time IntToTime(int milliseconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMilliseconds(milliseconds));
}

}  // namespace

namespace history {

class ClusterVisitDatabaseTest : public testing::Test,
                                 public ClusterVisitDatabase,
                                 public VisitDatabase {
 public:
  ClusterVisitDatabaseTest() {
    EXPECT_TRUE(db_.OpenInMemory());
    EXPECT_TRUE(InitVisitTable());
    EXPECT_TRUE(InitClusterVisitTable());
  }
  ~ClusterVisitDatabaseTest() override { db_.Close(); }

  void AddVisitWithDetails(URLID url_id, base::Time visit_time) {
    VisitRow visit_row;
    visit_row.url_id = url_id;
    visit_row.visit_time = visit_time;
    AddVisit(&visit_row, VisitSource::SOURCE_BROWSED);
  }

 private:
  // Provided for URL/VisitDatabase.
  sql::Database& GetDB() override { return db_; }

  sql::Database db_;
};

TEST_F(ClusterVisitDatabaseTest, AddDeleteAndGet) {
  AddVisitWithDetails(1, IntToTime(20));
  AddVisitWithDetails(1, IntToTime(30));
  AddVisitWithDetails(2, IntToTime(10));

  AddAnnotatedVisit({1, {true}, {}});   // Ordered 2nd
  AddAnnotatedVisit({2, {false}, {}});  // Ordered 1st
  AddAnnotatedVisit({3, {false}, {}});  // Ordered 3rd

  std::vector<AnnotatedVisitRow> rows = GetAnnotatedVisits(10);
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[2].visit_id, 3);
  EXPECT_FALSE(rows[2].context_annotations.omnibox_url_copied);

  rows = GetAnnotatedVisits(2);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_annotations.omnibox_url_copied);

  DeleteAnnotatedVisit(1);
  DeleteAnnotatedVisit(3);

  rows = GetAnnotatedVisits(10);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
}

}  // namespace history
