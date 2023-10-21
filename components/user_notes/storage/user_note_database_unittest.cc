// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include <vector>

#include "base/containers/contains.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/path_service.h"
#include "base/ranges/algorithm.h"
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

  void check_is_removed_from_db(UserNoteDatabase* user_note_db,
                                const base::UnguessableToken& id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db->sequence_checker_);

    sql::Statement statement_notes_body(user_note_db->db_.GetCachedStatement(
        SQL_FROM_HERE, "SELECT note_id FROM notes_body WHERE note_id = ?"));
    EXPECT_TRUE(statement_notes_body.is_valid());
    statement_notes_body.BindString(0, id.ToString());
    EXPECT_FALSE(statement_notes_body.Step());

    sql::Statement statement_notes(user_note_db->db_.GetCachedStatement(
        SQL_FROM_HERE, "SELECT id FROM notes WHERE id = ?"));
    EXPECT_TRUE(statement_notes.is_valid());
    statement_notes.BindString(0, id.ToString());
    EXPECT_FALSE(statement_notes.Step());

    sql::Statement statement_notes_text_target(
        user_note_db->db_.GetCachedStatement(
            SQL_FROM_HERE,
            "SELECT note_id FROM notes_text_target WHERE note_id = ?"));
    EXPECT_TRUE(statement_notes_text_target.is_valid());
    statement_notes_text_target.BindString(0, id.ToString());
    EXPECT_FALSE(statement_notes_text_target.Step());
  }

  void check_notes_body_from_db(UserNoteDatabase* user_note_db,
                                const base::UnguessableToken& id,
                                const std::u16string& text) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db->sequence_checker_);

    sql::Statement statement(user_note_db->db_.GetCachedStatement(
        SQL_FROM_HERE, "SELECT plain_text FROM notes_body WHERE note_id = ?"));

    EXPECT_TRUE(statement.is_valid());
    statement.BindString(0, id.ToString());
    EXPECT_TRUE(statement.Step());

    EXPECT_EQ(1, statement.ColumnCount());
    EXPECT_EQ(text, statement.ColumnString16(0));
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

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  bool create_note =
      user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                              /*is_creation=*/true);
  EXPECT_TRUE(create_note);

  check_notes_body_from_db(&user_note_db, note_id, u"new test note");
  delete user_note;
}

TEST_F(UserNoteDatabaseTest, UpdateNote) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  bool create_note =
      user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                              /*is_creation=*/true);
  bool update_note = user_note_db.UpdateNote(UserNote::Clone(user_note),
                                             u"edit test note", false);
  EXPECT_TRUE(create_note);
  EXPECT_TRUE(update_note);

  check_notes_body_from_db(&user_note_db, note_id, u"edit test note");
  delete user_note;
}

TEST_F(UserNoteDatabaseTest, DeleteNote) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());

  base::UnguessableToken note_id = base::UnguessableToken::Create();
  UserNote* user_note =
      new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                   GetTestUserNotePageTarget());

  bool create_note =
      user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                              /*is_creation=*/true);
  EXPECT_TRUE(create_note);
  bool delete_note = user_note_db.DeleteNote(note_id);
  EXPECT_TRUE(delete_note);

  check_is_removed_from_db(&user_note_db, note_id);
  delete user_note;
}

TEST_F(UserNoteDatabaseTest, GetNotesById) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db.sequence_checker_);

  UserNoteStorage::IdSet id_set;
  std::vector<base::UnguessableToken> ids;
  for (int i = 0; i < 3; i++) {
    base::UnguessableToken note_id = base::UnguessableToken::Create();
    ids.emplace_back(note_id);
    id_set.emplace(note_id);
    std::u16string original_text =
        u"original text " + base::NumberToString16(i);
    std::string selector = "selector " + base::NumberToString(i);
    std::u16string body = u"new test note " + base::NumberToString16(i);
    GURL url = GURL("https://www.test.com/");
    auto test_target = std::make_unique<UserNoteTarget>(
        UserNoteTarget::TargetType::kPageText, original_text, url, selector);
    UserNote* user_note =
        new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                     std::move(test_target));
    bool create_note = user_note_db.UpdateNote(UserNote::Clone(user_note), body,
                                               /*is_creation=*/true);
    EXPECT_TRUE(create_note);
    delete user_note;
  }

  std::vector<std::unique_ptr<UserNote>> notes =
      user_note_db.GetNotesById(id_set);
  EXPECT_EQ(3u, notes.size());

  for (std::unique_ptr<UserNote>& note : notes) {
    const auto& vector_it = base::ranges::find(ids, note->id());
    EXPECT_NE(vector_it, ids.end());
    EXPECT_NE(id_set.find(note->id()), id_set.end());
    int i = vector_it - ids.begin();
    EXPECT_EQ("https://www.test.com/", note->target().target_page().spec());
    EXPECT_EQ(u"original text " + base::NumberToString16(i),
              note->target().original_text());
    EXPECT_EQ("selector " + base::NumberToString(i), note->target().selector());
    EXPECT_EQ(u"new test note " + base::NumberToString16(i),
              note->body().plain_text_value());
    EXPECT_EQ(UserNoteTarget::TargetType::kPageText, note->target().type());
  }
}

TEST_F(UserNoteDatabaseTest, DeleteAllNotes) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());

  UserNoteStorage::IdSet ids;
  for (int i = 0; i < 3; i++) {
    base::UnguessableToken note_id = base::UnguessableToken::Create();
    ids.emplace(note_id);
    UserNote* user_note =
        new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                     GetTestUserNotePageTarget());
    bool create_note =
        user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                                /*is_creation=*/true);
    EXPECT_TRUE(create_note);
    delete user_note;
  }
  bool delete_notes = user_note_db.DeleteAllNotes();
  EXPECT_TRUE(delete_notes);

  for (const base::UnguessableToken& id : ids) {
    check_is_removed_from_db(&user_note_db, id);
  }
}

TEST_F(UserNoteDatabaseTest, DeleteAllForOrigin) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());

  UserNoteStorage::IdSet ids;
  for (int i = 0; i < 3; i++) {
    base::UnguessableToken note_id = base::UnguessableToken::Create();
    ids.emplace(note_id);
    UserNote* user_note =
        new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                     GetTestUserNotePageTarget("https://www.test.com"));
    bool create_note =
        user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                                /*is_creation=*/true);
    EXPECT_TRUE(create_note);
    delete user_note;
  }

  bool delete_notes = user_note_db.DeleteAllForOrigin(
      url::Origin::Create(GURL("https://www.test.com")));
  EXPECT_TRUE(delete_notes);

  for (const base::UnguessableToken& id : ids) {
    check_is_removed_from_db(&user_note_db, id);
  }
}

TEST_F(UserNoteDatabaseTest, DeleteAllForUrl) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());

  UserNoteStorage::IdSet ids;
  for (int i = 0; i < 3; i++) {
    base::UnguessableToken note_id = base::UnguessableToken::Create();
    ids.emplace(note_id);
    UserNote* user_note =
        new UserNote(note_id, GetTestUserNoteMetadata(), GetTestUserNoteBody(),
                     GetTestUserNotePageTarget("https://www.test.com"));
    bool create_note =
        user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                                /*is_creation=*/true);
    EXPECT_TRUE(create_note);
    delete user_note;
  }
  bool delete_notes =
      user_note_db.DeleteAllForUrl(GURL("https://www.test.com"));
  EXPECT_TRUE(delete_notes);

  for (const base::UnguessableToken& id : ids) {
    check_is_removed_from_db(&user_note_db, id);
  }
}

TEST_F(UserNoteDatabaseTest, GetNoteMetadataForUrls) {
  UserNoteDatabase user_note_db(db_dir());
  EXPECT_TRUE(user_note_db.Init());
  DCHECK_CALLED_ON_VALID_SEQUENCE(user_note_db.sequence_checker_);

  UserNoteStorage::IdSet ids;
  base::Time time = base::Time::FromSecondsSinceUnixEpoch(1600000000);
  int note_version = 1;
  for (int i = 0; i < 3; i++) {
    base::UnguessableToken note_id = base::UnguessableToken::Create();
    ids.emplace(note_id);
    auto note_metadata =
        std::make_unique<UserNoteMetadata>(time, time, note_version);
    UserNote* user_note =
        new UserNote(note_id, std::move(note_metadata), GetTestUserNoteBody(),
                     GetTestUserNotePageTarget("https://www.test.com"));
    bool create_note =
        user_note_db.UpdateNote(UserNote::Clone(user_note), u"new test note",
                                /*is_creation=*/true);
    EXPECT_TRUE(create_note);
    delete user_note;
  }

  GURL url1 = GURL("https://www.test.com");
  GURL url2 = GURL("https://www.test.com");
  GURL url3 = GURL("https://www.test.com/2");
  UserNoteStorage::UrlSet urls{url1, url2, url3};
  UserNoteMetadataSnapshot metadata_snapshot =
      user_note_db.GetNoteMetadataForUrls(urls);
  const UserNoteMetadataSnapshot::IdToMetadataMap* metadata_map =
      metadata_snapshot.GetMapForUrl(url1);
  EXPECT_EQ(3u, metadata_map->size());
  EXPECT_EQ(3u, metadata_snapshot.GetMapForUrl(url2)->size());
  EXPECT_EQ(nullptr, metadata_snapshot.GetMapForUrl(url3));

  for (const auto& metadata_it : *metadata_map) {
    EXPECT_TRUE(base::Contains(ids, metadata_it.first));
    EXPECT_EQ(time, metadata_it.second->creation_date());
    EXPECT_EQ(time, metadata_it.second->modification_date());
    EXPECT_EQ(note_version, metadata_it.second->min_note_version());
  }
}

}  // namespace user_notes
