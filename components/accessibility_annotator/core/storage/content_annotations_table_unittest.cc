// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/content_annotations_table.h"

#include <memory>

#include "base/files/scoped_temp_dir.h"
#include "components/os_crypt/async/browser/test_utils.h"
#include "sql/database.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class ContentAnnotationsTableTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    db_ = std::make_unique<sql::Database>(sql::DatabaseOptions(),
                                          sql::test::kTestTag);
    ASSERT_TRUE(db_->Open(temp_dir_.GetPath().AppendASCII("test.db")));
    encryptor_.emplace(os_crypt_async::GetTestEncryptorForTesting());
  }

  void TearDown() override { db_->Close(); }

 protected:
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<sql::Database> db_;
  std::optional<os_crypt_async::TestEncryptor> encryptor_;
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
  EXPECT_FALSE(table.CreateTablesIfNecessary());
}

// Tests that table creation successfully creates the content annotations table
// if it was missing.
TEST_F(ContentAnnotationsTableTest, Init) {
  ContentAnnotationsTable table;
  ASSERT_TRUE(table.Init(db_.get(), &*encryptor_));
  ASSERT_TRUE(table.CreateTablesIfNecessary());

  EXPECT_TRUE(db_->DoesTableExist("content_annotations"));
}

// Tests that table creation succeeds if table already exists.
TEST_F(ContentAnnotationsTableTest,
       DoNotSignalFailureToCreateTablesIfAllAlreadyExist) {
  ContentAnnotationsTable table;
  ASSERT_TRUE(table.Init(db_.get(), &*encryptor_));

  // Create placeholder tables (schema doesn't matter)
  ASSERT_TRUE(db_->Execute(
      R"SQL(CREATE TABLE content_annotations (
              id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL))SQL"));
  ASSERT_TRUE(db_->DoesTableExist("content_annotations"));

  EXPECT_TRUE(table.CreateTablesIfNecessary());
}

}  // namespace accessibility_annotator
