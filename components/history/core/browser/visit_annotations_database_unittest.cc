// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history/core/browser/visit_database.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

using ::testing::ElementsAre;

// Returns a Time that's `seconds` seconds after Windows epoch.
base::Time IntToTime(int seconds) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromSeconds(seconds));
}

}  // namespace

class VisitAnnotationsDatabaseTest : public testing::Test,
                                     public VisitAnnotationsDatabase,
                                     public VisitDatabase {
 public:
  VisitAnnotationsDatabaseTest() = default;
  ~VisitAnnotationsDatabaseTest() override = default;

 protected:
  // Convenience wrapper for  `VisitDatabase::AddVisit()`.
  void AddVisitWithDetails(URLID url_id, base::Time visit_time) {
    VisitRow visit_row;
    visit_row.url_id = url_id;
    visit_row.visit_time = visit_time;
    AddVisit(&visit_row, VisitSource::SOURCE_BROWSED);
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

TEST_F(VisitAnnotationsDatabaseTest, AddGetAndDeleteContextAnnotations) {
  AddVisitWithDetails(1, IntToTime(20));
  AddVisitWithDetails(1, IntToTime(30));
  AddVisitWithDetails(2, IntToTime(10));

  // Verify `AddContextAnnotationsForVisit()` and `GetAnnotatedVisits()`.
  AddContextAnnotationsForVisit(1, {true});   // Ordered 2nd
  AddContextAnnotationsForVisit(2, {false});  // Ordered 1st
  AddContextAnnotationsForVisit(3, {false});  // Ordered 3rd

  std::vector<AnnotatedVisitRow> rows = GetAnnotatedVisits(10);
  ASSERT_EQ(rows.size(), 3u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[2].visit_id, 3);
  EXPECT_FALSE(rows[2].context_annotations.omnibox_url_copied);

  // Verify `max_results` param of `GetAnnotatedVisits()`.
  rows = GetAnnotatedVisits(2);
  ASSERT_EQ(rows.size(), 2u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
  EXPECT_EQ(rows[1].visit_id, 1);
  EXPECT_TRUE(rows[1].context_annotations.omnibox_url_copied);

  // Verify `DeleteAnnotationsForVisit()`.
  DeleteAnnotationsForVisit(1);
  DeleteAnnotationsForVisit(3);

  rows = GetAnnotatedVisits(10);
  ASSERT_EQ(rows.size(), 1u);
  EXPECT_EQ(rows[0].visit_id, 2);
  EXPECT_FALSE(rows[0].context_annotations.omnibox_url_copied);
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

}  // namespace history
