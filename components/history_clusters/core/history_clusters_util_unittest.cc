// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/history_clusters_util.h"

#include "base/strings/stringprintf.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

TEST(HistoryClustersUtilTest, ComputeURLForDeduping) {
  {
    Config config;
    config.use_host_for_visit_deduping = false;
    SetConfigForTesting(config);

    EXPECT_EQ(ComputeURLForDeduping(GURL("https://www.google.com/")),
              "https://google.com/")
        << "Strip off WWW.";
    EXPECT_EQ(ComputeURLForDeduping(GURL("http://google.com/")),
              "https://google.com/")
        << "Normalizes scheme to https.";
    EXPECT_EQ(
        ComputeURLForDeduping(GURL("https://google.com/path?foo=bar#reftag")),
        "https://google.com/path")
        << "Strips ref and query, leaves path.";
    EXPECT_EQ(ComputeURLForDeduping(
                  GURL("http://www.google.com/path?foo=bar#reftag")),
              "https://google.com/path")
        << "Does all of the above at once.";
    EXPECT_EQ(ComputeURLForDeduping(GURL("https://google.com/path")),
              "https://google.com/path")
        << "Sanity check when no replacements needed.";
  }

  {
    Config config;
    config.use_host_for_visit_deduping = true;
    SetConfigForTesting(config);

    EXPECT_EQ(ComputeURLForDeduping(
                  GURL("http://www.google.com/path?foo=bar#reftag")),
              "https://google.com/")
        << "Does all of the above at once.";

    EXPECT_EQ(ComputeURLForDeduping(GURL("https://google.com/path/")),
              "https://google.com/")
        << "Strips path.";
  }
}

TEST(HistoryClustersUtilTest, ComputeURLKeywordForLookup) {
  {
    Config config;
    config.use_host_for_visit_deduping = false;
    SetConfigForTesting(config);

    EXPECT_EQ(ComputeURLKeywordForLookup(GURL("http://www.google.com/")),
              "http://google.com/")
        << "Strip off WWW.";
    EXPECT_EQ(ComputeURLKeywordForLookup(GURL("https://google.com/")),
              "http://google.com/")
        << "Normalizes scheme to http.";
    EXPECT_EQ(ComputeURLKeywordForLookup(
                  GURL("http://google.com/path?foo=bar#reftag")),
              "http://google.com/path")
        << "Strips ref and query, leaves path.";
    EXPECT_EQ(ComputeURLKeywordForLookup(
                  GURL("https://www.google.com/path?foo=bar#reftag")),
              "http://google.com/path")
        << "Does all of the above at once.";
    EXPECT_EQ(ComputeURLKeywordForLookup(GURL("http://google.com/path")),
              "http://google.com/path")
        << "Sanity check when no replacements needed.";
  }

  {
    Config config;
    config.use_host_for_visit_deduping = true;
    SetConfigForTesting(config);

    EXPECT_EQ(ComputeURLKeywordForLookup(GURL("https://google.com/path/")),
              "http://google.com/")
        << "Strips path.";

    EXPECT_EQ(ComputeURLKeywordForLookup(
                  GURL("https://www.google.com/path?foo=bar#reftag")),
              "http://google.com/")
        << "Does everything at once.";
  }
}

TEST(HistoryClustersUtilTest, FilterClustersMatchingQuery) {
  std::vector<history::Cluster> all_clusters;
  all_clusters.push_back(
      history::Cluster(1,
                       {
                           GetHardcodedClusterVisit(2),
                           GetHardcodedClusterVisit(1),
                       },
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false,
                       /*label=*/u"LabelOne"));
  all_clusters.push_back(
      history::Cluster(2,
                       {
                           GetHardcodedClusterVisit(2),
                       },
                       {},
                       /*should_show_on_prominent_ui_surfaces=*/true,
                       /*label=*/u"LabelTwo"));

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
      // Verify that we can find clusters by label.
      {"labeltwo", false, true},
  };

  for (size_t i = 0; i < std::size(test_data); ++i) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s",
                                    static_cast<int>(i),
                                    test_data[i].query.c_str()));

    auto clusters = all_clusters;
    ApplySearchQuery(test_data[i].query, clusters);

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
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));

  // No promotion when we match a keyword.
  {
    std::vector clusters = all_clusters;
    ApplySearchQuery("apples", clusters);
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
    ApplySearchQuery("git", clusters);
    ASSERT_EQ(clusters.size(), 1U);
    ASSERT_EQ(clusters[0].visits.size(), 2U);
    EXPECT_EQ(clusters[0].visits[0].annotated_visit.visit_row.visit_id, 2);
    EXPECT_FLOAT_EQ(clusters[0].visits[0].score, 0.75);
    EXPECT_EQ(clusters[0].visits[1].annotated_visit.visit_row.visit_id, 1);
    EXPECT_FLOAT_EQ(clusters[0].visits[1].score, 0.25);
  }
}

TEST(HistoryClustersUtilTest, SortClustersWithinBatchForQuery) {
  std::vector<history::Cluster> all_clusters;
  all_clusters.push_back(
      history::Cluster(1,
                       {
                           GetHardcodedClusterVisit(1),
                           GetHardcodedClusterVisit(2),
                       },
                       {{u"apples", history::ClusterKeywordData()},
                        {u"Red Oranges", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));
  all_clusters.push_back(
      history::Cluster(2,
                       {
                           GetHardcodedClusterVisit(1),
                       },
                       {{u"search", history::ClusterKeywordData()}},
                       /*should_show_on_prominent_ui_surfaces=*/false));

  // When the flag is off, leave the initial ordering alone.
  {
    Config config;
    config.sort_clusters_within_batch_for_query = false;
    SetConfigForTesting(config);

    std::vector clusters = all_clusters;
    ApplySearchQuery("search", clusters);
    ASSERT_EQ(clusters.size(), 2U);
    EXPECT_EQ(clusters[0].cluster_id, 1);
    EXPECT_EQ(clusters[1].cluster_id, 2);
    EXPECT_FLOAT_EQ(clusters[0].search_match_score, 0.5);
    EXPECT_FLOAT_EQ(clusters[1].search_match_score, 3.5);
  }

  // When the flag is on, second cluster should be sorted above the first one,
  // because the second one has a match on both the keyword and visit.
  {
    Config config;
    config.sort_clusters_within_batch_for_query = true;
    SetConfigForTesting(config);

    std::vector clusters = all_clusters;
    ApplySearchQuery("search", clusters);
    ASSERT_EQ(clusters.size(), 2U);
    EXPECT_EQ(clusters[0].cluster_id, 2);
    EXPECT_EQ(clusters[1].cluster_id, 1);
    EXPECT_FLOAT_EQ(clusters[0].search_match_score, 3.5);
    EXPECT_FLOAT_EQ(clusters[1].search_match_score, 0.5);
  }

  // With flag on, if both scores are equal, the ordering should be preserved.
  {
    Config config;
    config.sort_clusters_within_batch_for_query = true;
    SetConfigForTesting(config);

    std::vector clusters = all_clusters;
    ApplySearchQuery("google", clusters);
    ASSERT_EQ(clusters.size(), 2U);
    EXPECT_EQ(clusters[0].cluster_id, 1);
    EXPECT_EQ(clusters[1].cluster_id, 2);
    EXPECT_FLOAT_EQ(clusters[0].search_match_score, 0.5);
    EXPECT_FLOAT_EQ(clusters[1].search_match_score, 0.5);
  }
}

TEST(HistoryClustersUtilTest, HideAndCullLowScoringVisits) {
  std::vector<history::Cluster> all_clusters;

  // High scoring visits should always be above the fold.
  history::Cluster cluster1;
  cluster1.cluster_id = 4;
  cluster1.visits.push_back(GetHardcodedClusterVisit(1, 1));
  cluster1.visits.push_back(GetHardcodedClusterVisit(1, .8));
  cluster1.visits.push_back(GetHardcodedClusterVisit(1, .5));
  cluster1.visits.push_back(GetHardcodedClusterVisit(1, .5));
  cluster1.visits.push_back(GetHardcodedClusterVisit(1, .5));
  cluster1.keyword_to_data_map = {{u"keyword", history::ClusterKeywordData()}};

  // Low scoring visits should be above the fold only if they're one of top 4.
  history::Cluster cluster2;
  cluster2.cluster_id = 6;
  cluster2.visits.push_back(GetHardcodedClusterVisit(1, .4));
  cluster2.visits.push_back(GetHardcodedClusterVisit(1, .4));
  cluster2.visits.push_back(GetHardcodedClusterVisit(1, .4));
  cluster2.visits.push_back(GetHardcodedClusterVisit(1, .4));
  cluster2.visits.push_back(GetHardcodedClusterVisit(1, .4));
  cluster2.keyword_to_data_map = {{u"keyword", history::ClusterKeywordData()}};

  // 0 scoring visits should be above the fold only if they're 1st.
  history::Cluster cluster3;
  cluster3.cluster_id = 8;
  cluster3.visits.push_back(GetHardcodedClusterVisit(1, 0.0));
  cluster3.visits.push_back(GetHardcodedClusterVisit(1, 0.0));
  cluster3.keyword_to_data_map = {{u"keyword", history::ClusterKeywordData()}};

  all_clusters.push_back(cluster1);
  all_clusters.push_back(cluster2);
  all_clusters.push_back(cluster3);

  {
    Config config;
    config.drop_hidden_visits = true;
    SetConfigForTesting(config);

    auto clusters = all_clusters;
    HideAndCullLowScoringVisits(clusters);

    EXPECT_EQ(clusters[0].cluster_id, 4);
    auto& visits = clusters[0].visits;
    ASSERT_EQ(visits.size(), 5u);
    EXPECT_EQ(visits[0].hidden, false);
    EXPECT_EQ(visits[1].hidden, false);
    EXPECT_EQ(visits[2].hidden, false);
    EXPECT_EQ(visits[3].hidden, false);
    EXPECT_EQ(visits[4].hidden, false);

    EXPECT_EQ(clusters[1].cluster_id, 6);
    visits = clusters[1].visits;
    ASSERT_EQ(visits.size(), 4u);
    EXPECT_EQ(visits[0].hidden, false);
    EXPECT_EQ(visits[1].hidden, false);
    EXPECT_EQ(visits[2].hidden, false);
    EXPECT_EQ(visits[3].hidden, false);

    EXPECT_EQ(clusters[2].cluster_id, 8);
    ASSERT_EQ(clusters[2].visits.size(), 1u);
    EXPECT_EQ(clusters[2].visits[0].hidden, false);
  }

  {
    Config config;
    config.drop_hidden_visits = false;
    SetConfigForTesting(config);

    auto clusters = all_clusters;
    HideAndCullLowScoringVisits(clusters);

    EXPECT_EQ(clusters[0].cluster_id, 4);
    EXPECT_EQ(clusters[0].visits.size(), 5u);

    EXPECT_EQ(clusters[1].cluster_id, 6);
    const auto& visits = clusters[1].visits;
    ASSERT_EQ(visits.size(), 5u);
    EXPECT_EQ(visits[0].hidden, false);
    EXPECT_EQ(visits[1].hidden, false);
    EXPECT_EQ(visits[2].hidden, false);
    EXPECT_EQ(visits[3].hidden, false);
    EXPECT_EQ(visits[4].hidden, true);

    EXPECT_EQ(clusters[2].cluster_id, 8);
    ASSERT_EQ(clusters[2].visits.size(), 2u);
    EXPECT_EQ(clusters[2].visits[0].hidden, false);
    EXPECT_EQ(clusters[2].visits[1].hidden, true);
  }
}

TEST(HistoryClustersUtilTest, CoalesceRelatedSearches) {
  // canonical_visit has the same URL as Visit1.
  history::ClusterVisit visit1 = GetHardcodedClusterVisit(1);
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search1");
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search2");
  visit1.annotated_visit.content_annotations.related_searches.push_back(
      "search3");

  history::ClusterVisit visit2 = GetHardcodedClusterVisit(2);
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search4");
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search5");
  visit2.annotated_visit.content_annotations.related_searches.push_back(
      "search6");

  history::Cluster cluster;
  cluster.visits = {visit1, visit2};
  std::vector<history::Cluster> clusters;
  clusters.push_back(cluster);

  CoalesceRelatedSearches(clusters);
  EXPECT_THAT(clusters[0].related_searches,
              testing::ElementsAre("search1", "search2", "search3", "search4",
                                   "search5"));
}

TEST(HistoryClustersUtilTest, SortClusters) {
  std::vector<history::Cluster> clusters;
  // This first cluster is meant to validate that the higher scoring "visit 1"
  // gets sorted to the top, even though "visit 1" is older than "visit 2".
  // It's to validate the within-cluster sorting.
  clusters.push_back(history::Cluster(0,
                                      {
                                          GetHardcodedClusterVisit(2, 0.5),
                                          GetHardcodedClusterVisit(1, 0.9),
                                      },
                                      {}));
  // The second cluster is lower scoring, but newer, because the top visit is
  // newer. It should be sorted above the first cluster because of reverse
  // chronological between-cluster sorting.
  clusters.push_back(history::Cluster(0,
                                      {
                                          GetHardcodedClusterVisit(3, 0.1),
                                      },
                                      {}));

  SortClusters(&clusters);

  ASSERT_EQ(clusters.size(), 2u);

  auto& visits = clusters[0].visits;
  ASSERT_EQ(visits.size(), 1u);
  EXPECT_FLOAT_EQ(visits[0].score, 0.1);

  visits = clusters[1].visits;
  ASSERT_EQ(visits.size(), 2u);
  EXPECT_FLOAT_EQ(visits[0].score, 0.9);
  EXPECT_FLOAT_EQ(visits[1].score, 0.5);
}

}  // namespace
}  // namespace history_clusters
