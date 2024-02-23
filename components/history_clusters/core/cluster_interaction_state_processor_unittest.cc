// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_clusters/core/cluster_interaction_state_processor.h"
#include <string>
#include <vector>

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/clustering_test_utils.h"
#include "components/history_clusters/core/config.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history_clusters {
namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

// Returns four clusters with following properties:
// 1. Cluster 1 with a visits: that has search terms marked as hidden.
//      - visit 1: Search term: Hidden; Interaction state: Default.
//      - visit 2: Search term: Hidden; Interaction state: Default.
//      - visit 3: Search term: Hidden; Interaction state: Default.
// 2. Cluster 2 with a visits: that has search terms marked as done and hidden.
//      - visit 4: Search term: Done; Interaction state: Default.
//      - visit 5: Search term: Show; Interaction state: Default.
//      - visit 6: Search term: Hidden; Interaction state: Default.
// 3. Cluster 3 with visits:
//      - visit 7: Search term: Done; Interaction state: Done.
//      - visit 8: Search term: Hidden; Interaction state: Default.
//      - visit 9: Search term: Hidden; Interaction state: Default.
// 4. Cluster 4 with visits:
//      - visit 10: Search term: Done; Interaction state: Default.
//      - visit 11: Search term: Hidden; Interaction state: Hidden.
//      - visit 12: Search term: Hidden; Interaction state: Default.
std::vector<history::Cluster> GetTestClusters() {
  std::vector<history::Cluster> clusters;

  history::AnnotatedVisit visit1 =
      testing::CreateDefaultAnnotatedVisit(1, GURL("https://github.com/"));
  visit1.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit1.content_annotations.search_terms = u"hidden";
  history::AnnotatedVisit visit2 =
      testing::CreateDefaultAnnotatedVisit(2, GURL("https://google.com/"));
  visit2.content_annotations.model_annotations.entities = {{"baz", 1}};
  visit2.content_annotations.search_terms = u"hidden";
  history::AnnotatedVisit visit3 =
      testing::CreateDefaultAnnotatedVisit(3, GURL("https://github.com/"));
  visit3.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit3.content_annotations.search_terms = u"hidden";
  history::Cluster cluster1;
  cluster1.visits = {
      testing::CreateClusterVisit(
          visit1, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
      testing::CreateClusterVisit(
          visit2, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
      testing::CreateClusterVisit(
          visit3, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
  };
  clusters.push_back(cluster1);

  history::AnnotatedVisit visit4 =
      testing::CreateDefaultAnnotatedVisit(4, GURL("https://github.com/"));
  visit4.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit4.content_annotations.search_terms = u"done";
  history::AnnotatedVisit visit5 =
      testing::CreateDefaultAnnotatedVisit(5, GURL("https://google.com/"));
  visit5.content_annotations.model_annotations.entities = {{"baz", 1}};
  visit5.content_annotations.search_terms = u"show";
  history::AnnotatedVisit visit6 =
      testing::CreateDefaultAnnotatedVisit(6, GURL("https://github.com/"));
  visit6.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit6.content_annotations.search_terms = u"hidden";
  history::Cluster cluster2;
  cluster2.visits = {
      testing::CreateClusterVisit(
          visit4, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
      testing::CreateClusterVisit(
          visit5, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
      testing::CreateClusterVisit(
          visit6, std::nullopt, 1.0,
          history::ClusterVisit::InteractionState::kDefault),
  };
  clusters.push_back(cluster2);

  history::AnnotatedVisit visit7 =
      testing::CreateDefaultAnnotatedVisit(7, GURL("https://github.com/"));
  visit7.content_annotations.model_annotations.entities = {{"github", 1}};
  visit7.content_annotations.search_terms = u"done";
  history::AnnotatedVisit visit8 =
      testing::CreateDefaultAnnotatedVisit(8, GURL("https://google.com/"));
  visit8.content_annotations.model_annotations.entities = {{"baz", 1}};
  visit8.content_annotations.search_terms = u"hidden";
  history::AnnotatedVisit visit9 =
      testing::CreateDefaultAnnotatedVisit(9, GURL("https://github.com/"));
  visit9.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit9.content_annotations.search_terms = u"hidden";
  history::Cluster cluster3;
  cluster3.visits = {testing::CreateClusterVisit(
                         visit7, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kDone),
                     testing::CreateClusterVisit(
                         visit8, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kDefault),
                     testing::CreateClusterVisit(
                         visit9, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kDefault)};
  clusters.push_back(cluster3);

  history::AnnotatedVisit visit10 =
      testing::CreateDefaultAnnotatedVisit(10, GURL("https://github.com/"));
  visit10.content_annotations.model_annotations.entities = {{"otherentity", 1}};
  visit10.content_annotations.search_terms = u"done";
  history::AnnotatedVisit visit11 =
      testing::CreateDefaultAnnotatedVisit(11, GURL("https://google.com/"));
  visit11.content_annotations.model_annotations.entities = {{"baz", 1}};
  visit11.content_annotations.search_terms = u"hidden";
  history::AnnotatedVisit visit12 =
      testing::CreateDefaultAnnotatedVisit(12, GURL("https://google.com/"));
  visit12.content_annotations.model_annotations.entities = {{"baz", 1}};
  visit12.content_annotations.search_terms = u"hidden";
  history::Cluster cluster4;
  cluster4.visits = {testing::CreateClusterVisit(
                         visit10, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kDefault),
                     testing::CreateClusterVisit(
                         visit11, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kHidden),
                     testing::CreateClusterVisit(
                         visit12, std::nullopt, 1.0,
                         history::ClusterVisit::InteractionState::kDefault)};
  clusters.push_back(cluster4);
  return clusters;
}

// Do not filter any clusters on interaction state.
TEST(ClusterInteractionStateProcessorTest, NoInteractionStateFiltration) {
  QueryClustersFilterParams filter_params;
  auto clusters = GetTestClusters();
  auto cluster_processor =
      std::make_unique<ClusterInteractionStateProcessor>(filter_params);
  cluster_processor->ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              UnorderedElementsAre(
                  ElementsAre(testing::VisitResult(1, 1.0, {}, u"hidden"),
                              testing::VisitResult(2, 1.0, {}, u"hidden"),
                              testing::VisitResult(3, 1.0, {}, u"hidden")),
                  ElementsAre(testing::VisitResult(4, 1.0, {}, u"done"),
                              testing::VisitResult(5, 1.0, {}, u"show"),
                              testing::VisitResult(6, 1.0, {}, u"hidden")),
                  ElementsAre(testing::VisitResult(7, 1.0, {}, u"done"),
                              testing::VisitResult(8, 1.0, {}, u"hidden"),
                              testing::VisitResult(9, 1.0, {}, u"hidden")),
                  ElementsAre(testing::VisitResult(10, 1.0, {}, u"done"),
                              testing::VisitResult(11, 1.0, {}, u"hidden"),
                              testing::VisitResult(12, 1.0, {}, u"hidden"))));
}

// Filter all clusters with done visits or done search terms. i.e: Cluster 3
// should filter out Cluster 2 and 4 as well.
TEST(ClusterInteractionStateProcessorTest, FilterClustersWithDoneStateVisits) {
  QueryClustersFilterParams filter_params;
  filter_params.filter_done_clusters = true;
  auto clusters = GetTestClusters();
  auto cluster_processor =
      std::make_unique<ClusterInteractionStateProcessor>(filter_params);
  cluster_processor->ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              UnorderedElementsAre(
                  ElementsAre(testing::VisitResult(1, 1.0, {}, u"hidden"),
                              testing::VisitResult(2, 1.0, {}, u"hidden"),
                              testing::VisitResult(3, 1.0, {}, u"hidden"))));
}

// Filter all visits marked hidden or with hidden search terms. i.e: Visit 11
// should filter out Visits 1,2,3,6,8,9,12.
TEST(ClusterInteractionStateProcessorTest, FilterHiddenStateVisits) {
  QueryClustersFilterParams filter_params = QueryClustersFilterParams();
  filter_params.filter_hidden_visits = true;
  auto clusters = GetTestClusters();
  auto cluster_processor =
      std::make_unique<ClusterInteractionStateProcessor>(filter_params);
  cluster_processor->ProcessClusters(&clusters);
  EXPECT_THAT(testing::ToVisitResults(clusters),
              UnorderedElementsAre(
                  ElementsAre(testing::VisitResult(4, 1.0, {}, u"done"),
                              testing::VisitResult(5, 1.0, {}, u"show")),
                  ElementsAre(testing::VisitResult(7, 1.0, {}, u"done")),
                  ElementsAre(testing::VisitResult(10, 1.0, {}, u"done"))));
}

// Filter both done and hidden states, should give empty clusters.
TEST(ClusterInteractionStateProcessorTest,
     FilterBothDoneAndHiddenStateClusters) {
  QueryClustersFilterParams filter_params;
  filter_params.filter_hidden_visits = true;
  filter_params.filter_done_clusters = true;
  auto clusters = GetTestClusters();
  auto cluster_processor =
      std::make_unique<ClusterInteractionStateProcessor>(filter_params);
  cluster_processor->ProcessClusters(&clusters);
  EXPECT_TRUE(clusters.empty());
}

}  // namespace
}  // namespace history_clusters
