// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_
#define COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_

#include <unordered_map>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/user_notes/interfaces/user_note_metadata_snapshot.h"
#include "components/user_notes/interfaces/user_note_storage.h"
#include "components/user_notes/model/user_note.h"
#include "sql/database.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace user_notes {

constexpr base::FilePath::CharType kDatabaseName[] =
    FILE_PATH_LITERAL("UserNotes.db");

// Provides the backend SQLite support for user notes.
// This class must be used on a same blocking sequence.
class UserNoteDatabase {
 public:
  explicit UserNoteDatabase(const base::FilePath& path_to_database_dir);
  ~UserNoteDatabase();

  // Initialises internal database. Must be called prior to any other usage.
  bool Init();

  UserNoteMetadataSnapshot GetNoteMetadataForUrls(
      const UserNoteStorage::UrlSet& urls);

  std::vector<std::unique_ptr<UserNote>> GetNotesById(
      const UserNoteStorage::IdSet& ids);

  bool UpdateNote(std::unique_ptr<UserNote> model,
                  std::u16string note_body_text,
                  bool is_creation);

  bool DeleteNote(const base::UnguessableToken& id);

  bool DeleteAllForUrl(const GURL& url);

  bool DeleteAllForOrigin(const url::Origin& origin);

  bool DeleteAllNotes();

 private:
  FRIEND_TEST_ALL_PREFIXES(UserNoteDatabaseTest, GetNotesById);
  FRIEND_TEST_ALL_PREFIXES(UserNoteDatabaseTest, GetNoteMetadataForUrls);
  friend class UserNoteDatabaseTest;

  // Initialises internal database if needed.
  bool EnsureDBInit();

  // Called by the database to report errors.
  void DatabaseErrorCallback(int error, sql::Statement* stmt);

  // Creates or migrates to the new schema if needed.
  bool InitSchema();

  // Called by UpdateNote() with is_creation=true to create a new note.
  bool CreateNote(std::unique_ptr<UserNote> model,
                  std::u16string note_body_text);

  bool CreateSchema();

  bool DeleteNoteWithStringId(std::string id);

  std::unique_ptr<UserNote> GetNoteById(const base::UnguessableToken& id);

  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  const base::FilePath db_file_path_;

  SEQUENCE_CHECKER(sequence_checker_);
};
}  // namespace user_notes

#endif  // COMPONENTS_USER_NOTES_STORAGE_USER_NOTE_DATABASE_H_
