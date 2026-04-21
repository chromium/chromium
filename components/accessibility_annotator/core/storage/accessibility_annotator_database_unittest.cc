// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<AccessibilityAnnotatorDatabase>();
  }

  ContentAnnotationsData CreateTestData() {
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
  base::FilePath GetDbPath() const {
    return temp_dir_.GetPath().AppendASCII("TestDB");
  }

  base::ScopedTempDir temp_dir_;
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kAccessibilityAnnotatorDatabaseStorage};
  std::unique_ptr<AccessibilityAnnotatorDatabase> db_;
};

// Tests that all migrations/initialization from an empty database succeed.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeEmptyToCurrent) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
  db_.reset();

  // Verify post-conditions.  These are expectations for current version of the
  // database.
  {
    sql::Database connection(sql::test::kTestTag);
    ASSERT_TRUE(connection.Open(GetDbPath()));

    sql::Statement get_user_version_stm(
        connection.GetUniqueStatement("PRAGMA user_version"));
    ASSERT_TRUE(get_user_version_stm.is_valid());
    ASSERT_TRUE(get_user_version_stm.Step());
    int detected_user_version = get_user_version_stm.ColumnInt(0);
    EXPECT_EQ(AccessibilityAnnotatorDatabase::kCurrentVersionNumber,
              detected_user_version);

    // Check that expected tables are present.
    EXPECT_TRUE(connection.DoesTableExist("content_annotations"));
  }
}

// Tests that all initialization from an existing database succeed.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeWithExistingDatabase) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
  db_.reset();

  // Re-initialize the database.
  AccessibilityAnnotatorDatabase db;
  EXPECT_TRUE(
      db.Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
}

// Tests that not a SQLite file is handled by deleting and recreating the
// database.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeWithCorruptFile) {
  // Create a non-SQLite file at the database path.
  ASSERT_TRUE(base::WriteFile(GetDbPath(), "This is not a SQLite file"));

  sql::test::ScopedErrorExpecter expecter;
  expecter.ExpectError(static_cast<int>(sql::SqliteResultCode::kNotADatabase));

  // Initialize the database. This should detect the corrupt file, delete it,
  // and create a new one.
  EXPECT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));

  EXPECT_TRUE(expecter.SawExpectedErrors());
}

// Tests that all migrations/initialization from an newer database no-ops.
TEST_F(AccessibilityAnnotatorDatabaseTest,
       InitializeGreaterVersionThanCurrent) {
  // Set the user-version to a version greater than the current version.
  sql::Database connection(sql::test::kTestTag);
  ASSERT_TRUE(connection.Open(GetDbPath()));
  ASSERT_TRUE(connection.Execute("PRAGMA user_version=1000000"));
  connection.Close();

  EXPECT_FALSE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
}

// Tests that initialization fails if the feature flag is disabled.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeWithFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kAccessibilityAnnotatorDatabaseStorage);

  EXPECT_FALSE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
}

// Tests that calling methods before initialization fails and returns the
// expected default values.
TEST_F(AccessibilityAnnotatorDatabaseTest,
       CallContentAnnotationMethodsBeforeInit) {
  history::VisitID visit_id(1);

  EXPECT_FALSE(db_->AddContentAnnotation(visit_id, CreateTestData()));
  EXPECT_FALSE(db_->GetContentAnnotation(visit_id).has_value());
  EXPECT_TRUE(db_->GetAllContentAnnotations().empty());
  EXPECT_FALSE(db_->DeleteContentAnnotation(visit_id));
  EXPECT_FALSE(db_->ClearAllContentAnnotations());
}

// Tests adding and retrieving content annotations.
TEST_F(AccessibilityAnnotatorDatabaseTest, AddAndGetContentAnnotation) {
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));

  ContentAnnotationsData data = CreateTestData();
  history::VisitID visit_id(1);

  // Successfully add the content annotation to the database.
  EXPECT_TRUE(db_->AddContentAnnotation(visit_id, data));

  // Retrieve the content annotation from the database and verify its contents.
  std::optional<ContentAnnotationsData> retrieved =
      db_->GetContentAnnotation(visit_id);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->url, data.url);
  EXPECT_EQ(retrieved->navigation_timestamp, data.navigation_timestamp);
  EXPECT_EQ(retrieved->page_title, data.page_title);
  EXPECT_EQ(retrieved->tab_id, data.tab_id);
  EXPECT_THAT(retrieved->content_annotation,
              base::test::EqualsProto(data.content_annotation));
  EXPECT_EQ(retrieved->classifier_results, data.classifier_results);
}

// Tests retrieving a non-existent content annotation.
TEST_F(AccessibilityAnnotatorDatabaseTest, GetNonExistentContentAnnotation) {
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));

  EXPECT_FALSE(db_->GetContentAnnotation(999).has_value());
}

// Tests retrieving and clearing all content annotations.
TEST_F(AccessibilityAnnotatorDatabaseTest, GetAndClearAllContentAnnotations) {
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));

  history::VisitID visit_id_1(1);
  history::VisitID visit_id_2(2);

  // Add two content annotations to the database successfully.
  EXPECT_TRUE(db_->AddContentAnnotation(visit_id_1, CreateTestData()));
  EXPECT_TRUE(db_->AddContentAnnotation(visit_id_2, CreateTestData()));

  // Verify that both content annotations are retrieved from the database.
  std::vector<std::pair<history::VisitID, ContentAnnotationsData>>
      all_annotations = db_->GetAllContentAnnotations();
  ASSERT_EQ(all_annotations.size(), 2u);
  EXPECT_EQ(all_annotations[0].first, visit_id_1);
  EXPECT_EQ(all_annotations[1].first, visit_id_2);

  // Clear all content annotations from the database successfully.
  EXPECT_TRUE(db_->ClearAllContentAnnotations());

  // Verify that the database is empty after clearing all content annotations.
  EXPECT_TRUE(db_->GetAllContentAnnotations().empty());
}

// Tests deleting a content annotation.
TEST_F(AccessibilityAnnotatorDatabaseTest, DeleteContentAnnotation) {
  ASSERT_TRUE(
      db_->Init(GetDbPath(), os_crypt_async::GetTestEncryptorForTesting()));
  history::VisitID visit_id(1);

  // Add the content annotation to the database successfully.
  EXPECT_TRUE(db_->AddContentAnnotation(visit_id, CreateTestData()));

  // Verify that the content annotation is present in the database.
  EXPECT_TRUE(db_->GetContentAnnotation(visit_id).has_value());

  // Delete the content annotation from the database successfully and verify
  // that it is no longer present.
  EXPECT_TRUE(db_->DeleteContentAnnotation(visit_id));
  EXPECT_FALSE(db_->GetContentAnnotation(visit_id).has_value());
}

}  // namespace accessibility_annotator
