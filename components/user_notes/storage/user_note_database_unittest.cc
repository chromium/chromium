// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "components/user_notes/model/user_note_model_test_utils.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
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

    // Should have 4 tables and 6 indexes
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

TEST_F(UserNoteDatabaseTest, CreateNote) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db.sequence_checker_);

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  user_note_db.UpdateNote(user_note, "new test note", /*is_creation=*/true);

  sql::Statement statement(user_note_db.db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT plain_text FROM notes_body WHERE note_id = ?"));

  EXPECT_TRUE(statement.is_valid());
  statement.BindString(0, note_id.ToString());
  EXPECT_TRUE(statement.Step());

  EXPECT_EQ(1, statement.ColumnCount());
  EXPECT_EQ("new test note", statement.ColumnString(0));
  delete user_note;
}

TEST_F(UserNoteDatabaseTest, UpdateNote) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db.sequence_checker_);

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  user_note_db.UpdateNote(user_note, "new test note", /*is_creation=*/true);
  user_note_db.UpdateNote(user_note, "edit test note", false);

  sql::Statement statement(user_note_db.db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT plain_text FROM notes_body WHERE note_id = ?"));

  EXPECT_TRUE(statement.is_valid());
  statement.BindString(0, note_id.ToString());
  EXPECT_TRUE(statement.Step());

  EXPECT_EQ(1, statement.ColumnCount());
  EXPECT_EQ("edit test note", statement.ColumnString(0));
  delete user_note;
}

TEST_F(UserNoteDatabaseTest, DeleteNote) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db.sequence_checker_);

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  user_note_db.UpdateNote(user_note, "new test note", /*is_creation=*/true);
  user_note_db.DeleteNote(note_id);

  sql::Statement statement_notes_body(user_note_db.db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT note_id FROM notes_body WHERE note_id = ?"));
  EXPECT_TRUE(statement_notes_body.is_valid());
  statement_notes_body.BindString(0, note_id.ToString());
  EXPECT_FALSE(statement_notes_body.Step());

  sql::Statement statement_notes(user_note_db.db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM notes WHERE id = ?"));
  EXPECT_TRUE(statement_notes.is_valid());
  statement_notes.BindString(0, note_id.ToString());
  EXPECT_FALSE(statement_notes.Step());

  sql::Statement statement_notes_text_target(
      user_note_db.db_.GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT note_id FROM notes_text_target WHERE note_id = ?"));
  EXPECT_TRUE(statement_notes_text_target.is_valid());
  statement_notes_text_target.BindString(0, note_id.ToString());
  EXPECT_FALSE(statement_notes_text_target.Step());
  delete user_note;
}

}  // namespace user_notes
