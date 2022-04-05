// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

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
      annotation_flags, model_annotations, related_searches,
      GURL("http://pagewithvisit.com?q=search"), u"search"};
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
      annotation_flags, model_annotations, related_searches,
      GURL("http://pagewithvisit.com?q=search"), u"search"};
  AddContentAnnotationsForVisit(visit_id, original);

  // Mutate that row.
  VisitContentAnnotations modification(original);
  modification.model_annotations.visibility_score = 0.3f;
  modification.related_searches.emplace_back("búsquedas relacionadas");
  modification.search_normalized_url =
      GURL("http://pagewithvisit.com?q=search2");
  modification.search_terms = u"search2";
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

TEST_F(VisitAnnotationsDatabaseTest, GetVisitsInCluster) {
  // Add unclustered visits.
  AddVisitWithTime(IntToTime(0));
  AddVisitWithTime(IntToTime(2));
  AddVisitWithTime(IntToTime(4));
  // Add clustered visits.
  AddCluster({AddVisitWithTime(IntToTime(1))});
  AddCluster({AddVisitWithTime(IntToTime(3))});
  AddCluster({AddVisitWithTime(IntToTime(5)), AddVisitWithTime(IntToTime(7))});

  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(4));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(7, 6));
}

TEST_F(VisitAnnotationsDatabaseTest, DeleteAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentModelAnnotations model_annotations = {
      0.5f,
      {{/*id=*/"1", /*weight=*/1}, {/*id=*/"2", /*weight=*/1}},
      123,
      {{/*id=*/"entity1", /*weight=*/1}, {/*id=*/"entity2", /*weight=*/1}}};
  std::vector<std::string> related_searches{"related searches",
                                            "búsquedas relacionadas"};
  VisitContentAnnotationFlags annotation_flags =
      VisitContentAnnotationFlag::kNone;
  VisitContentAnnotations content_annotations{
      annotation_flags, model_annotations, related_searches,
      GURL("http://pagewithvisit.com?q=search"), u"search"};
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

TEST_F(VisitAnnotationsDatabaseTest, AddClusters_DeleteClusters) {
  AddClusters(CreateClusters({{3, 2, 5}, {3, 2, 5}, {6}}));

  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(6));

  DeleteClusters({});

  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre(6));

  DeleteClusters({1, 3, 4});

  EXPECT_THAT(GetVisitIdsInCluster(1), ElementsAre());
  EXPECT_THAT(GetVisitIdsInCluster(2), ElementsAre(5, 3, 2));
  EXPECT_THAT(GetVisitIdsInCluster(3), ElementsAre());
  EXPECT_THAT(GetVisitIdsInCluster(4), ElementsAre());
}

}  // namespace history
