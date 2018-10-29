// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/omnibox/browser/shortcuts_database.h"

#include <stddef.h>

#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/omnibox/browser/autocomplete_match_type.h"
#include "components/omnibox/browser/shortcuts_constants.h"
#include "sql/statement.h"
#include "sql/test/scoped_error_expecter.h"
#include "sql/test/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"

using base::ASCIIToUTF16;

// Helpers --------------------------------------------------------------------

namespace {

struct ShortcutsDatabaseTestInfo {
  std::string guid;
  std::string text;
  std::string fill_into_edit;
  std::string destination_url;
  std::string contents;
  std::string contents_class;
  std::string description;
  std::string description_class;
  ui::PageTransition transition;
  AutocompleteMatchType::Type type;
  std::string keyword;
  int days_from_now;
  int number_of_hits;
} shortcut_test_db[] = {
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880DF", "goog", "www.google.com",
    "http://www.google.com/", "Google", "0,1,4,0", "Google", "0,1",
    ui::PAGE_TRANSITION_GENERATED, AutocompleteMatchType::SEARCH_HISTORY,
    "google.com", 1, 100, },
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E0", "slash", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0",
    ui::PAGE_TRANSITION_TYPED, AutocompleteMatchType::HISTORY_URL, "", 0,
    100},
  { "BD85DBA2-8C29-49F9-84AE-48E1E90880E1", "news", "slashdot.org",
    "http://slashdot.org/", "slashdot.org", "0,1",
    "Slashdot - News for nerds, stuff that matters", "0,0",
    ui::PAGE_TRANSITION_LINK, AutocompleteMatchType::HISTORY_TITLE, "", 0,
    5},
};

typedef testing::Test ShortcutsDatabaseMigrationTest;

// Checks that the database at |db| has the version 2 columns iff |is_v2|.
void CheckV2ColumnExistence(const base::FilePath& db_path, bool is_v2) {
  sql::Database connection;
  ASSERT_TRUE(connection.Open(db_path));
  EXPECT_EQ(is_v2,
            connection.DoesColumnExist("omni_box_shortcuts", "fill_into_edit"));
  EXPECT_EQ(is_v2,
            connection.DoesColumnExist("omni_box_shortcuts", "transition"));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts", "type"));
  EXPECT_EQ(is_v2, connection.DoesColumnExist("omni_box_shortcuts", "keyword"));
}

const base::FilePath GetTestDataDir() {
  base::FilePath path;
  base::PathService::Get(base::DIR_SOURCE_ROOT, &path);
  return path.AppendASCII("components/test/data/omnibox");
}

}  // namespace

// ShortcutsDatabaseTest ------------------------------------------------------

class ShortcutsDatabaseTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  void ClearDB();
  size_t CountRecords() const;

  ShortcutsDatabase::Shortcut ShortcutFromTestInfo(
      const ShortcutsDatabaseTestInfo& info);

  void AddAll();

  base::ScopedTempDir temp_dir_;
  scoped_refptr<ShortcutsDatabase> db_;
};

void ShortcutsDatabaseTest::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  base::FilePath db_path(temp_dir_.GetPath().Append(kShortcutsDatabaseName));
  db_ = new ShortcutsDatabase(db_path);
  ASSERT_TRUE(db_->Init());
  ClearDB();
}

void ShortcutsDatabaseTest::TearDown() {
  db_ = nullptr;
}

void ShortcutsDatabaseTest::ClearDB() {
  sql::Statement s(
      db_->db_.GetUniqueStatement("DELETE FROM omni_box_shortcuts"));
  EXPECT_TRUE(s.Run());
}

size_t ShortcutsDatabaseTest::CountRecords() const {
  sql::Statement s(
      db_->db_.GetUniqueStatement("SELECT count(*) FROM omni_box_shortcuts"));
  EXPECT_TRUE(s.Step());
  return static_cast<size_t>(s.ColumnInt(0));
}

ShortcutsDatabase::Shortcut ShortcutsDatabaseTest::ShortcutFromTestInfo(
    const ShortcutsDatabaseTestInfo& info) {
  return ShortcutsDatabase::Shortcut(
      info.guid, ASCIIToUTF16(info.text),
      ShortcutsDatabase::Shortcut::MatchCore(
          ASCIIToUTF16(info.fill_into_edit), GURL(info.destination_url),
          ASCIIToUTF16(info.contents), info.contents_class,
          ASCIIToUTF16(info.description), info.description_class,
          info.transition, info.type, ASCIIToUTF16(info.keyword)),
      base::Time::Now() - base::TimeDelta::FromDays(info.days_from_now),
      info.number_of_hits);
}

void ShortcutsDatabaseTest::AddAll() {
  ClearDB();
  for (size_t i = 0; i < arraysize(shortcut_test_db); ++i)
    db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[i]));
  EXPECT_EQ(arraysize(shortcut_test_db), CountRecords());
}

// Actual tests ---------------------------------------------------------------

TEST_F(ShortcutsDatabaseTest, AddShortcut) {
  ClearDB();
  EXPECT_EQ(0U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[0])));
  EXPECT_EQ(1U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[1])));
  EXPECT_EQ(2U, CountRecords());
  EXPECT_TRUE(db_->AddShortcut(ShortcutFromTestInfo(shortcut_test_db[2])));
  EXPECT_EQ(3U, CountRecords());
}

TEST_F(ShortcutsDatabaseTest, UpdateShortcut) {
  AddAll();
  ShortcutsDatabase::Shortcut shortcut(
      ShortcutFromTestInfo(shortcut_test_db[1]));
  shortcut.match_core.contents = ASCIIToUTF16("gro.todhsals");
  EXPECT_TRUE(db_->UpdateShortcut(shortcut));
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  ShortcutsDatabase::GuidToShortcutMap::const_iterator it(
      shortcuts.find(shortcut.id));
  EXPECT_TRUE(it != shortcuts.end());
  EXPECT_TRUE(it->second.match_core.contents == shortcut.match_core.contents);
}

TEST_F(ShortcutsDatabaseTest, DeleteShortcutsWithIds) {
  AddAll();
  std::vector<std::string> shortcut_ids;
  shortcut_ids.push_back(shortcut_test_db[0].guid);
  shortcut_ids.push_back(shortcut_test_db[2].guid);
  EXPECT_TRUE(db_->DeleteShortcutsWithIDs(shortcut_ids));
  EXPECT_EQ(arraysize(shortcut_test_db) - 2, CountRecords());

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  auto it = shortcuts.find(shortcut_test_db[0].guid);
  EXPECT_TRUE(it == shortcuts.end());

  it = shortcuts.find(shortcut_test_db[1].guid);
  EXPECT_TRUE(it != shortcuts.end());

  it = shortcuts.find(shortcut_test_db[2].guid);
  EXPECT_TRUE(it == shortcuts.end());
}

TEST_F(ShortcutsDatabaseTest, DeleteShortcutsWithURL) {
  AddAll();

  EXPECT_TRUE(db_->DeleteShortcutsWithURL("http://slashdot.org/"));
  EXPECT_EQ(arraysize(shortcut_test_db) - 2, CountRecords());

  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);

  auto it = shortcuts.find(shortcut_test_db[0].guid);
  EXPECT_TRUE(it != shortcuts.end());

  it = shortcuts.find(shortcut_test_db[1].guid);
  EXPECT_TRUE(it == shortcuts.end());

  it = shortcuts.find(shortcut_test_db[2].guid);
  EXPECT_TRUE(it == shortcuts.end());
}

TEST_F(ShortcutsDatabaseTest, DeleteAllShortcuts) {
  AddAll();
  ShortcutsDatabase::GuidToShortcutMap shortcuts;
  db_->LoadShortcuts(&shortcuts);
  EXPECT_EQ(arraysize(shortcut_test_db), shortcuts.size());
  EXPECT_TRUE(db_->DeleteAllShortcuts());
  db_->LoadShortcuts(&shortcuts);
  EXPECT_EQ(0U, shortcuts.size());
}

TEST(ShortcutsDatabaseMigrationTest, MigrateTableAddFillIntoEdit) {
  // Use the pre-v0 test file to create a test database in a temp dir.
  base::FilePath sql_path = GetTestDataDir().AppendASCII(
#if defined(OS_ANDROID)
      "Shortcuts.v1.sql");
#else
      "Shortcuts.no_fill_into_edit.sql");
#endif
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath db_path(temp_dir.GetPath().AppendASCII("TestShortcuts.db"));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, sql_path));

  CheckV2ColumnExistence(db_path, false);

  // Create a ShortcutsDatabase from the test database, which will migrate the
  // test database to the current version.
  {
    scoped_refptr<ShortcutsDatabase> db(new ShortcutsDatabase(db_path));
    db->Init();
  }

  CheckV2ColumnExistence(db_path, true);

  // Check the values in each of the new columns.
  sql::Database connection;
  ASSERT_TRUE(connection.Open(db_path));
  sql::Statement statement(connection.GetUniqueStatement(
      "SELECT fill_into_edit, url, transition, type, keyword "
      "FROM omni_box_shortcuts"));
  ASSERT_TRUE(statement.is_valid());
  while (statement.Step()) {
    // |fill_into_edit| should have been copied from the |url|.
    EXPECT_EQ(statement.ColumnString(1), statement.ColumnString(0));

    // The other three columns have default values.
    EXPECT_TRUE(ui::PageTransitionTypeIncludingQualifiersIs(
        ui::PageTransitionFromInt(statement.ColumnInt(2)),
        ui::PAGE_TRANSITION_TYPED));
    EXPECT_EQ(AutocompleteMatchType::HISTORY_TITLE,
              static_cast<AutocompleteMatchType::Type>(statement.ColumnInt(3)));
    EXPECT_TRUE(statement.ColumnString(4).empty());
  }
  EXPECT_TRUE(statement.Succeeded());
#if !defined(OS_WIN)
  EXPECT_TRUE(temp_dir.Delete());
#endif
}

TEST(ShortcutsDatabaseMigrationTest, MigrateV0ToV1) {
  // Use the v0 test file to create a test database in a temp dir.
  base::FilePath sql_path = GetTestDataDir().AppendASCII("Shortcuts.v0.sql");
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath db_path(temp_dir.GetPath().AppendASCII("TestShortcuts.db"));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, sql_path));

  // Create a ShortcutsDatabase from the test database, which will migrate the
  // test database to the current version.
  {
    scoped_refptr<ShortcutsDatabase> db(new ShortcutsDatabase(db_path));
    db->Init();
  }

  // Check that all the old type values got converted to new values.
  sql::Database connection;
  ASSERT_TRUE(connection.Open(db_path));
  sql::Statement statement(connection.GetUniqueStatement(
      "SELECT count(1) FROM omni_box_shortcuts WHERE type in (9, 10, 11, 12)"));
  ASSERT_TRUE(statement.is_valid());
  while (statement.Step())
    EXPECT_EQ(0, statement.ColumnInt(0));
  EXPECT_TRUE(statement.Succeeded());
#if !defined(OS_WIN)
  EXPECT_TRUE(temp_dir.Delete());
#endif
}

TEST(ShortcutsDatabaseMigrationTest, Recovery1) {
#if defined(OS_ANDROID)
  char kBasename[] = "Shortcuts.v1.sql";
#else
  char kBasename[] = "Shortcuts.no_fill_into_edit.sql";
#endif
  // Use the pre-v0 test file to create a test database in a temp dir.
  base::FilePath sql_path = GetTestDataDir().AppendASCII(kBasename);
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  base::FilePath db_path(temp_dir.GetPath().AppendASCII("TestShortcuts.db"));
  ASSERT_TRUE(sql::test::CreateDatabaseFromSQL(db_path, sql_path));

  // Capture the row count from the golden file before corrupting the database.
  static const char kCountSql[] = "SELECT COUNT(*) FROM omni_box_shortcuts";
  int row_count;
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(db_path));
    sql::Statement statement(connection.GetUniqueStatement(kCountSql));
    ASSERT_TRUE(statement.is_valid());
    ASSERT_TRUE(statement.Step());
    row_count = statement.ColumnInt(0);
  }

  // Break the database.
  ASSERT_TRUE(sql::test::CorruptSizeInHeader(db_path));

  // Verify that the database is broken.  The corruption will prevent reading
  // the schema, causing the prepared statement to not compile.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);

    sql::Database connection;
    ASSERT_TRUE(connection.Open(db_path));
    sql::Statement statement(connection.GetUniqueStatement(kCountSql));
    ASSERT_FALSE(statement.is_valid());

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  // The sql::Database::Open() called by ShortcutsDatabase::Init() will hit
  // the corruption, the error callback will recover and poison the database,
  // then Open() will retry successfully, allowing Init() to succeed.
  {
    sql::test::ScopedErrorExpecter expecter;
    expecter.ExpectError(SQLITE_CORRUPT);

    scoped_refptr<ShortcutsDatabase> db(new ShortcutsDatabase(db_path));
    ASSERT_TRUE(db->Init());

    ASSERT_TRUE(expecter.SawExpectedErrors());
  }

  CheckV2ColumnExistence(db_path, true);

  // The previously-broken statement works and all of the data should have been
  // recovered.
  {
    sql::Database connection;
    ASSERT_TRUE(connection.Open(db_path));
    sql::Statement statement(connection.GetUniqueStatement(kCountSql));
    ASSERT_TRUE(statement.is_valid());
    ASSERT_TRUE(statement.Step());
    EXPECT_EQ(row_count, statement.ColumnInt(0));
  }
}
