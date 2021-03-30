// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/visit_annotations_database.h"

#include "base/files/scoped_temp_dir.h"
#include "sql/database.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace history {

namespace {

using ::testing::ElementsAre;

}  // namespace

class VisitAnnotationsDatabaseTest : public testing::Test,
                                     public VisitAnnotationsDatabase {
 public:
  VisitAnnotationsDatabaseTest() = default;
  ~VisitAnnotationsDatabaseTest() override = default;

 private:
  // Test setup.
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    base::FilePath db_file =
        temp_dir_.GetPath().AppendASCII("VisitAnnotationsTest.db");

    ASSERT_TRUE(db_.Open(db_file));

    // Initialize the tables for this test.
    InitVisitAnnotationsTables();
  }
  void TearDown() override { db_.Close(); }

  // VisitAnnotationsDatabase:
  sql::Database& GetDB() override { return db_; }

  base::ScopedTempDir temp_dir_;
  sql::Database db_;
};

TEST_F(VisitAnnotationsDatabaseTest, AddContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentAnnotations content_annotations = {
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  EXPECT_TRUE(AddContentAnnotationsForVisit(visit_id, content_annotations));

  // Query for it.
  base::Optional<VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForVisit(visit_id);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_EQ(0.5f, got_content_annotations->floc_protected_score);
  EXPECT_THAT(
      got_content_annotations->categories,
      ElementsAre(VisitContentAnnotations::Category(/*id=*/1, /*weight=*/1),
                  VisitContentAnnotations::Category(/*id=*/2, /*weight=*/1)));
  EXPECT_EQ(123, got_content_annotations->page_topics_model_version);
}

TEST_F(VisitAnnotationsDatabaseTest, UpdateContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentAnnotations original = {
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  EXPECT_TRUE(AddContentAnnotationsForVisit(visit_id, original));

  // Mutate that row.
  VisitContentAnnotations modification(original);
  modification.floc_protected_score = 0.3f;
  EXPECT_TRUE(UpdateContentAnnotationsForVisit(visit_id, modification));

  // Check that the mutated version was written.
  base::Optional<VisitContentAnnotations> final =
      GetContentAnnotationsForVisit(visit_id);
  ASSERT_TRUE(final.has_value());
  EXPECT_EQ(0.3f, final->floc_protected_score);
  EXPECT_THAT(
      final->categories,
      ElementsAre(VisitContentAnnotations::Category(/*id=*/1, /*weight=*/1),
                  VisitContentAnnotations::Category(/*id=*/2, /*weight=*/1)));
  EXPECT_EQ(123, final->page_topics_model_version);
}

TEST_F(VisitAnnotationsDatabaseTest, DeleteContentAnnotationsForVisit) {
  // Add content annotations for 1 visit.
  VisitID visit_id = 1;
  VisitContentAnnotations content_annotations = {
      0.5f, {{/*id=*/1, /*weight=*/1}, {/*id=*/2, /*weight=*/1}}, 123};
  EXPECT_TRUE(AddContentAnnotationsForVisit(visit_id, content_annotations));

  // First make sure the annotations are there.
  EXPECT_TRUE(GetContentAnnotationsForVisit(visit_id).has_value());

  // Delete annotations and make sure we cannot query for it.
  EXPECT_TRUE(DeleteContentAnnotationsForVisit(visit_id));
  EXPECT_FALSE(GetContentAnnotationsForVisit(visit_id).has_value());
}

}  // namespace history
