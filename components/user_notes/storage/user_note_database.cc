// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include "base/files/file_util.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
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

void UserNoteDatabase::CreateNote(const UserNote* model,
                                  std::string note_body_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  sql::Statement create_note(db_.GetCachedStatement(SQL_FROM_HERE,
                                                    "INSERT INTO notes("
                                                    "id,"
                                                    "creation_date,"
                                                    "modification_date,"
                                                    "url,"
                                                    "origin,"
                                                    "type)"
                                                    "VALUES(?,?,?,?,?,?)"));

  if (!create_note.is_valid())
    return;

  // TODO: possibly the time should be passed to this function, for example for
  // sync to add notes with past creation date.
  create_note.BindString(0, model->id().ToString());
  create_note.BindTime(1, model->metadata().creation_date());
  create_note.BindTime(2, model->metadata().modification_date());
  create_note.BindString(3, model->target().target_page().spec());
  create_note.BindString(
      4, url::Origin::Create(model->target().target_page()).Serialize());
  create_note.BindInt(5, model->target().type());

  if (!create_note.Run())
    return;

  if (model->target().type() == UserNoteTarget::TargetType::kPageText) {
    sql::Statement notes_text_target(db_.GetCachedStatement(
        SQL_FROM_HERE,
        "INSERT INTO notes_text_target(note_id, original_text, selector) "
        "VALUES(?,?,?)"));
    if (!notes_text_target.is_valid())
      return;

    notes_text_target.BindString(0, model->id().ToString());
    notes_text_target.BindString(1, model->target().original_text());
    notes_text_target.BindString(2, model->target().selector());

    if (!notes_text_target.Run())
      return;
  }

  sql::Statement notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO notes_body(note_id, type, plain_text) "
      "VALUES(?,?,?)"));
  if (!notes_body.is_valid())
    return;

  notes_body.BindString(0, model->id().ToString());
  notes_body.BindInt(1, UserNoteBody::BodyType::PLAIN_TEXT);
  notes_body.BindString(2, note_body_text);

  if (!notes_body.Run())
    return;

  transaction.Commit();
}

void UserNoteDatabase::UpdateNote(const UserNote* model,
                                  std::string note_body_text,
                                  bool is_creation) {
  if (is_creation) {
    CreateNote(model, note_body_text);
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  // Only the text of the note body can be modified.
  // TODO(crbug.com/1313967): This will need to be updated if in the future we
  // wish to support changing the target text.
  sql::Statement update_notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE, "UPDATE notes_body SET plain_text = ? WHERE note_id = ?"));
  if (!update_notes_body.is_valid())
    return;

  update_notes_body.BindString(0, note_body_text);
  update_notes_body.BindString(1, model->id().ToString());

  if (!update_notes_body.Run())
    return;

  sql::Statement update_modification_date(db_.GetCachedStatement(
      SQL_FROM_HERE, "UPDATE notes SET modification_date = ? WHERE id = ?"));
  if (!update_modification_date.is_valid())
    return;

  update_modification_date.BindTime(0, base::Time::Now());
  update_modification_date.BindString(1, model->id().ToString());

  if (!update_modification_date.Run())
    return;

  transaction.Commit();
}

void UserNoteDatabase::DeleteNote(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return;

  sql::Statement delete_notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM notes_body WHERE note_id = ?"));

  if (!delete_notes_body.is_valid())
    return;

  delete_notes_body.BindString(0, id.ToString());
  if (!delete_notes_body.Run())
    return;

  sql::Statement delete_notes_text_target(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM notes_text_target WHERE note_id = ?"));
  if (!delete_notes_text_target.is_valid())
    return;

  delete_notes_text_target.BindString(0, id.ToString());
  if (!delete_notes_text_target.Run())
    return;

  sql::Statement delete_notes(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM notes WHERE id = ?"));
  if (!delete_notes.is_valid())
    return;

  delete_notes.BindString(0, id.ToString());
  if (!delete_notes.Run())
    return;

  transaction.Commit();
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

bool UserNoteDatabase::EnsureDBInit() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (db_.is_open())
    return true;
  return Init();
}

void UserNoteDatabase::DatabaseErrorCallback(int error, sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!sql::IsErrorCatastrophic(error))
    return;

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
