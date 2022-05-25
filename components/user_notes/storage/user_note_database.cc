// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include "base/files/file_util.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace user_notes {

namespace {

// `kCurrentVersionNumber` and `kCompatibleVersionNumber` are used for DB
// migrations. Update both accordingly when changing the schema.
// Version 1 - 2021-04 - Initial Schema - https://crrev.com/c/3546500
const int kCurrentVersionNumber = 1;

const int kCompatibleVersionNumber = 1;

}  // namespace

UserNoteDatabase::UserNoteDatabase(const base::FilePath& path_to_database_dir)
    : db_(sql::DatabaseOptions{.exclusive_locking = true,
                               .page_size = 4096,
                               .cache_size = 128}),
      db_file_path_(path_to_database_dir.Append(kDatabaseName)) {}

UserNoteDatabase::~UserNoteDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool UserNoteDatabase::Init() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (db_.is_open()) {
    return true;
  }

  // Use of Unretained is safe as sql::Database will only run the callback while
  // it's alive. As UserNoteDatabase instance owns the sql::Database it's
  // guaranteed that the UserNoteDatabase will be alive when the callback is
  // run.
  db_.set_error_callback(base::BindRepeating(
      &UserNoteDatabase::DatabaseErrorCallback, base::Unretained(this)));
  db_.set_histogram_tag("UserNotes");

  const base::FilePath dir = db_file_path_.DirName();
  if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
    DLOG(ERROR) << "Failed to create directory for user notes database";
    return false;
  }

  if (!db_.Open(db_file_path_)) {
    DLOG(ERROR) << "Failed to open user notes database: "
                << db_.GetErrorMessage();
    return false;
  }

  if (!InitSchema()) {
    DLOG(ERROR) << "Failed to create schema for user notes database: "
                << db_.GetErrorMessage();
    db_.Close();
    return false;
  }
  return true;
}

UserNoteMetadataSnapshot UserNoteDatabase::GetNoteMetadataForUrls(
    std::vector<GURL> urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.

  return UserNoteMetadataSnapshot();
}

std::vector<std::unique_ptr<UserNote>> UserNoteDatabase::GetNotesById(
    std::vector<base::UnguessableToken> ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.

  return std::vector<std::unique_ptr<UserNote>>();
}

void UserNoteDatabase::CreateNote(base::UnguessableToken id,
                                  std::string note_body_text,
                                  UserNoteTarget::TargetType target_type,
                                  std::string original_text,
                                  GURL target_page,
                                  std::string selector) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::UpdateNote(base::UnguessableToken id,
                                  std::string note_body_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteNote(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllForUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllForOrigin(const url::Origin& page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DeleteAllNotes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(gayane): Implement.
}

void UserNoteDatabase::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sql::IsErrorCatastrophic(error)) {
    return;
  }

  // Ignore repeated callbacks.
  db_.reset_error_callback();

  // After this call, the `db_` handle is poisoned so that future calls will
  // return errors until the handle is re-opened.
  db_.RazeAndClose();
}

bool UserNoteDatabase::InitSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::MetaTable meta_table;
  bool has_metatable = meta_table.DoesTableExist(&db_);
  bool has_schema = db_.DoesTableExist("notes");
  if (!has_metatable && has_schema) {
    // Existing DB with no meta table. Cannot determine DB version.
    db_.Raze();
  }

  // Create the meta table if it doesn't exist.
  if (!meta_table.Init(&db_, kCurrentVersionNumber, kCompatibleVersionNumber)) {
    return false;
  }

  // If DB and meta table already existed and current version is not compatible
  // with DB then it should fail.
  if (meta_table.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    return false;
  }
  if (!has_schema) {
    return CreateSchema();
  }

  meta_table.SetVersionNumber(kCurrentVersionNumber);
  meta_table.SetCompatibleVersionNumber(kCompatibleVersionNumber);
  return true;
}

bool UserNoteDatabase::CreateSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  // `id` is the primary key of the table.
  // `creation_date` The date and time in seconds when the row was created.
  // `modification_date` The date and time in seconds when the row was last
  //  modified.
  // `url` The URL of the target page.
  // `type` The type of target this note has (0-page, 1-page text).
  // clang-format off
  static constexpr char kUserNotesTableSql[] =
      "CREATE TABLE IF NOT EXISTS notes("
          "id TEXT PRIMARY KEY NOT NULL,"
          "creation_date INTEGER NOT NULL,"
          "modification_date INTEGER NOT NULL,"
          "url TEXT NOT NULL,"
          "origin TEXT NOT NULL,"
          "type INTEGER NOT NULL)";
  // clang-format on
  if (!db_.Execute(kUserNotesTableSql)) {
    return false;
  }

  // Optimizes user note look up by url.
  // clang-format off
  static constexpr char kUserNoteByUrlIndexSql[] =
      "CREATE INDEX IF NOT EXISTS notes_by_url "
          "ON notes(url)";
  // clang-format on
  if (!db_.Execute(kUserNoteByUrlIndexSql)) {
    return false;
  }

  // Optimizes user note look up by origin.
  // clang-format off
  static constexpr char kUserNoteByOriginIndexSql[] =
      "CREATE INDEX IF NOT EXISTS notes_by_origin "
          "ON notes(origin)";
  // clang-format on
  if (!db_.Execute(kUserNoteByOriginIndexSql)) {
    return false;
  }

  // `note_id` is the primary key of the table. Matches the `id` of
  // corresponding note in `notes` table.
  // `original_text` The original text to which the note was attached.
  // `selector` The text fragment selector that identifies the target text.
  // clang-format off
  static constexpr char kUserNotesTextTargetTableSql[] =
      "CREATE TABLE IF NOT EXISTS notes_text_target("
          "note_id TEXT PRIMARY KEY NOT NULL,"
          "original_text TEXT NOT NULL,"
          "selector TEXT NOT NULL)";
  // clang-format on
  if (!db_.Execute(kUserNotesTextTargetTableSql)) {
    return false;
  }

  // `note_id` is the primary key of the table. Matches the `id` of
  // corresponding note in `notes` table.
  // `type` The type of body this note has (only plain text is currently
  // supported). `plain_text` The note body in plain text.
  // clang-format off
  static constexpr char kUserNotesBodyTableSql[] =
      "CREATE TABLE IF NOT EXISTS notes_body("
          "note_id TEXT PRIMARY KEY NOT NULL,"
          "type INTEGER NOT NULL,"
          "plain_text TEXT)";
  // clang-format on
  if (!db_.Execute(kUserNotesBodyTableSql)) {
    return false;
  }

  return transaction.Commit();
}
}  // namespace user_notes
