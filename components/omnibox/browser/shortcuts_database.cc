// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_database.h"

#include <string>
#include <tuple>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/common/omnibox_features.h"
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
const int kCurrentVersionNumber = 2;
const int kCompatibleVersionNumber = 1;

void BindShortcutToStatement(const ShortcutsDatabase::Shortcut& shortcut,
                             sql::Statement& s) {
  DCHECK(base::Uuid::ParseCaseInsensitive(shortcut.id).is_valid());
  s.BindString(0, shortcut.id);
  s.BindString16(1, shortcut.text);
  s.BindString16(2, shortcut.match_core.fill_into_edit);
  s.BindString(3, shortcut.match_core.destination_url.spec());
  s.BindInt(4, base::checked_cast<int>(shortcut.match_core.document_type));
  s.BindString16(5, shortcut.match_core.contents);
  s.BindString(6, shortcut.match_core.contents_class);
  s.BindString16(7, shortcut.match_core.description);
  s.BindString(8, shortcut.match_core.description_class);
  s.BindInt(9, base::checked_cast<int>(shortcut.match_core.transition));
  s.BindInt(10, base::checked_cast<int>(shortcut.match_core.type));
  s.BindString16(11, shortcut.match_core.keyword);
  s.BindTime(12, shortcut.last_access_time);
  s.BindInt(13, shortcut.number_of_hits);
}

void DatabaseErrorCallback(sql::Database* db,
                           int extended_error,
                           sql::Statement* stmt) {
  // Attempt to recover a corrupt database, if it is eligible to be recovered.
  if (sql::Recovery::RecoverIfPossible(
          db, extended_error, sql::Recovery::Strategy::kRecoverOrRaze)) {
    // Recovery was attempted. The database handle has been poisoned and the
    // error callback has been reset.

    // Signal the test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db->GetErrorMessage();
}

}  // namespace

// ShortcutsDatabase::Shortcut::MatchCore -------------------------------------

ShortcutsDatabase::Shortcut::MatchCore::MatchCore(
    const std::u16string& fill_into_edit,
    const GURL& destination_url,
    AutocompleteMatch::DocumentType document_type,
    const std::u16string& contents,
    const std::string& contents_class,
    const std::u16string& description,
    const std::string& description_class,
    ui::PageTransition transition,
    AutocompleteMatchType::Type type,
    const std::u16string& keyword)
    : fill_into_edit(fill_into_edit),
      destination_url(destination_url),
      document_type(document_type),
      contents(contents),
      contents_class(contents_class),
      description(description),
      description_class(description_class),
      transition(transition),
      type(type),
      keyword(keyword) {}

ShortcutsDatabase::Shortcut::MatchCore::MatchCore(const MatchCore&) = default;

ShortcutsDatabase::Shortcut::MatchCore::~MatchCore() = default;

// ShortcutsDatabase::Shortcut ------------------------------------------------

ShortcutsDatabase::Shortcut::Shortcut(const std::string& id,
                                      const std::u16string& text,
                                      const MatchCore& match_core,
                                      const base::Time& last_access_time,
                                      int number_of_hits)
    : id(id),
      text(text),
      match_core(match_core),
      last_access_time(last_access_time),
      number_of_hits(number_of_hits) {}

ShortcutsDatabase::Shortcut::Shortcut()
    : match_core(std::u16string(),
                 GURL(),
                 AutocompleteMatch::DocumentType::NONE,
                 std::u16string(),
                 std::string(),
                 std::u16string(),
                 std::string(),
                 ui::PageTransition::PAGE_TRANSITION_FIRST,
                 // AutocompleteMatchType doesn't have a sentinel or null value,
                 // so we just use the value equal to 0. This constructor is
                 // only used by STL anyways, so this is harmless.
                 AutocompleteMatchType::Type::URL_WHAT_YOU_TYPED,
                 std::u16string()),
      last_access_time(base::Time::Now()),
      number_of_hits(0) {}

ShortcutsDatabase::Shortcut::Shortcut(const Shortcut&) = default;

ShortcutsDatabase::Shortcut::~Shortcut() = default;

// ShortcutsDatabase ----------------------------------------------------------

ShortcutsDatabase::ShortcutsDatabase(const base::FilePath& database_path)
    : db_({// Set the database page size to something a little larger to give us
           // better performance (we're typically seek rather than bandwidth
           // limited). Must be a power of 2 and a max of 65536.
           .page_size = 4096,
           .cache_size = 500}),
      database_path_(database_path) {}

bool ShortcutsDatabase::Init() {
  db_.set_histogram_tag("Shortcuts");

  if (!db_.has_error_callback()) {
    // The error callback may be reset if recovery was attempted, so ensure the
    // callback is re-set when the database is re-opened.
    db_.set_error_callback(base::BindRepeating(&DatabaseErrorCallback, &db_));
  }

  // Attach the database to our index file.
  return db_.Open(database_path_) && EnsureTable();
}

bool ShortcutsDatabase::AddShortcut(const Shortcut& shortcut) {
  static constexpr char kInsertSql[] =
      // clang-format off
      "INSERT INTO omni_box_shortcuts("
        "id,text,fill_into_edit,url,document_type,contents,contents_class,"
        "description,description_class,transition,type,keyword,"
        "last_access_time,number_of_hits)"
      "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?)";
  // clang-format on

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  BindShortcutToStatement(shortcut, s);
  return s.Run();
}

bool ShortcutsDatabase::UpdateShortcut(const Shortcut& shortcut) {
  static constexpr char kUpdateSql[] =
      // clang-format off
      "UPDATE omni_box_shortcuts "
        "SET "
          "id=?,text=?,fill_into_edit=?,url=?,"
          "document_type=?,contents=?,contents_class=?,description=?,"
          "description_class=?,transition=?,type=?,keyword=?,"
          "last_access_time=?,number_of_hits=? "
        "WHERE id=?";
  // clang-format on

  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE, kUpdateSql));
  BindShortcutToStatement(shortcut, s);
  s.BindString(14, shortcut.id);
  return s.Run();
}

bool ShortcutsDatabase::DeleteShortcutsWithIDs(
    const ShortcutIDs& shortcut_ids) {
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement s(
      db_.GetUniqueStatement("DELETE FROM omni_box_shortcuts WHERE id=?"));

  for (const std::string& id : shortcut_ids) {
    s.Reset(/*clear_bound_vars=*/true);
    s.BindString(0, id);
    if (!s.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool ShortcutsDatabase::DeleteShortcutsWithURL(
    const std::string& shortcut_url_spec) {
  sql::Statement s(
      db_.GetUniqueStatement("DELETE FROM omni_box_shortcuts WHERE url=?"));
  s.BindString(0, shortcut_url_spec);
  return s.Run();
}

bool ShortcutsDatabase::DeleteAllShortcuts() {
  if (!db_.Execute("DELETE FROM omni_box_shortcuts"))
    return false;

  std::ignore = db_.Execute("VACUUM");
  return true;
}

void ShortcutsDatabase::LoadShortcuts(GuidToShortcutMap* shortcuts) {
  DCHECK(shortcuts);
  shortcuts->clear();

  static constexpr char kSelectSql[] =
      // clang-format off
      "SELECT id,text,fill_into_edit,url,document_type,contents,contents_class,"
        "description,description_class,transition,type,keyword,"
        "last_access_time,number_of_hits "
      "FROM omni_box_shortcuts";
  // clang-format on

  sql::Statement s(db_.GetUniqueStatement(kSelectSql));

  while (s.Step()) {
    // Some users have corrupt data in their SQL database. That causes crashes.
    // Therefore, validate the integral values first. https://crbug.com/1024114
    AutocompleteMatch::DocumentType document_type;
    if (!AutocompleteMatch::DocumentTypeFromInteger(s.ColumnInt(4),
                                                    &document_type)) {
      continue;
    }

    AutocompleteMatchType::Type type;
    if (!AutocompleteMatchType::FromInteger(s.ColumnInt(10), &type))
      continue;

    const int page_transition_integer = s.ColumnInt(9);
    if (!ui::IsValidPageTransitionType(page_transition_integer)) {
      continue;
    }
    ui::PageTransition transition =
        ui::PageTransitionFromInt(page_transition_integer);

    shortcuts->insert(std::make_pair(
        s.ColumnString(0),
        Shortcut(
            s.ColumnString(0),                            // id
            s.ColumnString16(1),                          // text
            Shortcut::MatchCore(s.ColumnString16(2),      // fill_into_edit
                                GURL(s.ColumnString(3)),  // destination_url
                                document_type,            // document_type
                                s.ColumnString16(5),      // contents
                                s.ColumnString(6),        // contents_class
                                s.ColumnString16(7),      // description
                                s.ColumnString(8),        // description_class
                                transition,               // transition
                                type,                     // type
                                s.ColumnString16(11)),    // keyword
            s.ColumnTime(12),
            // last_access_time
            s.ColumnInt(13))));  // number_of_hits
  }
}

ShortcutsDatabase::~ShortcutsDatabase() = default;

bool ShortcutsDatabase::EnsureTable() {
  if (!db_.DoesTableExist("omni_box_shortcuts"))
    return DoMigration(-1);

  // The first version of the shortcuts table (pre-v0) lacked the
  // fill_into_edit, transition, type, and keyword columns.
  // Additionally, pre-v0 lacks a MetaTable from which to identify the version,
  // thus requiring checking the existence of those columns.
  if (!db_.DoesColumnExist("omni_box_shortcuts", "fill_into_edit") &&
      !DoMigration(0)) {
    return false;
  }

  // v0 also lacks a MetaTable. Migrating to v1 introduces the MetaTable in
  // addition to other changes handled by |DoMigration|. If the MetaTable
  // exists, lookup |current_version|. Otherwise, leave it at 0, and
  // |DoMigration(1)| will create the MetaTable.
  int current_version = 0;
  if (sql::MetaTable::DoesTableExist(&db_)) {
    if (!(meta_table_.Init(&db_, 1, kCompatibleVersionNumber)))
      return false;
    current_version = meta_table_.GetVersionNumber();
  }

  for (int i = current_version + 1; i <= kCurrentVersionNumber; ++i) {
    if (!DoMigration(i)) {
      return false;
    }
  }

  return true;
}

bool ShortcutsDatabase::DoMigration(int version) {
  // The migration must be performed transactionally with the meta table
  // update, or else one of them can be applied without the other, leading to
  // incorrect logic on subsequent use.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  switch (version) {
    case -1:
      // When there is no existing table, skip iterative migration; instead,
      // migrate to the latest version.
      return meta_table_.Init(&db_, kCurrentVersionNumber,
                              kCompatibleVersionNumber) &&
             db_.Execute(
                 "CREATE TABLE omni_box_shortcuts(id VARCHAR PRIMARY KEY,"
                 "text VARCHAR,fill_into_edit VARCHAR,url VARCHAR,"
                 "document_type INTEGER,contents VARCHAR,"
                 "contents_class VARCHAR,description VARCHAR,"
                 "description_class VARCHAR,transition INTEGER,type INTEGER,"
                 "keyword VARCHAR,last_access_time INTEGER,"
                 "number_of_hits INTEGER)") &&
             transaction.Commit();
    case 0:
      static_assert(static_cast<int>(ui::PAGE_TRANSITION_TYPED) == 1);
      static_assert(static_cast<int>(AutocompleteMatchType::HISTORY_TITLE) ==
                    2);

      // Version pre-0 of the shortcuts table lacked the fill_into_edit,
      // transition type, and keyword columns.
      return db_.Execute(
                 "ALTER TABLE omni_box_shortcuts "
                 "ADD COLUMN fill_into_edit VARCHAR") &&
             db_.Execute("UPDATE omni_box_shortcuts SET fill_into_edit=url") &&
             db_.Execute(
                 "ALTER TABLE omni_box_shortcuts "
                 "ADD COLUMN transition INTEGER") &&
             db_.Execute("UPDATE omni_box_shortcuts SET transition=1") &&
             db_.Execute(
                 "ALTER TABLE omni_box_shortcuts ADD COLUMN type INTEGER") &&
             db_.Execute("UPDATE omni_box_shortcuts SET type=2") &&
             db_.Execute(
                 "ALTER TABLE omni_box_shortcuts ADD COLUMN keyword VARCHAR") &&
             transaction.Commit();
    case 1:
      return  // Create the MetaTable.
          meta_table_.Init(&db_, 1, kCompatibleVersionNumber) &&
          // Migrate old SEARCH_OTHER_ENGINE values to the new type value.
          db_.Execute("UPDATE omni_box_shortcuts SET type=13 WHERE type=9") &&
          // Migrate old EXTENSION_APP values to the new type value.
          db_.Execute("UPDATE omni_box_shortcuts SET type=14 WHERE type=10") &&
          // Migrate old CONTACT values to the new type value.
          db_.Execute("UPDATE omni_box_shortcuts SET type=15 WHERE type=11") &&
          // Migrate old BOOKMARK_TITLE values to the new type value.
          db_.Execute("UPDATE omni_box_shortcuts SET type=16 WHERE type=12") &&
          transaction.Commit();
    case 2:
      // Version 1 of the shortcuts table lacked the document_type column.
      return db_.Execute(
                 "ALTER TABLE omni_box_shortcuts "
                 "ADD COLUMN document_type INTEGER") &&
             meta_table_.SetVersionNumber(version) && transaction.Commit();
    default:
      return false;
  }
}
