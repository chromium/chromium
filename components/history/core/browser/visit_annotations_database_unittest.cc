// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"
#include <string>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/visit_database.h"
#include "components/history/core/test/visit_annotations_test_utils.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

using ::testing::ElementsAre;
using ::testing::UnorderedElementsAre;

VisitContextAnnotations MakeContextAnnotations(
    VisitContextAnnotations::OnVisitFields on_visit,
    bool omnibox_url_copied,
    bool is_existing_part_of_tab_group,
    bool is_placed_in_tab_group,
    bool is_existing_bookmark,
    bool is_new_bookmark,
    bool is_ntp_custom_link) {
  VisitContextAnnotations result;
  result.on_visit = on_visit;
  result.omnibox_url_copied = omnibox_url_copied;
  result.is_existing_part_of_tab_group = is_existing_part_of_tab_group;
  result.is_placed_in_tab_group = is_placed_in_tab_group;
  result.is_existing_bookmark = is_existing_bookmark;
  result.is_new_bookmark = is_new_bookmark;
  result.is_ntp_custom_link = is_ntp_custom_link;
  return result;
}

}  // namespace

class VisitAnnotationsDatabaseTest : public testing::Test,
                                     public VisitAnnotationsDatabase,
                                     public VisitDatabase {
 public:
  VisitAnnotationsDatabaseTest() = default;
  ~VisitAnnotationsDatabaseTest() override = default;

 protected:
  VisitID AddVisitWithTime(base::Time visit_time,
                           bool add_context_annotation = true) {
    VisitRow visit_row;
    visit_row.visit_time = visit_time;
    auto visit_id = AddVisit(&visit_row, VisitSource::SOURCE_BROWSED);
    if (add_context_annotation)
      AddContextAnnotationsForVisit(visit_id, {});
    return visit_id;
  }

  void AddCluster(const std::vector<VisitID>& visit_ids) {
    AddClusters({CreateCluster(visit_ids)});
  }

  void ExpectContextAnnotations(VisitContextAnnotations actual,
                                VisitContextAnnotations expected) {
    EXPECT_EQ(actual.on_visit.browser_type, expected.on_visit.browser_type);
    EXPECT_EQ(actual.on_visit.window_id, expected.on_visit.window_id);
    EXPECT_EQ(actual.on_visit.tab_id, expected.on_visit.tab_id);
    EXPECT_EQ(actual.on_visit.task_id, expected.on_visit.task_id);
    EXPECT_EQ(actual.on_visit.root_task_id, expected.on_visit.root_task_id);
    EXPECT_EQ(actual.on_visit.parent_task_id, expected.on_visit.parent_task_id);
    EXPECT_EQ(actual.on_visit.response_code, expected.on_visit.response_code);
    EXPECT_EQ(actual.omnibox_url_copied, expected.omnibox_url_copied);
    EXPECT_EQ(actual.is_existing_part_of_tab_group,
              expected.is_existing_part_of_tab_group);
    EXPECT_EQ(actual.is_placed_in_tab_group, expected.is_placed_in_tab_group);
    EXPECT_EQ(actual.is_existing_bookmark, expected.is_existing_bookmark);
    EXPECT_EQ(actual.is_new_bookmark, expected.is_new_bookmark);
    EXPECT_EQ(actual.is_ntp_custom_link, expected.is_ntp_custom_link);
    EXPECT_EQ(actual.duration_since_last_visit,
              expected.duration_since_last_visit);
    EXPECT_EQ(actual.page_end_reason, expected.page_end_reason);
    EXPECT_EQ(actual.total_foreground_duration,
              expected.total_foreground_duration);
  }

 private:
  // Test setup.
  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());

    // Initialize the tables for this test.
    EXPECT_TRUE(InitVisitTable());
    EXPECT_TRUE(InitVisitAnnotationsTables());
  }
  void TearDown() override { db_.Close(); }

  // VisitAnnotationsDatabase:
  sql::Database& GetDB() override { return db_; }

  sql::Database db_;
};

TEST_F(VisitAnnotationsDatabaseTest, AddContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentModelAnnotations model_annotations = {
      0.5f,
      {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}},
      123,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kBrowsingTopicsEligible;
  std::vector<std::string> related_searches{"related searches",
                                            "búsquedas relacionadas"};
  VisitContentAnnotations content_annotations{
      annotation_flags,
      model_annotations,
      related_searches,
      GURL("http://pagewithvisit.com?q=search"),
      u"search",
      "Alternative title",
      "en",
      VisitContentAnnotations::PasswordState::kUnknown,
      /*has_url_keyed_image=*/true};
  AddContentAnnotationsForVisit(visit_id, content_annotations);

  // Query for it.
  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(
      GetContentAnnotationsForVisit(visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(0.5f, got_content_annotations.model_annotations.visibility_score);
  EXPECT_THAT(
      got_content_annotations.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(
      123, got_content_annotations.model_annotations.page_topics_model_version);
  EXPECT_THAT(got_content_annotations.model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));
  EXPECT_THAT(got_content_annotations.related_searches,
              ElementsAre("related searches", "búsquedas relacionadas"));
  EXPECT_EQ(GURL("http://pagewithvisit.com?q=search"),
            got_content_annotations.search_normalized_url);
  EXPECT_EQ(u"search", got_content_annotations.search_terms);
  EXPECT_EQ("Alternative title", got_content_annotations.alternative_title);
  EXPECT_TRUE(got_content_annotations.has_url_keyed_image);
}

TEST_F(VisitAnnotationsDatabaseTest,
       AddContextAnnotationsForVisit_GetAnnotatedVisit) {
  AddVisitWithTime(IntToTime(20), false);
  AddVisitWithTime(IntToTime(30), false);
  AddVisitWithTime(IntToTime(10), false);

  const std::vector<VisitContextAnnotations> visit_context_annotations_list = {
      MakeContextAnnotations(
          {VisitContextAnnotations::BrowserType::kTabbed,
           SessionID::FromSerializedValue(10),
           SessionID::FromSerializedValue(11), 101, 102, 103, 200},
          true, false, true, true, false, false),
      MakeContextAnnotations(
          {VisitContextAnnotations::BrowserType::kPopup,
           SessionID::FromSerializedValue(12),
           SessionID::FromSerializedValue(13), 104, 105, 106, 200},
          false, true, false, false, false, true),
      MakeContextAnnotations(
          {VisitContextAnnotations::BrowserType::kCustomTab,
           SessionID::FromSerializedValue(14),
           SessionID::FromSerializedValue(15), 107, 108, 109, 404},
          false, true, true, false, true, false),
      MakeContextAnnotations(
          {VisitContextAnnotations::BrowserType::kAuthTab,
           SessionID::FromSerializedValue(16),
           SessionID::FromSerializedValue(17), 110, 111, 112, 404},
          false, false, false, false, false, false)};

  // Verify `AddContextAnnotationsForVisit()` and `GetAnnotatedVisits()`.
  AddContextAnnotationsForVisit(1, visit_context_annotations_list[0]);
  AddContextAnnotationsForVisit(2, visit_context_annotations_list[1]);
  AddContextAnnotationsForVisit(3, visit_context_annotations_list[2]);
  AddContextAnnotationsForVisit(4, visit_context_annotations_list[3]);

  for (size_t i = 0; i < std::size(visit_context_annotations_list); ++i) {
    SCOPED_TRACE(testing::Message() << "i: " << i);
    VisitContextAnnotations actual;
    VisitID visit_id = i + 1;  // VisitIDs are start at 1.
    EXPECT_TRUE(GetContextAnnotationsForVisit(visit_id, &actual));
    ExpectContextAnnotations(actual, visit_context_annotations_list[i]);
  }

  // Verify `DeleteAnnotationsForVisit()`.
  DeleteAnnotationsForVisit(1);
  DeleteAnnotationsForVisit(3);

  VisitContextAnnotations actual;
  EXPECT_FALSE(GetContextAnnotationsForVisit(1, &actual));

  // Visit ID = 2 is in the 1st indexed position.
  EXPECT_TRUE(GetContextAnnotationsForVisit(2, &actual));
  ExpectContextAnnotations(actual, visit_context_annotations_list[1]);

  EXPECT_FALSE(GetContextAnnotationsForVisit(3, &actual));
}

TEST_F(VisitAnnotationsDatabaseTest, UpdateContextAnnotationsForVisit) {
  // Add the initial visits and annotations.
  VisitID visit1_id = AddVisitWithTime(IntToTime(10), false);
  VisitID visit2_id = AddVisitWithTime(IntToTime(20), false);

  VisitContextAnnotations visit1_annotation = MakeContextAnnotations(
      {VisitContextAnnotations::BrowserType::kTabbed,
       SessionID::FromSerializedValue(10), SessionID::FromSerializedValue(11),
       101, 102, 103, 200},
      false, false, false, false, false, false);
  VisitContextAnnotations visit2_annotation = MakeContextAnnotations(
      {VisitContextAnnotations::BrowserType::kPopup,
       SessionID::FromSerializedValue(12), SessionID::FromSerializedValue(13),
       104, 105, 106, 200},
      false, true, false, false, false, true);

  AddContextAnnotationsForVisit(visit1_id, visit1_annotation);
  AddContextAnnotationsForVisit(visit2_id, visit2_annotation);

  // Update the annotation of the first visit.
  VisitContextAnnotations visit1_annotation_updated = MakeContextAnnotations(
      {VisitContextAnnotations::BrowserType::kCustomTab,
       SessionID::FromSerializedValue(14), SessionID::FromSerializedValue(15),
       107, 108, 109, 400},
      true, true, true, true, true, true);
  UpdateContextAnnotationsForVisit(visit1_id, visit1_annotation_updated);

  // Make sure all the fields were updated.
  VisitContextAnnotations visit1_annotation_actual;
  ASSERT_TRUE(
      GetContextAnnotationsForVisit(visit1_id, &visit1_annotation_actual));
  ExpectContextAnnotations(visit1_annotation_actual, visit1_annotation_updated);

  // The annotation for the other visit should be unchanged.
  VisitContextAnnotations visit2_annotation_actual;
  ASSERT_TRUE(
      GetContextAnnotationsForVisit(visit2_id, &visit2_annotation_actual));
  ExpectContextAnnotations(visit2_annotation_actual, visit2_annotation);
}

TEST_F(VisitAnnotationsDatabaseTest, UpdateContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentModelAnnotations model_annotations = {
      0.5f,
      {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}},
      123,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  std::vector<std::string> related_searches{"related searches"};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kBrowsingTopicsEligible;
  VisitContentAnnotations original{
      annotation_flags,
      model_annotations,
      related_searches,
      GURL("http://pagewithvisit.com?q=search"),
      u"search",
      "Alternative title",
      "en",
      VisitContentAnnotations::PasswordState::kUnknown,
      /*has_url_keyed_image=*/false};
  AddContentAnnotationsForVisit(visit_id, original);

  // Mutate that row.
  VisitContentAnnotations modification(original);
  modification.model_annotations.visibility_score = 0.3f;
  modification.related_searches.emplace_back("búsquedas relacionadas");
  modification.search_normalized_url =
      GURL("http://pagewithvisit.com?q=search2");
  modification.search_terms = u"search2";
  modification.alternative_title = "New alternative title";
  modification.has_url_keyed_image = true;
  UpdateContentAnnotationsForVisit(visit_id, modification);

  // Check that the mutated version was written.
  VisitContentAnnotations final;
  ASSERT_TRUE(GetContentAnnotationsForVisit(visit_id, &final));

  EXPECT_EQ(VisitContentAnnotationFlag::kBrowsingTopicsEligible,
            final.annotation_flags);
  EXPECT_EQ(0.3f, final.model_annotations.visibility_score);
  EXPECT_THAT(
      final.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/"1", /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/"2", /*weight=*/1)));
  EXPECT_EQ(123, final.model_annotations.page_topics_model_version);
  EXPECT_THAT(final.model_annotations.entities,
              ElementsAre(VisitContentModelAnnotations::Category(
                              /*id=*/"entity1", /*weight=*/1),
                          VisitContentModelAnnotations::Category(
                              /*id=*/"entity2", /*weight=*/1)));
  EXPECT_THAT(final.related_searches,
              ElementsAre("related searches", "búsquedas relacionadas"));
  EXPECT_EQ(final.search_normalized_url,
            GURL("http://pagewithvisit.com?q=search2"));
  EXPECT_EQ(final.search_terms, u"search2");
  EXPECT_EQ(final.alternative_title, "New alternative title");
  EXPECT_TRUE(final.has_url_keyed_image);
}

TEST_F(
    VisitAnnotationsDatabaseTest,
    AddClusters_GetCluster_GetClusterVisit_GetClusterKeywords_GetDuplicateClusterVisitIdsForClusterVisit) {
  // Test `AddClusters()`.

  // Cluster without visits shouldn't be added.
  std::vector<Cluster> clusters;
  // Cluster ID shouldn't matter, it should be auto-incremented in the db.
  clusters.push_back({10, {}});

  // Clusters with visits should be added.
  ClusterVisit visit_1;
  // Visit ID should matter, they should not be auto-incremented in the db.
  visit_1.annotated_visit.visit_row.visit_id = 20;
  visit_1.score = .4;
  visit_1.engagement_score = .3;
  visit_1.url_for_deduping = GURL{"url_for_deduping"};
  visit_1.normalized_url = GURL{"normalized_url"};
  visit_1.url_for_display = u"url_for_display";
  // `matches_search_query` shouldn't matter, it isn't persisted.
  visit_1.matches_search_query = true;
  // Duplicate visits should be persisted.
  visit_1.duplicate_visits.push_back({3});
  visit_1.duplicate_visits.push_back({4});

  ClusterVisit visit_2;
  visit_2.annotated_visit.visit_row.visit_id = 21;
  visit_2.score = .2;
  visit_2.engagement_score = .1;
  visit_2.url_for_deduping = GURL{"url_for_deduping_2"};
  visit_2.normalized_url = GURL{"normalized_url_2"};
  visit_2.url_for_display = u"url_for_display_2";
  visit_2.interaction_state = ClusterVisit::InteractionState::kHidden;

  // `search_match_score` shouldn't matter, it is not persisted.
  clusters.push_back(
      {11, {visit_1, visit_2}, {}, false, u"label", u"raw_label", {}, {}, .6});

  // Empty or `nullopt` labels should both be retrieved as `nullopt`.
  clusters.push_back({11, {visit_2}, {}, false, u"", std::nullopt, {}, {}, .6});
  AddClusters(clusters);

  // Test `GetCluster()`.

  // Should return the non-empty cluster2.
  const auto cluster_1 = GetCluster(1);
  EXPECT_EQ(cluster_1.cluster_id, 1);
  EXPECT_EQ(cluster_1.should_show_on_prominent_ui_surfaces, false);
  EXPECT_EQ(cluster_1.label, u"label");
  EXPECT_EQ(cluster_1.raw_label, u"raw_label");
  // Should not populate `visits`.
  EXPECT_TRUE(cluster_1.visits.empty());
  EXPECT_THAT(GetVisitIdsInCluster(1), UnorderedElementsAre(20, 21));
  // Should not populate the non-persisted `search_match_score` field.
  EXPECT_EQ(cluster_1.search_match_score, 0);

  const auto cluster_2 = GetCluster(2);
  EXPECT_EQ(cluster_2.cluster_id, 2);
  EXPECT_EQ(cluster_2.label, std::nullopt);
  EXPECT_EQ(cluster_2.raw_label, std::nullopt);
  EXPECT_THAT(GetVisitIdsInCluster(2), UnorderedElementsAre(21));

  // There should be no other cluster.
  EXPECT_EQ(GetCluster(3).cluster_id, 0);

  // Test `GetClusterVisit()`.

  const auto visit_1_retrieved = GetClusterVisit(20);
  EXPECT_EQ(visit_1_retrieved.annotated_visit.visit_row.visit_id, 20);
  EXPECT_EQ(visit_1_retrieved.score, .4f);
  EXPECT_EQ(visit_1_retrieved.engagement_score, .3f);
  EXPECT_EQ(visit_1_retrieved.url_for_deduping, GURL{"url_for_deduping"});
  EXPECT_EQ(visit_1_retrieved.normalized_url, GURL{"normalized_url"});
  EXPECT_EQ(visit_1_retrieved.url_for_display, u"url_for_display");
  // Should not populate the non-persisted `matches_search_query` field.`
  EXPECT_EQ(visit_1_retrieved.matches_search_query, false);
  // Should not populate `duplicate_visits`.
  EXPECT_TRUE(visit_1_retrieved.duplicate_visits.empty());

  const auto visit_2_retrieved = GetClusterVisit(21);
  EXPECT_EQ(visit_2_retrieved.annotated_visit.visit_row.visit_id, 21);
  EXPECT_EQ(visit_2_retrieved.score, .2f);
  EXPECT_EQ(visit_2_retrieved.engagement_score, .1f);
  EXPECT_EQ(visit_2_retrieved.url_for_deduping, GURL{"url_for_deduping_2"});
  EXPECT_EQ(visit_2_retrieved.normalized_url, GURL{"normalized_url_2"});
  EXPECT_EQ(visit_2_retrieved.url_for_display, u"url_for_display_2");
  EXPECT_EQ(visit_2_retrieved.interaction_state,
            ClusterVisit::InteractionState::kHidden);

  // Test `GetDuplicateClusterVisitIdsForClusterVisit()`.

  const auto duplicate_visits_retrieved =
      GetDuplicateClusterVisitIdsForClusterVisit(20);
  EXPECT_THAT(duplicate_visits_retrieved, ElementsAre(3, 4));
}

TEST_F(VisitAnnotationsDatabaseTest, GetMostRecentClusterIds) {
  AddCluster(
      {AddVisitWithTime(IntToTime(11)), AddVisitWithTime(IntToTime(12))});
  AddCluster(
      {AddVisitWithTime(IntToTime(101)), AddVisitWithTime(IntToTime(102))});
  AddCluster(
      {AddVisitWithTime(IntToTime(13)), AddVisitWithTime(IntToTime(104))});
  AddCluster(
      {AddVisitWithTime(IntToTime(103)), AddVisitWithTime(IntToTime(50))});

  // Should return clusters with at least 1 visit >= min time.
  // Should be ordered max visit time descending.
  EXPECT_EQ(GetMostRecentClusterIds(IntToTime(101), IntToTime(120), 10),
            std::vector<int64_t>({3, 4, 2}));
  // Should not return clusters with visits > max time.
  EXPECT_EQ(GetMostRecentClusterIds(IntToTime(100), IntToTime(103), 10),
            std::vector<int64_t>({2}));
  // Should return at most `max_clusters`.
  EXPECT_EQ(GetMostRecentClusterIds(IntToTime(0), IntToTime(500), 1),
            std::vector<int64_t>({3}));
}

TEST_F(VisitAnnotationsDatabaseTest,
       GetVisitIdsInCluster_GetClusterIdContainingVisit) {
  // Add unclustered visits.
  AddVisitWithTime(IntToTime(0));
  AddVisitWithTime(IntToTime(2));
  AddVisitWithTime(IntToTime(4));
  // Add clustered visits.
  AddCluster({AddVisitWithTime(IntToTime(1))});
  AddCluster({AddVisitWithTime(IntToTime(3))});
  // Add a cluster with multiple visits.
  auto cluster = CreateCluster(
      {AddVisitWithTime(IntToTime(5)), AddVisitWithTime(IntToTime(7)),
       AddVisitWithTime(IntToTime(9)), AddVisitWithTime(IntToTime(11))});
  cluster.visits[0].score = .6;  // visit 6
  cluster.visits[1].score = 1;   // visit 7
  cluster.visits[2].score = .6;  // visit 8
  cluster.visits[3].score = .8;  // visit 9
  AddClusters({cluster});

  // GetVisitIdsInCluster
  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(4));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(7, 9, 8, 6));

  // GetClusterIdContainingVisit
  EXPECT_EQ(GetClusterIdContainingVisit(1), 0);
  EXPECT_EQ(GetClusterIdContainingVisit(2), 0);
  EXPECT_EQ(GetClusterIdContainingVisit(3), 0);
  EXPECT_EQ(GetClusterIdContainingVisit(4), 1);
  EXPECT_EQ(GetClusterIdContainingVisit(5), 2);
  EXPECT_EQ(GetClusterIdContainingVisit(6), 3);
  EXPECT_EQ(GetClusterIdContainingVisit(7), 3);
}

TEST_F(VisitAnnotationsDatabaseTest,
       ReserveNextClusterId_AddVisitsToCluster_UpdateClusterTriggerability) {
  // Add an initial cluster with multiple visits.
  VisitID visit_id1 = AddVisitWithTime(IntToTime(0));
  VisitID visit_id2 = AddVisitWithTime(IntToTime(1));
  int64_t cluster_id1 = ReserveNextClusterId("", 0);
  int64_t cluster_id2 = ReserveNextClusterId("", 0);
  Cluster cluster1 = CreateCluster({visit_id1, visit_id2});
  cluster1.cluster_id = cluster_id1;
  AddVisitsToCluster(cluster_id1, cluster1.visits);

  // Add a second cluster.
  VisitID visit_id3 = AddVisitWithTime(IntToTime(2));
  Cluster cluster2 = CreateCluster({visit_id3});
  cluster2.cluster_id = cluster_id2;
  AddVisitsToCluster(cluster_id2, {cluster2.visits});

  // Update cluster triggerability initially.
  cluster1.should_show_on_prominent_ui_surfaces = true;
  cluster1.triggerability_calculated = true;
  cluster1.keyword_to_data_map[u"keyword1"];
  cluster1.keyword_to_data_map[u"keyword2"];
  cluster2.should_show_on_prominent_ui_surfaces = false;
  cluster2.triggerability_calculated = true;
  cluster2.keyword_to_data_map[u"keyword3"];
  UpdateClusterTriggerability({cluster1, cluster2});

  Cluster out_cluster1 = GetCluster(cluster_id1);
  out_cluster1.keyword_to_data_map = GetClusterKeywords(cluster_id1);
  EXPECT_TRUE(out_cluster1.should_show_on_prominent_ui_surfaces);
  EXPECT_TRUE(out_cluster1.triggerability_calculated);
  EXPECT_EQ(out_cluster1.keyword_to_data_map.size(), 2u);
  EXPECT_THAT(GetVisitIdsInCluster(cluster_id1),
              UnorderedElementsAre(visit_id1, visit_id2));

  Cluster out_cluster2 = GetCluster(cluster_id2);
  out_cluster2.keyword_to_data_map = GetClusterKeywords(cluster_id2);
  EXPECT_FALSE(out_cluster2.should_show_on_prominent_ui_surfaces);
  EXPECT_TRUE(out_cluster2.triggerability_calculated);
  EXPECT_EQ(out_cluster2.keyword_to_data_map.size(), 1u);
  EXPECT_TRUE(out_cluster2.keyword_to_data_map.contains(u"keyword3"));
  EXPECT_THAT(GetVisitIdsInCluster(cluster_id2),
              UnorderedElementsAre(visit_id3));
  EXPECT_EQ(GetClusterVisit(visit_id3).score, 1.0);

  // Add another visit to the second cluster.
  VisitID visit_id4 = AddVisitWithTime(IntToTime(4));
  ClusterVisit cluster_visit4;
  cluster_visit4.annotated_visit.visit_row.visit_id = visit_id4;
  AddVisitsToCluster(cluster_id2, {cluster_visit4});

  // Update cluster triggerability again for one of the clusters.
  cluster2.should_show_on_prominent_ui_surfaces = true;
  cluster2.keyword_to_data_map.clear();
  cluster2.keyword_to_data_map[u"keyword4"];
  cluster2.label = u"somelabel";
  cluster2.raw_label = u"somerawlabel";
  auto& cluster2_visit1 = cluster2.visits.front();
  cluster2_visit1.score = 0.5;
  cluster2_visit1.duplicate_visits.push_back({visit_id4});
  UpdateClusterTriggerability({cluster2});

  out_cluster2 = GetCluster(cluster_id2);
  out_cluster2.keyword_to_data_map = GetClusterKeywords(cluster_id2);
  EXPECT_TRUE(out_cluster2.should_show_on_prominent_ui_surfaces);
  EXPECT_TRUE(out_cluster2.triggerability_calculated);
  EXPECT_EQ(out_cluster2.label.value_or(u""), u"somelabel");
  EXPECT_EQ(out_cluster2.raw_label.value_or(u""), u"somerawlabel");
  EXPECT_EQ(out_cluster2.keyword_to_data_map.size(), 1u);
  EXPECT_TRUE(out_cluster2.keyword_to_data_map.contains(u"keyword4"));
  EXPECT_THAT(GetVisitIdsInCluster(cluster_id2),
              UnorderedElementsAre(visit_id3, visit_id4));
  EXPECT_EQ(GetClusterVisit(visit_id3).score, 0.5);
  EXPECT_THAT(GetDuplicateClusterVisitIdsForClusterVisit(visit_id3),
              UnorderedElementsAre(visit_id4));
}

TEST_F(VisitAnnotationsDatabaseTest, GetClusterIdForSyncedDetails) {
  std::string originator_cache_guid = "somedevice";
  int64_t originator_cluster_id = 1;

  // Not a cluster with these details yet, so we expect for the cluster id to be
  // 0.
  EXPECT_EQ(GetClusterIdForSyncedDetails(originator_cache_guid,
                                         originator_cluster_id),
            0);

  // Now, add a cluster ID with the details and make sure it was reserved
  // successfully.
  int64_t reserved_cluster_id =
      ReserveNextClusterId(originator_cache_guid, originator_cluster_id);
  EXPECT_GT(reserved_cluster_id, 0);

  // Ask for the cluster id for the same synced details and should get the one
  // that's been reserved.
  EXPECT_EQ(GetClusterIdForSyncedDetails(originator_cache_guid,
                                         originator_cluster_id),
            reserved_cluster_id);

  // Make sure that a different device with the same cluster id does not get
  // resolved to the same cluster id.
  EXPECT_EQ(GetClusterIdForSyncedDetails("otherdevice", originator_cluster_id),
            0);

  // Make sure that a cluster with the same originator cache guid but different
  // cluster id does not get resolved to the same cluster id.
  EXPECT_EQ(GetClusterIdForSyncedDetails(originator_cache_guid, 3), 0);
}

TEST_F(VisitAnnotationsDatabaseTest, DeleteAnnotationsForVisit) {
  // Add a cluster with 2 visits.
  AddContentAnnotationsForVisit(1, {});
  AddContextAnnotationsForVisit(1, {});
  AddContentAnnotationsForVisit(2, {});
  AddContextAnnotationsForVisit(2, {});
  auto cluster = CreateCluster({1, 2});
  cluster.keyword_to_data_map[u"keyword1"];
  cluster.keyword_to_data_map[u"keyword2"];
  cluster.visits[0].duplicate_visits.push_back({3});
  AddClusters({cluster});

  VisitContentAnnotations got_content_annotations;
  VisitContextAnnotations got_context_annotations;
  // First make sure the annotation and cluster tables are populated.
  EXPECT_TRUE(GetContentAnnotationsForVisit(1, &got_content_annotations));
  EXPECT_TRUE(GetContextAnnotationsForVisit(1, &got_context_annotations));
  EXPECT_TRUE(GetContentAnnotationsForVisit(2, &got_content_annotations));
  EXPECT_TRUE(GetContextAnnotationsForVisit(2, &got_context_annotations));
  EXPECT_EQ(GetCluster(1).cluster_id, 1);
  EXPECT_THAT(GetVisitIdsInCluster(1), UnorderedElementsAre(1, 2));
  EXPECT_EQ(GetClusterIdContainingVisit(1), 1);
  EXPECT_EQ(GetClusterIdContainingVisit(2), 1);
  EXPECT_EQ(GetClusterKeywords(1).size(), 2u);
  EXPECT_EQ(GetDuplicateClusterVisitIdsForClusterVisit(1).size(), 1u);
  EXPECT_TRUE(GetDuplicateClusterVisitIdsForClusterVisit(2).empty());
  EXPECT_TRUE(GetDuplicateClusterVisitIdsForClusterVisit(3).empty());

  // Delete 1 visit. Make sure the tables are updated, but the cluster remains.
  DeleteAnnotationsForVisit(1);
  EXPECT_FALSE(GetContentAnnotationsForVisit(1, &got_content_annotations));
  EXPECT_FALSE(GetContextAnnotationsForVisit(1, &got_context_annotations));
  EXPECT_TRUE(GetContentAnnotationsForVisit(2, &got_content_annotations));
  EXPECT_TRUE(GetContextAnnotationsForVisit(2, &got_context_annotations));
  EXPECT_EQ(GetCluster(1).cluster_id, 1);
  EXPECT_THAT(GetVisitIdsInCluster(1), UnorderedElementsAre(2));
  EXPECT_EQ(GetClusterIdContainingVisit(1), 0);
  EXPECT_EQ(GetClusterIdContainingVisit(2), 1);
  EXPECT_EQ(GetClusterKeywords(1).size(), 2u);
  EXPECT_TRUE(GetDuplicateClusterVisitIdsForClusterVisit(1).empty());

  // Delete the 2nd visit. Make sure the cluster is removed.
  DeleteAnnotationsForVisit(2);
  EXPECT_FALSE(GetContentAnnotationsForVisit(1, &got_content_annotations));
  EXPECT_FALSE(GetContextAnnotationsForVisit(1, &got_context_annotations));
  EXPECT_FALSE(GetContentAnnotationsForVisit(2, &got_content_annotations));
  EXPECT_FALSE(GetContextAnnotationsForVisit(2, &got_context_annotations));
  EXPECT_EQ(GetCluster(1).cluster_id, 0);
  EXPECT_TRUE(GetVisitIdsInCluster(1).empty());
  EXPECT_EQ(GetClusterIdContainingVisit(1), 0);
  EXPECT_EQ(GetClusterIdContainingVisit(2), 0);
  EXPECT_EQ(GetClusterKeywords(1).size(), 0u);
}

TEST_F(VisitAnnotationsDatabaseTest, AddClusters_DeleteClusters) {
  auto clusters = CreateClusters({{3, 2, 5}, {3, 2, 5}, {6}});
  clusters.back().visits[0].duplicate_visits.push_back({7});
  AddClusters(clusters);

  auto cluster_with_keyword_data = CreateCluster({10});
  cluster_with_keyword_data.keyword_to_data_map[u"keyword1"];
  cluster_with_keyword_data.keyword_to_data_map[u"keyword2"];
  AddClusters({cluster_with_keyword_data});

  EXPECT_EQ(GetCluster(1).cluster_id, 1);
  EXPECT_EQ(GetCluster(2).cluster_id, 2);
  EXPECT_EQ(GetCluster(3).cluster_id, 3);
  EXPECT_EQ(GetCluster(4).cluster_id, 4);
  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(6));
  EXPECT_THAT(GetVisitIdsInCluster(4), ElementsAre(10));
  EXPECT_THAT(GetDuplicateClusterVisitIdsForClusterVisit(6), ElementsAre(7));
  EXPECT_EQ(GetClusterKeywords(4).size(), 2u);

  DeleteClusters({});

  EXPECT_EQ(GetCluster(1).cluster_id, 1);
  EXPECT_EQ(GetCluster(2).cluster_id, 2);
  EXPECT_EQ(GetCluster(3).cluster_id, 3);
  EXPECT_EQ(GetCluster(4).cluster_id, 4);
  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(6));
  EXPECT_THAT(GetVisitIdsInCluster(4), ElementsAre(10));
  EXPECT_THAT(GetDuplicateClusterVisitIdsForClusterVisit(6), ElementsAre(7));
  EXPECT_EQ(GetClusterKeywords(4).size(), 2u);

  DeleteClusters({1, 3, 4, 5});

  EXPECT_EQ(GetCluster(1).cluster_id, 0);
  EXPECT_EQ(GetCluster(2).cluster_id, 2);
  EXPECT_EQ(GetCluster(3).cluster_id, 0);
  EXPECT_EQ(GetCluster(4).cluster_id, 0);
  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre());
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre());
  EXPECT_THAT(GetVisitIdsInCluster(4), ElementsAre());
  // Verifies that the `cluster_visit_duplicates` table is also cleaned up.
  // https://crbug.com/1383274
  EXPECT_THAT(GetDuplicateClusterVisitIdsForClusterVisit(6), ElementsAre());
  EXPECT_TRUE(GetClusterKeywords(4).empty());
}

TEST_F(VisitAnnotationsDatabaseTest, SerializeDataForCrossDeviceSync) {
  // Create required data to be serialized.
  std::vector<VisitContentModelAnnotations::Category> categories = {
      {/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}};
  std::vector<std::string> related_searches{"related searches",
                                            "búsquedas relacionadas"};
  // Serialize data being synced X-Device.
  const auto serialized_categories =
      VisitAnnotationsDatabase::ConvertCategoriesToStringColumn(categories);
  const auto serialized_related_searches =
      VisitAnnotationsDatabase::SerializeToStringColumn(related_searches);
  // Expected serialized format.
  const std::string expected_serialized_categories = "1:1,2:1";
  using std::string_literals::operator""s;
  const std::string expected_serialized_related_searches =
      "related searches\0búsquedas relacionadas"s;

  EXPECT_EQ(serialized_categories, expected_serialized_categories);
  EXPECT_EQ(serialized_related_searches, expected_serialized_related_searches);
}

TEST_F(VisitAnnotationsDatabaseTest, DeserializeDataFromCrossDeviceSync) {
  // Create required data to be deserialized.
  const std::string serialized_categories = "1:1,2:1";
  using std::string_literals::operator""s;
  const std::string serialized_related_searches =
      "related searches\0búsquedas relacionadas"s;

  // Deserialize data being synced X-Device.
  const auto deserialized_categories =
      VisitAnnotationsDatabase::GetCategoriesFromStringColumn(
          serialized_categories);
  const auto deserialized_related_searches =
      VisitAnnotationsDatabase::DeserializeFromStringColumn(
          serialized_related_searches);
  // Expected deserialized data.
  std::vector<VisitContentModelAnnotations::Category>
      expected_deserialized_categories = {{/*id=*/"1", /*weight=*/1},
                                          {/*id=*/"2", /*weight=*/1}};
  std::vector<std::string> expected_deserialized_related_searches{
      "related searches", "búsquedas relacionadas"};

  EXPECT_EQ(deserialized_categories, expected_deserialized_categories);
  EXPECT_EQ(deserialized_related_searches,
            expected_deserialized_related_searches);
}

TEST_F(VisitAnnotationsDatabaseTest, AddClusters_UpdateVisitsInteractionState) {
  const std::vector<VisitID>& kSampleVisitIds = {3, 2, 5};
  auto clusters = CreateClusters({kSampleVisitIds});
  AddClusters(clusters);

  EXPECT_EQ(GetClusterVisit(kSampleVisitIds.front()).interaction_state,
            ClusterVisit::InteractionState::kDefault);

  UpdateVisitsInteractionState(kSampleVisitIds,
                               ClusterVisit::InteractionState::kDone);
  for (auto visit_id : kSampleVisitIds) {
    EXPECT_EQ(GetClusterVisit(visit_id).interaction_state,
              ClusterVisit::InteractionState::kDone);
  }
}

}  // namespace history
