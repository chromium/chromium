// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/content_annotations_table.h"

#include <memory>
#include <optional>

#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/time/time.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

class ContentAnnotationsTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<sql::Database>(sql::DatabaseOptions(),
                                          sql::test::kTestTag);
    ASSERT_TRUE(db_->Open(temp_dir_.GetPath().AppendASCII("test.db")));
    encryptor_.emplace(os_crypt_async::GetTestEncryptorForTesting());
    ASSERT_TRUE(table_.Init(db_.get(), &*encryptor_));
  }

  void TearDown() override { db_->Close(); }

  ContentAnnotationsData CreateDefaultTestData() {
    ContentAnnotationsData data;
    data.url = GURL("https://example.com");
    data.navigation_timestamp = base::Time::FromSecondsSinceUnixEpoch(1000);
    data.page_title = "Example Title";
    data.tab_id = 123;
    data.content_annotation.set_description("Test description");
    data.classifier_results.Set("category", "test_category");
    return data;
  }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> db_;
  std::optional<os_crypt_async::TestEncryptor> encryptor_;
  ContentAnnotationsTable table_;
};

// Tests that initialization fails if the input database is null.
TEST_F(ContentAnnotationsTableTest, InitNullDb) {
  ContentAnnotationsTable table;
  EXPECT_FALSE(table.Init(nullptr, &*encryptor_));
}

// Tests that initialization fails if the input encryptor is null.
TEST_F(ContentAnnotationsTableTest, InitNullEncryptor) {
  ContentAnnotationsTable table;
  EXPECT_FALSE(table.Init(db_.get(), nullptr));
}

// Tests that table creation immediately fails if the input database is null.
TEST_F(ContentAnnotationsTableTest, DoNotCreateTablesIfNullDatabase) {
  ContentAnnotationsTable table;
  EXPECT_FALSE(table.MigrateFromCleanStateToVersion1());
}

// Tests that table creation successfully creates the content annotations table.
TEST_F(ContentAnnotationsTableTest, Init) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());
  EXPECT_TRUE(db_->DoesTableExist("content_annotations"));
}

// Tests that `data` is successfully added and retrieved.
TEST_F(ContentAnnotationsTableTest, AddAndGetContentAnnotation) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());

  ContentAnnotationsData data = CreateDefaultTestData();
  history::VisitID visit_id(1);

  // Add the `data` to the table successfully.
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id, data));

  // Retrieve the table row for `visit_id` and verify its contents.
  std::optional<ContentAnnotationsData> retrieved =
      table_.GetContentAnnotation(visit_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->url, data.url);
  EXPECT_EQ(retrieved->navigation_timestamp, data.navigation_timestamp);
  EXPECT_EQ(retrieved->page_title, data.page_title);
  EXPECT_EQ(retrieved->tab_id, data.tab_id);
  EXPECT_THAT(retrieved->content_annotation,
              base::test::EqualsProto(data.content_annotation));
  EXPECT_EQ(retrieved->classifier_results, data.classifier_results);
}

// Tests that DeleteContentAnnotations successfully removes the rows for the given
// visit IDs.
TEST_F(ContentAnnotationsTableTest, DeleteContentAnnotations) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());

  history::VisitID visit_id_1(1);
  history::VisitID visit_id_2(2);
  history::VisitID visit_id_3(3);

  // Successfully add to the table.
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id_1, CreateDefaultTestData()));
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id_2, CreateDefaultTestData()));
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id_3, CreateDefaultTestData()));

  // Verify that content annotations are present in the table.
  EXPECT_EQ(table_.GetAllContentAnnotations().size(), 3u);

  // Successfully delete multiple rows.
  EXPECT_TRUE(table_.DeleteContentAnnotations({visit_id_1, visit_id_2}));

  EXPECT_EQ(table_.GetAllContentAnnotations().size(), 1u);
  EXPECT_FALSE(table_.GetContentAnnotation(visit_id_1).has_value());
  EXPECT_FALSE(table_.GetContentAnnotation(visit_id_2).has_value());
  EXPECT_TRUE(table_.GetContentAnnotation(visit_id_3).has_value());
}

// Tests that ClearAllContentAnnotations successfully removes all rows.
TEST_F(ContentAnnotationsTableTest, GetAndClearAllContentAnnotations) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());

  history::VisitID visit_id_1(1);
  history::VisitID visit_id_2(2);

  // Add two rows to the table successfully.
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id_1, CreateDefaultTestData()));
  EXPECT_TRUE(table_.AddContentAnnotation(visit_id_2, CreateDefaultTestData()));

  // Verify that there are two rows in the table and they have correct visit
  // IDs.
  auto all_annotations = table_.GetAllContentAnnotations();
  EXPECT_EQ(all_annotations.size(), 2u);
  EXPECT_EQ(all_annotations[0].first, visit_id_1);
  EXPECT_EQ(all_annotations[1].first, visit_id_2);

  EXPECT_TRUE(table_.ClearAllContentAnnotations());

  // Verify that the table is empty after clearing all rows.
  EXPECT_TRUE(table_.GetAllContentAnnotations().empty());
}

// Tests that the table can handle missing tab IDs.
TEST_F(ContentAnnotationsTableTest, AddAndGetContentAnnotationWithNoTabId) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());

  ContentAnnotationsData data = CreateDefaultTestData();
  data.tab_id = std::nullopt;
  history::VisitID visit_id(1);

  EXPECT_TRUE(table_.AddContentAnnotation(visit_id, data));

  std::optional<ContentAnnotationsData> retrieved =
      table_.GetContentAnnotation(visit_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_FALSE(retrieved->tab_id.has_value());
}

// Tests that all functions fail if the table is not initialized.
TEST_F(ContentAnnotationsTableTest, FunctionsFailWithoutInit) {
  ContentAnnotationsTable uninitialized_table;
  history::VisitID visit_id(1);
  ContentAnnotationsData data = CreateDefaultTestData();

  EXPECT_FALSE(uninitialized_table.MigrateFromCleanStateToVersion1());
  EXPECT_FALSE(uninitialized_table.AddContentAnnotation(visit_id, data));
  EXPECT_FALSE(uninitialized_table.GetContentAnnotation(visit_id).has_value());
  EXPECT_TRUE(uninitialized_table.GetAllContentAnnotations().empty());
  EXPECT_FALSE(uninitialized_table.DeleteContentAnnotations({visit_id}));
  EXPECT_FALSE(uninitialized_table.ClearAllContentAnnotations());
}

// Tests that GetContentAnnotation returns std::nullopt if the row doesn't
// exist.
TEST_F(ContentAnnotationsTableTest, GetNonExistentAnnotation) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());
  EXPECT_FALSE(table_.GetContentAnnotation(history::VisitID(999)).has_value());
}

// Tests that GetAllContentAnnotations returns an empty vector if the table is
// empty.
TEST_F(ContentAnnotationsTableTest, GetAllAnnotationsEmptyTable) {
  ASSERT_TRUE(table_.MigrateFromCleanStateToVersion1());
  // Ensure it's empty.
  ASSERT_TRUE(table_.ClearAllContentAnnotations());
  EXPECT_TRUE(table_.GetAllContentAnnotations().empty());
}

}  // namespace accessibility_annotator
