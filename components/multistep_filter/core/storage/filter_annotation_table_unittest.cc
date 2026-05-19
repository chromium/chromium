// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/multistep_filter/core/storage/filter_annotation_table.h"

#include <vector>

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/multistep_filter/core/data_models/filter_annotation.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace multistep_filter {

namespace {

using testing::ElementsAre;
using testing::SizeIs;

constexpr size_t kMaxCount = 10;

class FilterAnnotationTableTest : public testing::Test {
 public:
  FilterAnnotationTable* table() { return &table_; }
  sql::Database* db() { return &db_; }

  void SetUp() override {
    ASSERT_TRUE(db_.OpenInMemory());
    ASSERT_TRUE(table_.Init(&db_));
  }

 protected:
  base::test::TaskEnvironment task_environment_;

  sql::Database db_{sql::DatabaseOptions{}, sql::test::kTestTag};
  FilterAnnotationTable table_;
};

TEST_F(FilterAnnotationTableTest, InitCreatesTables) {
  EXPECT_TRUE(db()->DoesTableExist("filter_annotations"));
  EXPECT_TRUE(db()->DoesTableExist("filter_annotation_attributes"));
}

TEST_F(FilterAnnotationTableTest, StoreAndRetrieveAnnotation) {
  base::Uuid id = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes;
  attributes.emplace_back("key1", "value1");
  attributes.emplace_back("key2", "value2");
  FilterAnnotation annotation(id, "task1", "example.com", base::Time::Now(),
                              attributes);

  ASSERT_TRUE(table()->StoreAnnotation(annotation));

  std::vector<FilterAnnotation> annotations =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time());
  ASSERT_THAT(annotations, SizeIs(1));

  EXPECT_EQ(annotations.front(), annotation);
}

TEST_F(FilterAnnotationTableTest,
       GetAnnotationsForTaskSortedByCreationTimestamp_FiltersByTaskType) {
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation1(id1, "task1", "example.com", base::Time::Now(),
                               {});
  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation2(id2, "task2", "example.com", base::Time::Now(),
                               {});

  ASSERT_TRUE(table()->StoreAnnotation(annotation1));
  ASSERT_TRUE(table()->StoreAnnotation(annotation2));

  std::vector<FilterAnnotation> annotations =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time());
  ASSERT_THAT(annotations, SizeIs(1));

  EXPECT_EQ(annotations.front(), annotation1);
}

TEST_F(FilterAnnotationTableTest,
       GetAnnotationsForTaskSortedByCreationTimestamp_SortsByTimestamp) {
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation1(id1, "task1", "example1.com",
                               base::Time::FromTimeT(100), {});
  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation2(id2, "task1", "example2.com",
                               base::Time::FromTimeT(200), {});
  base::Uuid id3 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation3(id3, "task1", "example3.com",
                               base::Time::FromTimeT(150), {});

  ASSERT_TRUE(table()->StoreAnnotation(annotation1));
  ASSERT_TRUE(table()->StoreAnnotation(annotation2));
  ASSERT_TRUE(table()->StoreAnnotation(annotation3));

  std::vector<FilterAnnotation> annotations =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time());
  ASSERT_THAT(annotations, SizeIs(3));

  EXPECT_THAT(annotations, ElementsAre(annotation2, annotation3, annotation1));
}

TEST_F(FilterAnnotationTableTest,
       GetAnnotationsForTaskSortedByCreationTimestamp_FiltersByCreationTime) {
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation1(id1, "task1", "example1.com",
                               base::Time::FromTimeT(100), {});
  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation2(id2, "task1", "example2.com",
                               base::Time::FromTimeT(200), {});

  ASSERT_TRUE(table()->StoreAnnotation(annotation1));
  ASSERT_TRUE(table()->StoreAnnotation(annotation2));

  // Retrieve annotations created after t=150. Should only get annotation2.
  std::vector<FilterAnnotation> annotations =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time::FromTimeT(150));
  ASSERT_THAT(annotations, SizeIs(1));

  EXPECT_EQ(annotations.front(), annotation2);
}
TEST_F(FilterAnnotationTableTest,
       StoreAnnotation_OverwritesExistingAnnotationForSameTaskAndDomain) {
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes1;
  attributes1.emplace_back("key1", "value1");
  FilterAnnotation annotation1(id1, "task1", "example.com", base::Time::Now(),
                               attributes1);

  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  std::vector<FilterAttribute> attributes2;
  attributes2.emplace_back("key2", "value2");
  FilterAnnotation annotation2(id2, "task1", "example.com",
                               base::Time::Now() + base::Seconds(1),
                               attributes2);

  ASSERT_TRUE(table()->StoreAnnotation(annotation1));
  ASSERT_TRUE(table()->StoreAnnotation(annotation2));

  std::vector<FilterAnnotation> annotations =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time());
  ASSERT_THAT(annotations, SizeIs(1));

  EXPECT_EQ(annotations.front(), annotation2);
}

TEST_F(FilterAnnotationTableTest,
       StoreAnnotation_DoesNotOverwriteForDifferentTaskOrDomain) {
  base::Uuid id1 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation1(id1, "task1", "example.com", base::Time::Now(),
                               {});
  base::Uuid id2 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation2(id2, "task2", "example.com", base::Time::Now(),
                               {});
  base::Uuid id3 = base::Uuid::GenerateRandomV4();
  FilterAnnotation annotation3(id3, "task1", "other.com", base::Time::Now(),
                               {});

  ASSERT_TRUE(table()->StoreAnnotation(annotation1));
  ASSERT_TRUE(table()->StoreAnnotation(annotation2));
  ASSERT_TRUE(table()->StoreAnnotation(annotation3));

  std::vector<FilterAnnotation> annotations1 =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task1", kMaxCount, base::Time());
  ASSERT_THAT(annotations1, SizeIs(2));

  std::vector<FilterAnnotation> annotations2 =
      table()->GetAnnotationsForTaskSortedByCreationTimestamp(
          "task2", kMaxCount, base::Time());
  ASSERT_THAT(annotations2, SizeIs(1));
}

}  // namespace

}  // namespace multistep_filter
