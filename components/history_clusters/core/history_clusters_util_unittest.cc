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

TEST(HistoryClustersUtilTest, ComputeURLForDeduping) {
  EXPECT_EQ(ComputeURLForDeduping(GURL("https://www.google.com/")),
            "https://google.com/")
      << "Strip off WWW.";
  EXPECT_EQ(ComputeURLForDeduping(GURL("http://google.com/")),
            "https://google.com/")
      << "Normalizes scheme to https.";
  EXPECT_EQ(
      ComputeURLForDeduping(GURL("https://google.com/path?foo=bar#reftag")),
      "https://google.com/path?foo=bar")
      << "Strips ref, leaves path and query.";
  EXPECT_EQ(
      ComputeURLForDeduping(GURL("http://www.google.com/path?foo=bar#reftag")),
      "https://google.com/path?foo=bar")
      << "Does all of the above at once.";
  EXPECT_EQ(ComputeURLForDeduping(GURL("https://google.com/path?foo=bar")),
            "https://google.com/path?foo=bar")
      << "Sanity check when no replacements needed.";
}

TEST(HistoryClustersUtilTest, FilterClustersMatchingQuery) {
  std::vector<history::Cluster> all_clusters;
  all_clusters.push_back(
      history::Cluster(1,
                       {
                           GetHardcodedClusterVisit(2),
                           GetHardcodedClusterVisit(1),
                       },
                       {u"apples", u"Red Oranges"},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  all_clusters.push_back(
      history::Cluster(2,
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
      // Empty query should get both clusters, even the non-prominent one,
      // because this function only filters for query, and ignores whether the
      // cluster is prominent or not.
      {"", true, true},
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
      // Verify that we match if the query spans the title and URL of a single
      // visit.
      {"goog search", true, false},
      // Verify that we DON'T match if the query spans the title and URL of a
      // multiple visits.
      {"goog code", false, false},
      // Verify that we DON'T match if the query spans both the visit and
      // keywords.
      {"goog red", false, false},
  };

  for (size_t i = 0; i < base::size(test_data); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s",
                                    static_cast<int>(i),
                                    test_data[i].query.c_str()));

    auto clusters = all_clusters;
    ApplySearchQuery(test_data[i].query, &clusters);

    size_t expected_size =
        static_cast<size_t>(test_data[i].expect_first_cluster) +
        static_cast<size_t>(test_data[i].expect_second_cluster);
    ASSERT_EQ(clusters.size(), expected_size);

    if (test_data[i].expect_first_cluster) {
      EXPECT_EQ(clusters[0].cluster_id, 1);
    }

    if (test_data[i].expect_second_cluster) {
      const auto& cluster =
          test_data[i].expect_first_cluster ? clusters[1] : clusters[0];
      EXPECT_EQ(cluster.cluster_id, 2);
    }
  }
}

TEST(HistoryClustersUtilTest, PromoteMatchingVisitsAboveNonMatchingVisits) {
  std::vector<history::Cluster> all_clusters;
  all_clusters.push_back(
      history::Cluster(0,
                       {
                           GetHardcodedClusterVisit(1),
                           GetHardcodedClusterVisit(2),
                       },
                       {u"apples", u"Red Oranges"},
                       /*should_show_on_prominent_ui_surfaces=*/false));

  // No promotion when we match a keyword.
  {
    std::vector clusters = all_clusters;
    ApplySearchQuery("apples", &clusters);
    ASSERT_EQ(clusters.size(), 1U);
    ASSERT_EQ(clusters[0].visits.size(), 2U);
    EXPECT_EQ(clusters[0].visits[0].annotated_visit.visit_row.visit_id, 1);
    EXPECT_FLOAT_EQ(clusters[0].visits[0].score, 0.5);
    EXPECT_EQ(clusters[0].visits[1].annotated_visit.visit_row.visit_id, 2);
    EXPECT_FLOAT_EQ(clusters[0].visits[1].score, 0.5);
  }

  // Promote the second visit over the first if we match the second visit.
  {
    std::vector clusters = all_clusters;
    ApplySearchQuery("git", &clusters);
    ASSERT_EQ(clusters.size(), 1U);
    ASSERT_EQ(clusters[0].visits.size(), 2U);
    EXPECT_EQ(clusters[0].visits[0].annotated_visit.visit_row.visit_id, 2);
    EXPECT_FLOAT_EQ(clusters[0].visits[0].score, 0.75);
    EXPECT_EQ(clusters[0].visits[1].annotated_visit.visit_row.visit_id, 1);
    EXPECT_FLOAT_EQ(clusters[0].visits[1].score, 0.25);
  }
}

}  // namespace
}  // namespace history_clusters
