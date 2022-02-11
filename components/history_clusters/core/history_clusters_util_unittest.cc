// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_util.h"

#include "base/strings/stringprintf.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

TEST(HistoryClustersUtilTest, FilterClustersMatchingQuery) {
  std::vector<history::Cluster> all_clusters;
  // This first cluster with keywords is marked hidden on sensitive UI
  // surfaces. This test thus verifies that it's hidden in the zero-query
  // state, but the user can still get to it by searching for its keywords.
  all_clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(2),
                           GetHardcodedClusterVisit(1),
                       },
                       {u"apples", u"Red Oranges"},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  all_clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(2),
                       },
                       {},
                       /*should_show_on_prominent_ui_surfaces=*/true));

  struct TestData {
    std::string query;
    const bool expect_first_cluster;
    const bool expect_second_cluster;
  } test_data[] = {
      // Empty query should get only the second, because the first is marked
      // hidden on prominent UI surfaces, including the zero query state.
      {"", false, true},
      // Non matching query should get none.
      {"non_matching_query", false, false},
      // Query matching one cluster.
      {"oran", true, false},
      // This verifies the memory doesn't flicker away as the user is typing
      // out: "red oran" one key at a time. Also tests out multi-term queries.
      {"red", true, false},
      {"red ", true, false},
      {"red o", true, false},
      {"red or", true, false},
      {"red ora", true, false},
      {"red oran", true, false},
      // Verify that we can search by URL.
      {"goog", true, false},
      // Verify we can search by page title, even mismatching case.
      {"code", true, true},
      // Verify that we match if the input query spans cluster keywords,
      // visit URLs, and visit titles.
      {"goog code apples", true, false},
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s",
                                    static_cast<int>(i),
                                    test_data[i].query.c_str()));

    auto clusters =
        FilterClustersMatchingQuery(test_data[i].query, all_clusters);

    size_t expected_size =
        static_cast<size_t>(test_data[i].expect_first_cluster) +
        static_cast<size_t>(test_data[i].expect_second_cluster);
    ASSERT_EQ(clusters.size(), expected_size);

    if (test_data[i].expect_first_cluster) {
      const auto& cluster = clusters[0];
      const auto& visits = cluster.visits;
      ASSERT_EQ(visits.size(), 2u);
      EXPECT_EQ(visits[0].annotated_visit.url_row.url(), "https://github.com/");
      EXPECT_EQ(visits[1].annotated_visit.url_row.url(), "https://google.com/");

      ASSERT_EQ(cluster.keywords.size(), 2u);
      EXPECT_EQ(cluster.keywords[0], u"apples");
      EXPECT_EQ(cluster.keywords[1], u"Red Oranges");
    }

    if (test_data[i].expect_second_cluster) {
      const auto& cluster =
          test_data[i].expect_first_cluster ? clusters[1] : clusters[0];
      const auto& visits = cluster.visits;
      ASSERT_EQ(visits.size(), 1u);
      EXPECT_EQ(visits[0].annotated_visit.url_row.url(), "https://github.com/");
      EXPECT_TRUE(cluster.keywords.empty());
    }
  }
}

}  // namespace
}  // namespace history_clusters
