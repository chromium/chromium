// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_database.h"

#include <string>

#include "base/bind.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/page_transition_types.h"


// Helpers --------------------------------------------------------------------

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 1;
const int kCompatibleVersionNumber = 1;

void BindShortcutToStatement(const ShortcutsDatabase::Shortcut& shortcut,
                             sql::Statement* s) {
  DCHECK(base::IsValidGUID(shortcut.id));
  s->BindString(0, shortcut.id);
  s->BindString16(1, shortcut.text);
  s->BindString16(2, shortcut.match_core.fill_into_edit);
  s->BindString(3, shortcut.match_core.destination_url.spec());
  s->BindString16(4, shortcut.match_core.contents);
  s->BindString(5, shortcut.match_core.contents_class);
  s->BindString16(6, shortcut.match_core.description);
  s->BindString(7, shortcut.match_core.description_class);
  s->BindInt(8, shortcut.match_core.transition);
  s->BindInt(9, shortcut.match_core.type);
  s->BindString16(10, shortcut.match_core.keyword);
  s->BindInt64(11, shortcut.last_access_time.ToInternalValue());
  s->BindInt(12, shortcut.number_of_hits);
}

bool DeleteShortcut(const char* field_name,
                    const std::string& id,
                    sql::Database& db) {
  sql::Statement s(db.GetUniqueStatement(
      base::StringPrintf("DELETE FROM omni_box_shortcuts WHERE %s = ?",
                         field_name).c_str()));
  s.BindString(0, id);
  return s.Run();
}

void DatabaseErrorCallback(sql::Database* db,
                           const base::FilePath& db_path,
                           int extended_error,
                           sql::Statement* stmt) {
  if (sql::Recovery::ShouldRecover(extended_error)) {
    // Prevent reentrant calls.
    db->reset_error_callback();

    // After this call, the |db| handle is poisoned so that future calls will
    // return errors until the handle is re-opened.
    sql::Recovery::RecoverDatabase(db, db_path);

    // The DLOG(FATAL) below is intended to draw immediate attention to errors
    // in newly-written code.  Database corruption is generally a result of OS
    // or hardware issues, not coding errors at the client level, so displaying
    // the error would probably lead to confusion.  The ignored call signals the
    // test-expectation framework that the error was handled.
    ignore_result(sql::Database::IsExpectedSqliteError(extended_error));
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

}  // namespace

// ShortcutsDatabase::Shortcut::MatchCore -------------------------------------

ShortcutsDatabase::Shortcut::MatchCore::MatchCore(
    const base::string16& fill_into_edit,
    const GURL& destination_url,
    const base::string16& contents,
    const std::string& contents_class,
    const base::string16& description,
    const std::string& description_class,
    int transition,
    int type,
    const base::string16& keyword)
    : fill_into_edit(fill_into_edit),
      destination_url(destination_url),
      contents(contents),
      contents_class(contents_class),
      description(description),
      description_class(description_class),
      transition(transition),
      type(type),
      keyword(keyword) {
}

ShortcutsDatabase::Shortcut::MatchCore::MatchCore(const MatchCore& other) =
    default;

ShortcutsDatabase::Shortcut::MatchCore::~MatchCore() {
}

// ShortcutsDatabase::Shortcut ------------------------------------------------

ShortcutsDatabase::Shortcut::Shortcut(
    const std::string& id,
    const base::string16& text,
    const MatchCore& match_core,
    const base::Time& last_access_time,
    int number_of_hits)
    : id(id),
      text(text),
      match_core(match_core),
      last_access_time(last_access_time),
      number_of_hits(number_of_hits) {
}

ShortcutsDatabase::Shortcut::Shortcut()
    : match_core(base::string16(), GURL(), base::string16(), std::string(),
                 base::string16(), std::string(), 0, 0, base::string16()),
      last_access_time(base::Time::Now()),
      number_of_hits(0) {
}

ShortcutsDatabase::Shortcut::Shortcut(const Shortcut& other) = default;

ShortcutsDatabase::Shortcut::~Shortcut() {
}


// ShortcutsDatabase ----------------------------------------------------------

ShortcutsDatabase::ShortcutsDatabase(const base::FilePath& database_path)
    : database_path_(database_path) {
}

bool ShortcutsDatabase::Init() {
  db_.set_histogram_tag("Shortcuts");

  // To recover from corruption.
  db_.set_error_callback(
      base::Bind(&DatabaseErrorCallback, &db_, database_path_));

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Run the database in exclusive mode. Nobody else should be accessing the
  // database while we're running, and this will give somewhat improved perf.
  db_.set_exclusive_locking();

  // Attach the database to our index file.
  return db_.Open(database_path_) && EnsureTable();
}

bool ShortcutsDatabase::AddShortcut(const Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT INTO omni_box_shortcuts (id, text, fill_into_edit, url, "
          "contents, contents_class, description, description_class, "
          "transition, type, keyword, last_access_time, number_of_hits) "
          "VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?)"));
  BindShortcutToStatement(shortcut, &s);
  return s.Run();
}

bool ShortcutsDatabase::UpdateShortcut(const Shortcut& shortcut) {
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "UPDATE omni_box_shortcuts SET id=?, text=?, fill_into_edit=?, url=?, "
          "contents=?, contents_class=?, description=?, description_class=?, "
          "transition=?, type=?, keyword=?, last_access_time=?, "
          "number_of_hits=? WHERE id=?"));
  BindShortcutToStatement(shortcut, &s);
  s.BindString(13, shortcut.id);
  return s.Run();
}

bool ShortcutsDatabase::DeleteShortcutsWithIDs(
    const ShortcutIDs& shortcut_ids) {
  bool success = true;
  db_.BeginTransaction();
  for (auto it(shortcut_ids.begin()); it != shortcut_ids.end(); ++it) {
    success &= DeleteShortcut("id", *it, db_);
  }
  db_.CommitTransaction();
  return success;
}

bool ShortcutsDatabase::DeleteShortcutsWithURL(
    const std::string& shortcut_url_spec) {
  return DeleteShortcut("url", shortcut_url_spec, db_);
}

bool ShortcutsDatabase::DeleteAllShortcuts() {
  if (!db_.Execute("DELETE FROM omni_box_shortcuts"))
    return false;

  ignore_result(db_.Execute("VACUUM"));
  return true;
}

void ShortcutsDatabase::LoadShortcuts(GuidToShortcutMap* shortcuts) {
  DCHECK(shortcuts);
  sql::Statement s(db_.GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT id, text, fill_into_edit, url, contents, contents_class, "
          "description, description_class, transition, type, keyword, "
          "last_access_time, number_of_hits FROM omni_box_shortcuts"));

  shortcuts->clear();
  while (s.Step()) {
    shortcuts->insert(std::make_pair(
        s.ColumnString(0),
        Shortcut(
            s.ColumnString(0),            // id
            s.ColumnString16(1),          // text
            Shortcut::MatchCore(
                s.ColumnString16(2),      // fill_into_edit
                GURL(s.ColumnString(3)),  // destination_url
                s.ColumnString16(4),      // contents
                s.ColumnString(5),        // contents_class
                s.ColumnString16(6),      // description
                s.ColumnString(7),        // description_class
                s.ColumnInt(8),           // transition
                s.ColumnInt(9),           // type
                s.ColumnString16(10)),    // keyword
            base::Time::FromInternalValue(s.ColumnInt64(11)),
                                          // last_access_time
            s.ColumnInt(12))));           // number_of_hits
  }
}

ShortcutsDatabase::~ShortcutsDatabase() {
}

bool ShortcutsDatabase::EnsureTable() {
  if (!db_.DoesTableExist("omni_box_shortcuts")) {
    return db_.Execute(
        "CREATE TABLE omni_box_shortcuts (id VARCHAR PRIMARY KEY, "
            "text VARCHAR, fill_into_edit VARCHAR, url VARCHAR, "
            "contents VARCHAR, contents_class VARCHAR, description VARCHAR, "
            "description_class VARCHAR, transition INTEGER, type INTEGER, "
            "keyword VARCHAR, last_access_time INTEGER, "
            "number_of_hits INTEGER)");
  }

  // The first version of the shortcuts table lacked the fill_into_edit,
  // transition, type, and keyword columns.
  if (!db_.DoesColumnExist("omni_box_shortcuts", "fill_into_edit")) {
    // Perform the upgrade in a transaction to ensure it doesn't happen
    // incompletely.
    sql::Transaction transaction(&db_);
    if (!(transaction.Begin() &&
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN fill_into_edit VARCHAR") &&
        db_.Execute("UPDATE omni_box_shortcuts SET fill_into_edit = url") &&
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN transition INTEGER") &&
        db_.Execute(base::StringPrintf(
            "UPDATE omni_box_shortcuts SET transition = %d",
            static_cast<int>(ui::PAGE_TRANSITION_TYPED)).c_str()) &&
        db_.Execute("ALTER TABLE omni_box_shortcuts ADD COLUMN type INTEGER") &&
        db_.Execute(base::StringPrintf(
            "UPDATE omni_box_shortcuts SET type = %d",
            static_cast<int>(AutocompleteMatchType::HISTORY_TITLE)).c_str()) &&
        db_.Execute("ALTER TABLE omni_box_shortcuts "
            "ADD COLUMN keyword VARCHAR") &&
        transaction.Commit())) {
      return false;
    }
  }

  if (!sql::MetaTable::DoesTableExist(&db_)) {
    sql::Transaction transaction(&db_);
    if (!(transaction.Begin() &&
          meta_table_.Init(
              &db_, kCurrentVersionNumber, kCompatibleVersionNumber) &&
          // Migrate old SEARCH_OTHER_ENGINE values to the new type value.
          db_.Execute(
              "UPDATE omni_box_shortcuts SET type = 13 WHERE type = 9") &&
          // Migrate old EXTENSION_APP values to the new type value.
          db_.Execute(
              "UPDATE omni_box_shortcuts SET type = 14 WHERE type = 10") &&
          // Migrate old CONTACT values to the new type value.
          db_.Execute(
              "UPDATE omni_box_shortcuts SET type = 15 WHERE type = 11") &&
          // Migrate old BOOKMARK_TITLE values to the new type value.
          db_.Execute(
              "UPDATE omni_box_shortcuts SET type = 16 WHERE type = 12") &&
          transaction.Commit())) {
      return false;
    }
  }
  return true;
}
