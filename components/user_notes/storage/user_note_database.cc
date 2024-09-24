// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_notes/storage/user_note_database.h"

#include <string_view>

#include "base/files/file_util.h"
#include "base/json/values_util.h"
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
    : db_(sql::DatabaseOptions{.page_size = 4096, .cache_size = 128}),
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
    const UserNoteStorage::UrlSet& urls) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return UserNoteMetadataSnapshot();

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return UserNoteMetadataSnapshot();

  UserNoteMetadataSnapshot metadata_snapshot;
  for (const GURL& url : urls) {
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE,
                               "SELECT id, creation_date, modification_date "
                               "FROM notes WHERE url = ?"));

    if (!statement.is_valid())
      continue;

    statement.BindString(0, url.spec());

    while (statement.Step()) {
      DCHECK_EQ(3, statement.ColumnCount());

      std::string id = statement.ColumnString(0);
      std::string_view string_piece(id);
      uint64_t high = 0;
      uint64_t low = 0;
      if (!base::HexStringToUInt64(string_piece.substr(0, 16), &high) ||
          !base::HexStringToUInt64(string_piece.substr(16, 16), &low)) {
        continue;
      }
      std::optional<base::UnguessableToken> token =
          base::UnguessableToken::Deserialize(high, low);
      if (!token.has_value()) {
        continue;
      }

      base::Time creation_date = statement.ColumnTime(1);
      base::Time modification_date = statement.ColumnTime(2);

      auto metadata = std::make_unique<UserNoteMetadata>(
          creation_date, modification_date, /*min_note_version=*/1);
      metadata_snapshot.AddEntry(url, token.value(), std::move(metadata));
    }
  }

  transaction.Commit();

  return metadata_snapshot;
}

std::vector<std::unique_ptr<UserNote>> UserNoteDatabase::GetNotesById(
    const UserNoteStorage::IdSet& ids) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<std::unique_ptr<UserNote>> user_notes;

  if (!EnsureDBInit())
    return user_notes;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return user_notes;

  for (const base::UnguessableToken& id : ids) {
    auto user_note = GetNoteById(id);
    if (!user_note)
      continue;

    user_notes.emplace_back(std::move(user_note));
  }

  transaction.Commit();

  return user_notes;
}

std::unique_ptr<UserNote> UserNoteDatabase::GetNoteById(
    const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Get creation_date, modification_date, url and type from notes.
  sql::Statement statement_notes(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT creation_date, modification_date, url, "
                             "type FROM notes WHERE id = ?"));
  if (!statement_notes.is_valid())
    return nullptr;
  statement_notes.BindString(0, id.ToString());
  if (!statement_notes.Step())
    return nullptr;
  DCHECK_EQ(4, statement_notes.ColumnCount());
  base::Time creation_date = statement_notes.ColumnTime(0);
  base::Time modification_date = statement_notes.ColumnTime(1);
  std::string url = statement_notes.ColumnString(2);
  int type = statement_notes.ColumnInt(3);
  auto metadata = std::make_unique<UserNoteMetadata>(
      creation_date, modification_date, /*min_note_version=*/1);

  // Get plain_text from notes_body.
  sql::Statement statement_notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT plain_text FROM notes_body WHERE note_id = ?"));
  if (!statement_notes_body.is_valid())
    return nullptr;
  statement_notes_body.BindString(0, id.ToString());
  if (!statement_notes_body.Step())
    return nullptr;
  DCHECK_EQ(1, statement_notes_body.ColumnCount());
  auto body =
      std::make_unique<UserNoteBody>(statement_notes_body.ColumnString16(0));

  // Get original_text and selector from notes_text_target.
  sql::Statement statement_notes_text_target(
      db_.GetCachedStatement(SQL_FROM_HERE,
                             "SELECT original_text, selector FROM "
                             "notes_text_target WHERE note_id = ?"));
  if (!statement_notes_text_target.is_valid())
    return nullptr;
  statement_notes_text_target.BindString(0, id.ToString());
  if (!statement_notes_text_target.Step())
    return nullptr;
  DCHECK_EQ(2, statement_notes_text_target.ColumnCount());
  std::u16string original_text = statement_notes_text_target.ColumnString16(0);
  std::string selector = statement_notes_text_target.ColumnString(1);
  auto target = std::make_unique<UserNoteTarget>(
      static_cast<UserNoteTarget::TargetType>(type), original_text, GURL(url),
      selector);

  return std::make_unique<UserNote>(id, std::move(metadata), std::move(body),
                                    std::move(target));
}

bool UserNoteDatabase::CreateNote(std::unique_ptr<UserNote> model,
                                  std::u16string note_body_text) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

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
    return false;

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
    return false;

  sql::Statement notes_text_target(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO notes_text_target(note_id, original_text, selector) "
      "VALUES(?,?,?)"));
  if (!notes_text_target.is_valid())
    return false;

  notes_text_target.BindString(0, model->id().ToString());
  notes_text_target.BindString16(1, model->target().original_text());
  notes_text_target.BindString(2, model->target().selector());

  if (!notes_text_target.Run())
    return false;

  sql::Statement notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO notes_body(note_id, type, plain_text) "
      "VALUES(?,?,?)"));
  if (!notes_body.is_valid())
    return false;

  notes_body.BindString(0, model->id().ToString());
  notes_body.BindInt(1, UserNoteBody::BodyType::PLAIN_TEXT);
  notes_body.BindString16(2, note_body_text);

  if (!notes_body.Run())
    return false;

  transaction.Commit();
  return true;
}

bool UserNoteDatabase::UpdateNote(std::unique_ptr<UserNote> model,
                                  std::u16string note_body_text,
                                  bool is_creation) {
  if (is_creation) {
    return CreateNote(std::move(model), note_body_text);
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  // Only the text of the note body can be modified.
  // TODO(crbug.com/40832588): This will need to be updated if in the future we
  // wish to support changing the target text.
  sql::Statement update_notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE, "UPDATE notes_body SET plain_text = ? WHERE note_id = ?"));
  if (!update_notes_body.is_valid())
    return false;

  update_notes_body.BindString16(0, note_body_text);
  update_notes_body.BindString(1, model->id().ToString());

  if (!update_notes_body.Run())
    return false;

  sql::Statement update_modification_date(db_.GetCachedStatement(
      SQL_FROM_HERE, "UPDATE notes SET modification_date = ? WHERE id = ?"));
  if (!update_modification_date.is_valid())
    return false;

  update_modification_date.BindTime(0, base::Time::Now());
  update_modification_date.BindString(1, model->id().ToString());

  if (!update_modification_date.Run())
    return false;

  transaction.Commit();

  return true;
}

bool UserNoteDatabase::DeleteNoteWithStringId(std::string id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Statement delete_notes_body(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM notes_body WHERE note_id = ?"));

  if (!delete_notes_body.is_valid())
    return false;

  delete_notes_body.BindString(0, id);
  if (!delete_notes_body.Run())
    return false;

  sql::Statement delete_notes_text_target(db_.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM notes_text_target WHERE note_id = ?"));
  if (!delete_notes_text_target.is_valid())
    return false;

  delete_notes_text_target.BindString(0, id);
  if (!delete_notes_text_target.Run())
    return false;

  sql::Statement delete_notes(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM notes WHERE id = ?"));
  if (!delete_notes.is_valid())
    return false;

  delete_notes.BindString(0, id);
  if (!delete_notes.Run())
    return false;

  return true;
}

bool UserNoteDatabase::DeleteNote(const base::UnguessableToken& id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  bool is_notes_deleted = DeleteNoteWithStringId(id.ToString());
  transaction.Commit();
  return is_notes_deleted;
}

bool UserNoteDatabase::DeleteAllForUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM notes WHERE url = ?"));

  if (!statement.is_valid())
    return false;

  statement.BindString(0, url.spec());

  std::vector<std::string> ids;
  while (statement.Step())
    ids.emplace_back(statement.ColumnString(0));

  if (!statement.Succeeded())
    return false;

  for (const std::string& id : ids) {
    if (!DeleteNoteWithStringId(id))
      return false;
  }

  transaction.Commit();
  return true;
}

bool UserNoteDatabase::DeleteAllForOrigin(const url::Origin& page) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, "SELECT id FROM notes WHERE origin = ?"));

  if (!statement.is_valid())
    return false;

  statement.BindString(0, page.Serialize());

  std::vector<std::string> ids;
  while (statement.Step()) {
    ids.emplace_back(statement.ColumnString(0));
  }

  if (!statement.Succeeded())
    return false;

  for (const std::string& id : ids) {
    if (!DeleteNoteWithStringId(id))
      return false;
  }

  transaction.Commit();
  return true;
}

bool UserNoteDatabase::DeleteAllNotes() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!EnsureDBInit())
    return false;

  sql::Transaction transaction(&db_);
  if (!transaction.Begin())
    return false;

  sql::Statement delete_notes_body(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM notes_body"));

  if (!delete_notes_body.is_valid())
    return false;

  if (!delete_notes_body.Run())
    return false;

  sql::Statement delete_notes_text_target(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM notes_text_target"));
  if (!delete_notes_text_target.is_valid())
    return false;

  if (!delete_notes_text_target.Run())
    return false;

  sql::Statement delete_notes(
      db_.GetCachedStatement(SQL_FROM_HERE, "DELETE FROM notes"));
  if (!delete_notes.is_valid())
    return false;

  if (!delete_notes.Run())
    return false;

  transaction.Commit();
  return true;
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
  db_.RazeAndPoison();
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

  return meta_table.SetVersionNumber(kCurrentVersionNumber) &&
         meta_table.SetCompatibleVersionNumber(kCompatibleVersionNumber);
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
