// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/segment_scorer.h"

#include <limits.h>
#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

// Segments scores are float values. Direct value comparison is finicky, and can
// are prone churn while tweaking. So instead of checking scores, we create
// named segment usage scenarios, score them, rank them, and check the resulting
// ranking by name.
class SegmentScorerTest : public ::testing::Test {
 private:
  struct SegmentTestItem {
    // |daily_visit_counts| represents successive visit counts from 0, 1, 2, ...
    // days ago. Non-zero counts populate |time_slots| and |visit_counts|.
    SegmentTestItem(base::Time midnight,
                    const std::string& name_in,
                    const std::vector<int>& daily_visit_counts)
        : name(name_in) {
      int n = static_cast<int>(daily_visit_counts.size());
      for (int days_ago = 0; days_ago < n; ++days_ago) {
        int visit_count = daily_visit_counts[days_ago];
        if (visit_count > 0) {
          time_slots.push_back(midnight - base::Days(days_ago));
          visit_counts.push_back(visit_count);
        }
      }
    }

    const std::string name;
    std::vector<base::Time> time_slots;
    std::vector<int> visit_counts;
    float score = 0.0f;
  };

 protected:
  SegmentScorerTest() {
    EXPECT_TRUE(base::Time::FromString("1 Sep 2024 10:00 GMT", &fake_now));
    fake_midnight = fake_now.UTCMidnight();
  }

  std::vector<SegmentTestItem> MakeTestItems() {
    base::Time m = fake_midnight;
    return {{m, "1 today", {1}},
            {m, "10 today", {10}},
            {m, "10 last 2 days", {5, 5}},
            {m, "10 last 5 days", {2, 2, 2, 2, 2}},
            {m, "10 last 10 days", {1, 1, 1, 1, 1, 1, 1, 1, 1, 1}},
            {m,
             "10 last 19 days",
             {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1}},
            {m, "10 decrease last 4 days", {1, 2, 3, 4}},
            {m, "10 increase last 4 days", {4, 3, 2, 1}},
            {m, "10 over 2 days week apart", {5, 0, 0, 0, 0, 0, 0, 5}}};
  }

  std::vector<std::string> GetTestDataRankingUsingScorer(
      std::vector<SegmentTestItem>&& test_items,
      std::unique_ptr<SegmentScorer> scorer) {
    for (auto& item : test_items) {
      item.score =
          scorer->Compute(item.time_slots, item.visit_counts, fake_now);
    }

    // Sort to make items with highest score appear first.
    size_t n = test_items.size();
    std::vector<size_t> order(n);
    for (size_t i = 0; i < n; ++i) {
      order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
      return test_items[a].score > test_items[b].score;
    });

    // Translate to names and return.
    std::vector<std::string> ret(n);
    for (size_t i = 0; i < n; ++i) {
      ret[i] = test_items[order[i]].name;
    }
    return ret;
  }

  // Fake current time for testing.
  base::Time fake_now;
  // Midnight for |fake_now|, to assign |time_slots| values.
  base::Time fake_midnight;
};

}  // namespace

TEST_F(SegmentScorerTest, RankByDefaultScorer) {
  std::vector<std::string> names_by_rank = GetTestDataRankingUsingScorer(
      MakeTestItems(), base::WrapUnique(new SegmentScorer(
                           kMvtScoringParamRecencyFactor_Default, INT_MAX)));
  int cur = 0;
  ASSERT_EQ("10 last 10 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 5 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 19 days", names_by_rank[cur++]);
  ASSERT_EQ("10 increase last 4 days", names_by_rank[cur++]);
  ASSERT_EQ("10 decrease last 4 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 2 days", names_by_rank[cur++]);
  ASSERT_EQ("10 over 2 days week apart", names_by_rank[cur++]);
  ASSERT_EQ("10 today", names_by_rank[cur++]);
  ASSERT_EQ("1 today", names_by_rank[cur++]);
}

TEST_F(SegmentScorerTest, RankByDecayStaircaseCap10Scorer) {
  std::vector<std::string> names_by_rank = GetTestDataRankingUsingScorer(
      MakeTestItems(), base::WrapUnique(new SegmentScorer(
                           kMvtScoringParamRecencyFactor_DecayStaircase, 10)));
  int cur = 0;
  ASSERT_EQ("10 last 10 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 5 days", names_by_rank[cur++]);
  ASSERT_EQ("10 increase last 4 days", names_by_rank[cur++]);
  ASSERT_EQ("10 decrease last 4 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 19 days", names_by_rank[cur++]);
  ASSERT_EQ("10 last 2 days", names_by_rank[cur++]);
  ASSERT_EQ("10 over 2 days week apart", names_by_rank[cur++]);
  ASSERT_EQ("10 today", names_by_rank[cur++]);
  ASSERT_EQ("1 today", names_by_rank[cur++]);
}

}  // namespace history
