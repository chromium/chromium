// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>

#include "base/strings/utf_string_conversions.h"
#include "components/history/core/browser/history_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

// Validates the consistency of the given history result. We just make sure
// that the URL rows match the indices structure. The unit tests themselves
// test the index structure to verify things are in the right order, so we
// don't need to.
void CheckHistoryResultConsistency(const QueryResults& result) {
  for (size_t i = 0; i < result.size(); i++) {
    size_t match_count;
    const size_t* matches = result.MatchesForURL(result[i].url(), &match_count);

    bool found = false;
    for (size_t match = 0; match < match_count; match++) {
      if (matches[match] == i) {
        found = true;
        break;
      }
    }

    EXPECT_TRUE(found) << "The URL had no index referring to it.";
  }
}

const char kURL1[] = "http://www.google.com/";
const char kURL2[] = "http://news.google.com/";

// Adds kURL1 twice and kURL2 once.
void AddSimpleData(QueryResults* results) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  std::vector<URLResult> test_vector;

  test_vector.push_back(URLResult(url1, base::Time::Now()));
  test_vector.push_back(URLResult(url1, base::Time::Now()));
  test_vector.push_back(URLResult(url2, base::Time::Now()));

  // The URLResults are invalid after being inserted.
  results->SetURLResults(std::move(test_vector));
  CheckHistoryResultConsistency(*results);
}

}  // namespace

// Tests insertion and deletion by range.
TEST(HistoryQueryResult, DeleteRange) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  QueryResults results;
  AddSimpleData(&results);

  // Make sure the first URL is in there twice. The indices can be in either
  // order.
  size_t match_count;
  const size_t* matches = results.MatchesForURL(url1, &match_count);
  ASSERT_EQ(2U, match_count);
  EXPECT_TRUE((matches[0] == 0 && matches[1] == 1) ||
              (matches[0] == 1 && matches[1] == 0));

  // Check the second one.
  matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 2);

  // Delete the first instance of the first URL.
  results.DeleteRange(0, 0);
  CheckHistoryResultConsistency(results);

  // Check the two URLs.
  matches = results.MatchesForURL(url1, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 0);
  matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 1);

  // Now delete everything and make sure it's deleted.
  results.DeleteRange(0, 1);
  EXPECT_EQ(0U, results.size());
  EXPECT_FALSE(results.MatchesForURL(url1, nullptr));
  EXPECT_FALSE(results.MatchesForURL(url2, nullptr));
}

// Tests insertion and deletion by URL.
TEST(HistoryQueryResult, ResultDeleteURL) {
  GURL url1(kURL1);
  GURL url2(kURL2);
  QueryResults results;
  AddSimpleData(&results);

  // Delete the first URL.
  results.DeleteURL(url1);
  CheckHistoryResultConsistency(results);
  EXPECT_EQ(1U, results.size());

  // The first one should be gone, and the second one should be at [0].
  size_t match_count;
  EXPECT_FALSE(results.MatchesForURL(url1, nullptr));
  const size_t* matches = results.MatchesForURL(url2, &match_count);
  ASSERT_EQ(1U, match_count);
  EXPECT_TRUE(matches[0] == 0);

  // Delete the second URL, there should be nothing left.
  results.DeleteURL(url2);
  EXPECT_EQ(0U, results.size());
  EXPECT_FALSE(results.MatchesForURL(url2, nullptr));
}

// Tests time ranges.
TEST(HistoryTypes, DeletionTimeRange) {
  auto invalid = DeletionTimeRange::Invalid();
  EXPECT_FALSE(invalid.IsValid());
  EXPECT_FALSE(invalid.IsAllTime());

  auto some_hours =
      DeletionTimeRange(base::Time::Now() - base::Hours(1), base::Time::Now());
  EXPECT_TRUE(some_hours.IsValid());
  EXPECT_FALSE(some_hours.IsAllTime());

  auto all_time = DeletionTimeRange::AllTime();
  EXPECT_TRUE(all_time.IsValid());
  EXPECT_TRUE(all_time.IsAllTime());

  auto another_all_time = DeletionTimeRange(base::Time(), base::Time());
  EXPECT_TRUE(another_all_time.IsValid());
  EXPECT_TRUE(another_all_time.IsAllTime());
}

}  // namespace history
