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

  void AddVisitWithVisitTime(base::Time visit_time) {
    VisitRow visit_row;
    visit_row.visit_time = visit_time;
    AddVisit(&visit_row, VisitSource::SOURCE_BROWSED);
  }

 private:
  // Provided for URL/VisitDatabase.
  sql::Database& GetDB() override { return db_; }

  sql::Database db_;
};

TEST_F(ClusterVisitDatabaseTest, AddDeleteAndGet) {
  AddVisitWithVisitTime(IntToTime(20));
  AddVisitWithVisitTime(IntToTime(30));
  AddVisitWithVisitTime(IntToTime(10));

  AddClusterVisit({0, 1, 1, {true}});   // Ordered 2rd
  AddClusterVisit({0, 2, 2, {false}});  // Ordered 1st
  AddClusterVisit({0, 1, 1, {true}});   // Ordered 3nd
  AddClusterVisit({0, 2, 3, {false}});  // Ordered 4th

  std::vector<ClusterVisitRow> rows = GetClusterVisits(10);
  ASSERT_EQ(rows.size(), 4u);
  EXPECT_EQ(rows[0].cluster_visit_id, 2);
  EXPECT_EQ(rows[0].url_id, 2);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_signals.omnibox_url_copied);
  EXPECT_EQ(rows[1].cluster_visit_id, 1);
  EXPECT_EQ(rows[1].url_id, 1);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_signals.omnibox_url_copied);
  EXPECT_EQ(rows[2].cluster_visit_id, 3);
  EXPECT_EQ(rows[2].url_id, 1);
  EXPECT_EQ(rows[2].visit_id, 1);
  EXPECT_TRUE(rows[2].context_signals.omnibox_url_copied);
  EXPECT_EQ(rows[3].cluster_visit_id, 4);
  EXPECT_EQ(rows[3].url_id, 2);
  EXPECT_EQ(rows[3].visit_id, 3);
  EXPECT_FALSE(rows[3].context_signals.omnibox_url_copied);

  rows = GetClusterVisits(2);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].cluster_visit_id, 2);
  EXPECT_EQ(rows[0].url_id, 2);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_signals.omnibox_url_copied);
  EXPECT_EQ(rows[1].cluster_visit_id, 1);
  EXPECT_EQ(rows[1].url_id, 1);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_signals.omnibox_url_copied);

  DeleteClusterVisit(1);
  DeleteClusterVisit(3);

  rows = GetClusterVisits(10);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].cluster_visit_id, 2);
  EXPECT_EQ(rows[0].url_id, 2);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_signals.omnibox_url_copied);
  EXPECT_EQ(rows[1].cluster_visit_id, 4);
  EXPECT_EQ(rows[1].url_id, 2);
  EXPECT_EQ(rows[1].visit_id, 3);
  EXPECT_FALSE(rows[1].context_signals.omnibox_url_copied);

  rows = GetClusterVisits(1);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].cluster_visit_id, 2);
  EXPECT_EQ(rows[0].url_id, 2);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_signals.omnibox_url_copied);
}

}  // namespace history
