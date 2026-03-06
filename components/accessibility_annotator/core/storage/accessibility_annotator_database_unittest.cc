// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

#include <memory>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorDatabaseTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<AccessibilityAnnotatorDatabase>();
  }

 protected:
  base::FilePath GetDbPath() const {
    return temp_dir_.GetPath().AppendASCII("TestDB");
  }

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AccessibilityAnnotatorDatabase> db_;
};

// Tests that all migrations/initialization from an empty database succeed.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeEmptyToCurrent) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(db_->Init(GetDbPath()));
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
    EXPECT_TRUE(connection.DoesTableExist("entities"));
  }
}

// Tests that all initialization from an existing database succeed.
TEST_F(AccessibilityAnnotatorDatabaseTest, InitializeWithExistingDatabase) {
  // Initialize the database from an empty database.
  ASSERT_TRUE(db_->Init(GetDbPath()));
  db_.reset();

  // Re-initialize the database.
  AccessibilityAnnotatorDatabase db;
  EXPECT_TRUE(db.Init(GetDbPath()));
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
  EXPECT_TRUE(db_->Init(GetDbPath()));

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

  EXPECT_FALSE(db_->Init(GetDbPath()));
}

}  // namespace accessibility_annotator
