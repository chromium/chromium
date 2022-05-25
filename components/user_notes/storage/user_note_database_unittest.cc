// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_notes {

class UserNoteDatabaseTest : public testing::Test {
 public:
  UserNoteDatabaseTest() = default;

  void SetUp() override { ASSERT_TRUE(temp_directory_.CreateUniqueTempDir()); }
  void TearDown() override { EXPECT_TRUE(temp_directory_.Delete()); }

  base::FilePath db_dir() { return temp_directory_.GetPath(); }

  base::FilePath db_file_path() {
    return temp_directory_.GetPath().Append(kDatabaseName);
  }

 private:
  base::ScopedTempDir temp_directory_;
};

TEST_F(UserNoteDatabaseTest, InitDatabase) {
  EXPECT_FALSE(base::PathExists(db_file_path()));
  {
    std::unique_ptr<UserNoteDatabase> user_note_db =
        std::make_unique<UserNoteDatabase>(db_dir());

    EXPECT_FALSE(base::PathExists(db_file_path()));

    EXPECT_TRUE(user_note_db->Init());
    EXPECT_TRUE(base::PathExists(db_file_path()));
  }

  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    // Should have 4 tables and 5 indexes
    // tables - user_notes, user_notes_text_target, user_note_body, meta.
    // indexes - 1 implicit index for all 4 tables, url and origin index for
    // `notes` table.
    EXPECT_EQ(4u, sql::test::CountSQLTables(&db));
    EXPECT_EQ(6u, sql::test::CountSQLIndices(&db));
  }
}

TEST_F(UserNoteDatabaseTest, DatabaseNewVersion) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Create an empty database with a newer schema version (version=1000000).
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));

    sql::MetaTable meta_table;
    constexpr int kFutureVersionNumber = 1000000;
    EXPECT_TRUE(meta_table.Init(&db, /*version=*/kFutureVersionNumber,
                                /*compatible_version=*/kFutureVersionNumber));

    EXPECT_EQ(1u, sql::test::CountSQLTables(&db)) << db.GetSchema();
  }

  EXPECT_TRUE(base::PathExists(db_file_path()));
  // Calling Init DB with existing DB ahead of current version should fail.
  {
    std::unique_ptr<UserNoteDatabase> user_note_db =
        std::make_unique<UserNoteDatabase>(db_dir());
    EXPECT_FALSE(user_note_db->Init());
  }
}

TEST_F(UserNoteDatabaseTest, DatabaseHasSchemaNoMeta) {
  ASSERT_FALSE(base::PathExists(db_file_path()));

  // Init DB with all tables including meta.
  {
    std::unique_ptr<UserNoteDatabase> user_note_db =
        std::make_unique<UserNoteDatabase>(db_dir());
    EXPECT_TRUE(user_note_db->Init());
  }

  // Drop meta table.
  {
    sql::Database db;
    EXPECT_TRUE(db.Open(db_file_path()));
    sql::MetaTable::DeleteTableForTesting(&db);
  }

  // Init again with no meta should raze the DB and recreate again successfully.
  {
    std::unique_ptr<UserNoteDatabase> user_note_db =
        std::make_unique<UserNoteDatabase>(db_dir());
    EXPECT_TRUE(user_note_db->Init());
    // TODO(gayane): Start with an non-empty DB and check that here the DB is
    // empty.
  }
}

}  // namespace user_notes
