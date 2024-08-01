// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/history_clusters/core/history_clusters_util.h"

#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_service_test_api.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;

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
  auto hidden_google_visit = GetHardcodedClusterVisit(1);
  hidden_google_visit.interaction_state =
      history::ClusterVisit::InteractionState::kHidden;
  all_clusters.push_back(
      history::Cluster(2,
                       {
                           GetHardcodedClusterVisit(2),
                           hidden_google_visit,
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
      // Verify that we can search by URL. Also double checks that we don't find
      // the hidden Google visit in the second cluster.
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
      // Verify that hidden visits are findable.
      {
          "HiddenVisitLabel",
      }};

  int i = 0;
  for (const auto& test_item : test_data) {
    SCOPED_TRACE(base::StringPrintf("Testing case i=%d, query=%s", i++,
                                    test_item.query.c_str()));

    auto clusters = all_clusters;
    ApplySearchQuery(test_item.query, clusters);

    size_t expected_size = static_cast<size_t>(test_item.expect_first_cluster) +
                           static_cast<size_t>(test_item.expect_second_cluster);
    ASSERT_EQ(clusters.size(), expected_size);

    if (test_item.expect_first_cluster) {
      EXPECT_EQ(clusters[0].cluster_id, 1);
    }

    if (test_item.expect_second_cluster) {
      const auto& cluster =
          test_item.expect_first_cluster ? clusters[1] : clusters[0];
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

TEST(HistoryClustersUtilTest, CullVisitsThatShouldBeHidden) {
  std::vector<history::Cluster> all_clusters;

  auto add_cluster = [&](int64_t cluster_id, std::vector<float> visit_scores) {
    history::Cluster cluster;
    cluster.cluster_id = cluster_id;
    base::ranges::transform(visit_scores, std::back_inserter(cluster.visits),
                            [&](const auto& visit_score) {
                              return GetHardcodedClusterVisit(1, visit_score);
                            });
    cluster.keyword_to_data_map = {{u"keyword", history::ClusterKeywordData()}};
    all_clusters.push_back(cluster);
  };

  // High scoring visits should always be above the fold.
  add_cluster(0, {1, .8, .5, .5, .5});

  // Low scoring visits should be above the fold only if they're one of top 4.
  add_cluster(1, {.4, .4, .4, .4, .4});

  // 0 scoring visits should never be above the fold.
  add_cluster(2, {0, 0, .8, .8});

  // Clusters with 1 visit after filtering should be removed.
  add_cluster(3, {.8, 0});

  // Clusters with 0 visits after filtering should be removed.
  add_cluster(4, {0, 0});

  // Hidden and Done visits can be culled out.
  add_cluster(5, {1, 1, 1, 1});
  all_clusters[5].visits[0].interaction_state =
      history::ClusterVisit::InteractionState::kHidden;
  all_clusters[5].visits[1].interaction_state =
      history::ClusterVisit::InteractionState::kDone;

  {
    // Test the zero-query state.
    base::HistogramTester histogram_tester;

    auto clusters = all_clusters;
    CullVisitsThatShouldBeHidden(clusters, /*is_zero_query_state=*/true);
    ASSERT_EQ(clusters.size(), 4u);

    histogram_tester.ExpectTotalCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 4);
    histogram_tester.ExpectTotalCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 4);

    // No visits are hidden as they are all high-scoring.
    EXPECT_EQ(clusters[0].cluster_id, 0);
    EXPECT_EQ(clusters[0].visits.size(), 5u);

    // 1 visit could have been shown but is below the fold due to score.
    EXPECT_EQ(clusters[1].cluster_id, 1);
    EXPECT_EQ(clusters[1].visits.size(), 4u);

    // No visits that should actually be shown are hidden.
    EXPECT_EQ(clusters[2].cluster_id, 2);
    EXPECT_EQ(clusters[2].visits.size(), 2u);

    // No visits that should actually be shown are hidden.
    EXPECT_EQ(clusters[3].cluster_id, 5);
    EXPECT_EQ(clusters[3].visits.size(), 2u);

    // Clusters index 0, 2, and 3.
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 0, 3);
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 0, 3);

    // Cluster index 1.
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 1, 1);
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 20, 1);
  }

  {
    base::HistogramTester histogram_tester;
    // Test the queried state with a higher threshold of required visits.
    auto clusters = all_clusters;
    CullVisitsThatShouldBeHidden(clusters, /*is_zero_query_state=*/false);

    histogram_tester.ExpectTotalCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 5);
    histogram_tester.ExpectTotalCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 5);

    // Cluster id = 3, with 1 visit after filtering should no longer be removed.
    ASSERT_EQ(clusters.size(), 5u);
    EXPECT_EQ(clusters[3].cluster_id, 3);
    EXPECT_EQ(clusters[3].visits.size(), 1u);
    // Cluster id = 5, with a Done visit, should have that Done visit visible.
    EXPECT_EQ(clusters[4].cluster_id, 5);
    EXPECT_EQ(clusters[4].visits.size(), 3u);

    // Clusters 0, 2, 3, and 4.
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 0, 4);
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 0, 4);

    // Cluster 1.
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFold", 1, 1);
    histogram_tester.ExpectBucketCount(
        "History.Clusters.Backend.NumVisitsBelowFoldPercentage", 20, 1);
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
  EXPECT_THAT(
      clusters[0].related_searches,
      ElementsAre("search1", "search2", "search3", "search4", "search5"));
}

// Verifies crbug.com/1426657.
TEST(HistoryClustersUtilTest,
     CoalesceRelatedSearchesHandlesMultipleClustersTruncation) {
  history::ClusterVisit visit = GetHardcodedClusterVisit(1);
  visit.annotated_visit.content_annotations.related_searches = {
      "search1", "search2", "search3", "search4",
      "search5", "search6", "search7"};

  history::Cluster cluster;
  cluster.visits = {visit};

  // Deliberately push two instances of the same cluster into the vector.
  std::vector<history::Cluster> clusters;
  clusters.push_back(cluster);
  clusters.push_back(cluster);

  // Verify that we correctly coalesce searches for BOTH clusters.
  CoalesceRelatedSearches(clusters);
  EXPECT_THAT(
      clusters[0].related_searches,
      ElementsAre("search1", "search2", "search3", "search4", "search5"));
  EXPECT_THAT(
      clusters[1].related_searches,
      ElementsAre("search1", "search2", "search3", "search4", "search5"));
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

TEST(HistoryClustersUtilTest, IsShownVisitCandidateZeroScore) {
  history::ClusterVisit cluster_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                           base::Time::FromTimeT(10)),
      std::nullopt, 0.0);

  ASSERT_FALSE(IsShownVisitCandidate(cluster_visit));
}

TEST(HistoryClustersUtilTest, IsShownVisitCandidateHidden) {
  history::ClusterVisit cluster_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                           base::Time::FromTimeT(10)),
      std::nullopt, 1.0);
  cluster_visit.interaction_state =
      history::ClusterVisit::InteractionState::kHidden;

  ASSERT_FALSE(IsShownVisitCandidate(cluster_visit));
}

TEST(HistoryClustersUtilTest, IsShownVisitCandidateNoTitle) {
  history::ClusterVisit cluster_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                           base::Time::FromTimeT(10)),
      std::nullopt, 0.0);
  cluster_visit.annotated_visit.url_row.set_title(u"");

  ASSERT_FALSE(IsShownVisitCandidate(cluster_visit));
}

TEST(HistoryClustersUtilTest, IsShownVisitCandidate) {
  history::ClusterVisit cluster_visit = testing::CreateClusterVisit(
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://two.com/"),
                                           base::Time::FromTimeT(10)),
      std::nullopt, 1.0);
  cluster_visit.annotated_visit.url_row.set_title(u"sometitle");

  ASSERT_TRUE(IsShownVisitCandidate(cluster_visit));
}

}  // namespace
}  // namespace history_clusters
