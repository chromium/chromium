// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include "base/cxx17_backports.h"
#include "base/test/gtest_util.h"
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/visit_database.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "visit_annotations_test_utils.h"

namespace history {

namespace {

using ::testing::ElementsAre;

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

  void VerifyRecentAnnotatedVisitIds(
      const std::vector<VisitID>& expected_visit_ids,
      base::Time time = base::Time::Min(),
      int max_results = 100) {
    EXPECT_EQ(GetRecentAnnotatedVisitIds(time, max_results),
              expected_visit_ids);
  }

  void ExpectContextAnnotations(VisitContextAnnotations actual,
                                VisitContextAnnotations expected) {
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
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kFlocEligibleRelaxed;
  VisitContentAnnotations content_annotations{annotation_flags,
                                              model_annotations};
  AddContentAnnotationsForVisit(visit_id, content_annotations);

  // Query for it.
  VisitContentAnnotations got_content_annotations;
  ASSERT_TRUE(
      GetContentAnnotationsForVisit(visit_id, &got_content_annotations));

  EXPECT_EQ(VisitContentAnnotationFlag::kFlocEligibleRelaxed,
            got_content_annotations.annotation_flags);
  EXPECT_EQ(0.5f,
            got_content_annotations.model_annotations.floc_protected_score);
  EXPECT_THAT(
      got_content_annotations.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/1, /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/2, /*weight=*/1)));
  EXPECT_EQ(
      123, got_content_annotations.model_annotations.page_topics_model_version);
}

TEST_F(VisitAnnotationsDatabaseTest,
       AddContextAnnotationsForVisit_GetAnnotatedVisit) {
  AddVisitWithTime(IntToTime(20), false);
  AddVisitWithTime(IntToTime(30), false);
  AddVisitWithTime(IntToTime(10), false);

  const std::vector<VisitContextAnnotations> visit_context_annotations_list = {
      {true, false, true, true, false, false},
      {false, true, false, false, false, true},
      {false, true, true, false, true, false},
  };

  // Verify `AddContextAnnotationsForVisit()` and `GetAnnotatedVisits()`.
  AddContextAnnotationsForVisit(1, visit_context_annotations_list[0]);
  AddContextAnnotationsForVisit(2, visit_context_annotations_list[1]);
  AddContextAnnotationsForVisit(3, visit_context_annotations_list[2]);

  for (size_t i = 0; i < base::size(visit_context_annotations_list); ++i) {
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

TEST_F(VisitAnnotationsDatabaseTest, UpdateContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentModelAnnotations model_annotations = {
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kFlocEligibleRelaxed;
  VisitContentAnnotations original{annotation_flags, model_annotations};
  AddContentAnnotationsForVisit(visit_id, original);

  // Mutate that row.
  VisitContentAnnotations modification(original);
  modification.model_annotations.floc_protected_score = 0.3f;
  UpdateContentAnnotationsForVisit(visit_id, modification);

  // Check that the mutated version was written.
  VisitContentAnnotations final;
  ASSERT_TRUE(GetContentAnnotationsForVisit(visit_id, &final));

  EXPECT_EQ(VisitContentAnnotationFlag::kFlocEligibleRelaxed,
            final.annotation_flags);
  EXPECT_EQ(0.3f, final.model_annotations.floc_protected_score);
  EXPECT_THAT(
      final.model_annotations.categories,
      ElementsAre(
          VisitContentModelAnnotations::Category(/*id=*/1, /*weight=*/1),
          VisitContentModelAnnotations::Category(/*id=*/2, /*weight=*/1)));
  EXPECT_EQ(123, final.model_annotations.page_topics_model_version);
}

TEST_F(VisitAnnotationsDatabaseTest,
       GetRecentAnnotatedVisitIds_GetRecentClusters) {
  // Shouldn't return old unclustered visits.
  AddVisitWithTime(IntToTime(10));
  // Shouldn't return old clustered visits.
  AddCluster(
      {AddVisitWithTime(IntToTime(11)), AddVisitWithTime(IntToTime(12))});
  // Should return recent unclustered visits.
  AddVisitWithTime(IntToTime(100));
  // Should return recent clustered visits.
  AddCluster(
      {AddVisitWithTime(IntToTime(101)), AddVisitWithTime(IntToTime(102))});
  // Shouldn't return old visits in recent clusters.
  AddCluster(
      {AddVisitWithTime(IntToTime(13)), AddVisitWithTime(IntToTime(103))});

  // Verify `GetRecentAnnotatedVisitIds()`.
  VerifyRecentAnnotatedVisitIds({8, 6, 5, 4}, IntToTime(100));
  // Verify `GetRecentAnnotatedVisitIds()` with `time`.
  VerifyRecentAnnotatedVisitIds({8, 7, 6, 5, 4, 3, 2, 1}, IntToTime(10));
  VerifyRecentAnnotatedVisitIds({}, IntToTime(104));
  // Verify `GetRecentAnnotatedVisitIds()` with `max_results`.
  VerifyRecentAnnotatedVisitIds({8, 7, 6}, IntToTime(10), 3);

  // Verify `GetRecentClusterIds()`.
  EXPECT_EQ(GetRecentClusterIds(IntToTime(100)), std::vector<int64_t>({3, 2}));
  EXPECT_EQ(GetRecentClusterIds(IntToTime(10)),
            std::vector<int64_t>({3, 2, 1}));
  EXPECT_EQ(GetRecentClusterIds(IntToTime(104)), std::vector<int64_t>({}));
}

TEST_F(VisitAnnotationsDatabaseTest,
       GetClusteredAnnotatedVisits_GetVisitsInCluster) {
  // Add unclustered visits.
  AddVisitWithTime(IntToTime(0));
  AddVisitWithTime(IntToTime(2));
  AddVisitWithTime(IntToTime(4));
  // Add clustered visits.
  AddCluster({AddVisitWithTime(IntToTime(1))});
  AddCluster({AddVisitWithTime(IntToTime(3))});
  AddCluster({AddVisitWithTime(IntToTime(5)), AddVisitWithTime(IntToTime(7))});

  EXPECT_THAT(GetVisitIds(GetClusteredAnnotatedVisits(10)),
              ElementsAre(7, 6, 5, 4));
  EXPECT_THAT(GetVisitIdsInCluster(1, 10), ElementsAre(4));
  EXPECT_THAT(GetVisitIdsInCluster(2, 0), ElementsAre());
  EXPECT_THAT(GetVisitIdsInCluster(3, 10), ElementsAre(7, 6));
  EXPECT_THAT(GetVisitIdsInCluster(3, 1), ElementsAre(7));
}

TEST_F(VisitAnnotationsDatabaseTest, DeleteAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentModelAnnotations model_annotations = {
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kNone;
  VisitContentAnnotations content_annotations{annotation_flags,
                                              model_annotations};
  AddContentAnnotationsForVisit(visit_id, content_annotations);

  VisitContentAnnotations got_content_annotations;
  // First make sure the annotations are there.
  EXPECT_TRUE(
      GetContentAnnotationsForVisit(visit_id, &got_content_annotations));

  // Delete annotations and make sure we cannot query for it.
  DeleteAnnotationsForVisit(visit_id);
  EXPECT_FALSE(
      GetContentAnnotationsForVisit(visit_id, &got_content_annotations));
}

TEST_F(VisitAnnotationsDatabaseTest, AddClusters_GetClusters_DeleteClusters) {
  const auto verify_clusters =
      [&](const std::vector<ClusterRow>& actual_clusters,
          const std::vector<ClusterRow>& expected_clusters) {
        ASSERT_EQ(actual_clusters.size(), expected_clusters.size());
        for (size_t i = 0; i < actual_clusters.size(); ++i) {
          SCOPED_TRACE(i);
          EXPECT_EQ(actual_clusters[i].cluster_id,
                    expected_clusters[i].cluster_id);
          EXPECT_EQ(actual_clusters[i].visit_ids,
                    expected_clusters[i].visit_ids);
        }
      };

  AddClusters(CreateClusters({{3, 2, 5}, {3, 2, 5}, {6}}));

  {
    SCOPED_TRACE("`GetClusters(10)`");
    verify_clusters(GetClusters(10),
                    {CreateClusterRow(1, {5, 3, 2}),
                     CreateClusterRow(2, {5, 3, 2}), CreateClusterRow(3, {6})});
  }
  {
    SCOPED_TRACE("`GetClusters(5)`");
    verify_clusters(GetClusters(5), {CreateClusterRow(1, {5, 3, 2}),
                                     CreateClusterRow(2, {5, 3})});
  }
  {
    SCOPED_TRACE("`GetClusters(3)`");
    verify_clusters(GetClusters(3), {CreateClusterRow(1, {5, 3, 2})});
  }
  {
    SCOPED_TRACE("`GetClusters(1)`");
    verify_clusters(GetClusters(1), {CreateClusterRow(1, {5})});
  }
}

}  // namespace history
