// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_schema.h"

#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/string_escape.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {
namespace {
static const char kSomeTableCreationSql[] =
    "CREATE TABLE some_table "
    "(id INTEGER PRIMARY KEY NOT NULL,"
    " value INTEGER NOT NULL)";

static const char kAnotherTableCreationSql[] =
    "CREATE TABLE another_table "
    "(id INTEGER PRIMARY KEY NOT NULL,"
    " name VARCHAR NOT NULL)";

std::vector<std::string> TableColumns(sql::Database* db,
                                      const std::string table_name) {
  std::vector<std::string> columns;
  std::string sql = "PRAGMA TABLE_INFO(" + table_name + ")";
  sql::Statement table_info(db->GetUniqueStatement(sql.c_str()));
  while (table_info.Step())
    columns.push_back(table_info.ColumnString(1));
  return columns;
}

struct Table {
  std::string ToString() const {
    std::ostringstream ss;
    ss << "-- TABLE " << name << " --\n";
    for (size_t row_index = 0; row_index < rows.size(); ++row_index) {
      ss << "--- ROW " << row_index << " ---\n";
      const std::vector<std::string>& row = rows[row_index];
      for (size_t i = 0; i < row.size(); ++i) {
        ss << column_names[i] << ": " << base::GetQuotedJSONString(row[i])
           << '\n';
      }
    }
    return ss.str();
  }

  std::string name;
  std::vector<std::string> column_names;
  // List of all values. Has size [row_count][column_count].
  std::vector<std::vector<std::string>> rows;
};

// Returns the value contained in a table cell, or nullptr if the cell row or
// column is invalid.
const std::string* TableCell(const Table& table,
                             const std::string& column,
                             size_t row) {
  if (row >= table.rows.size())
    return nullptr;
  for (size_t i = 0; i < table.column_names.size(); ++i) {
    if (table.column_names[i] == column) {
      return &table.rows[row][i];
    }
  }
  return nullptr;
}

struct DatabaseTables {
  std::string ToString() {
    std::ostringstream ss;
    for (auto i = tables.begin(); i != tables.end(); ++i)
      ss << i->second.ToString();
    return ss.str();
  }
  std::map<std::string, Table> tables;
};

Table ReadTable(sql::Database* db, const std::string table_name) {
  Table table;
  table.name = table_name;
  table.column_names = TableColumns(db, table_name);
  std::string sql = "SELECT * FROM " + table_name;
  sql::Statement all_data(db->GetUniqueStatement(sql.c_str()));
  while (all_data.Step()) {
    std::vector<std::string> row;
    for (size_t i = 0; i < table.column_names.size(); ++i) {
      row.push_back(all_data.ColumnString(i));
    }
    table.rows.push_back(std::move(row));
  }
  return table;
}

// Returns all tables in |db|, except the 'meta' table. We don't test the 'meta'
// table directly in this file, but instead use the MetaTable class.
DatabaseTables ReadTables(sql::Database* db) {
  DatabaseTables database_tables;
  std::stringstream ss;
  sql::Statement table_names(db->GetUniqueStatement(
      "SELECT name FROM sqlite_master WHERE type='table'"));
  while (table_names.Step()) {
    const std::string table_name = table_names.ColumnString(0);
    if (table_name == "meta")
      continue;
    database_tables.tables[table_name] = ReadTable(db, table_name);
  }
  return database_tables;
}

// Returns the SQL that defines a table.
std::string TableSql(sql::Database* db, const std::string& table_name) {
  DatabaseTables database_tables;
  std::stringstream ss;
  sql::Statement table_sql(db->GetUniqueStatement(
      "SELECT sql FROM sqlite_master WHERE type='table' AND name=?"));
  table_sql.BindString(0, table_name);
  if (!table_sql.Step())
    return std::string();
  // Try to normalize the SQL, since we use this to compare schemas.
  std::string sql =
      base::CollapseWhitespaceASCII(table_sql.ColumnString(0), true);
  base::ReplaceSubstringsAfterOffset(&sql, 0, ", ", ",");
  base::ReplaceSubstringsAfterOffset(&sql, 0, ",", ",\n");
  return sql;
}

std::string ReadSchemaFile(const std::string& file_name) {
  std::string data;
  base::FilePath path;
  CHECK(base::PathService::Get(base::DIR_SOURCE_ROOT, &path));
  path = path.AppendASCII(
                 "components/test/data/offline_pages/prefetch/version_schemas/")
             .AppendASCII(file_name);
  CHECK(base::ReadFileToString(path, &data)) << path;
  return data;
}

std::unique_ptr<sql::Database> CreateTablesWithSampleRows(int version) {
  auto db = std::make_unique<sql::Database>();
  CHECK(db->OpenInMemory());
  // Write a meta table. v*.sql overwrites version and last_compatible_version.
  sql::MetaTable meta_table;
  CHECK(meta_table.Init(db.get(), 1, 1));

  const std::string schema = ReadSchemaFile(
      base::StrCat({"v", base::NumberToString(version), ".sql"}));
  CHECK(db->Execute(schema.c_str()));
  return db;
}

void ExpectDbIsCurrent(sql::Database* db) {
  // Check the meta table.
  sql::MetaTable meta_table;
  EXPECT_TRUE(meta_table.Init(db, 1, 1));
  EXPECT_EQ(PrefetchStoreSchema::kCurrentVersion,
            meta_table.GetVersionNumber());
  EXPECT_EQ(PrefetchStoreSchema::kCompatibleVersion,
            meta_table.GetCompatibleVersionNumber());

  std::unique_ptr<sql::Database> current_db =
      CreateTablesWithSampleRows(PrefetchStoreSchema::kCurrentVersion);

  // Check that database schema is current.
  for (auto name_and_table : ReadTables(db).tables) {
    const std::string current_sql =
        TableSql(current_db.get(), name_and_table.first);
    const std::string real_sql = TableSql(db, name_and_table.first);
    EXPECT_EQ(current_sql, real_sql);
  }
}

TEST(PrefetchStoreSchemaPreconditionTest,
     TestSqliteCreateTableIsTransactional) {
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db.Execute(kSomeTableCreationSql));
  EXPECT_TRUE(db.Execute(kAnotherTableCreationSql));
  transaction.Rollback();

  EXPECT_FALSE(db.DoesTableExist("some_table"));
  EXPECT_FALSE(db.DoesTableExist("another_table"));
}

TEST(PrefetchStoreSchemaPreconditionTest, TestSqliteDropTableIsTransactional) {
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());
  EXPECT_TRUE(db.Execute(kSomeTableCreationSql));
  EXPECT_TRUE(db.Execute(kAnotherTableCreationSql));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db.Execute("DROP TABLE some_table"));
  EXPECT_TRUE(db.Execute("DROP TABLE another_table"));
  transaction.Rollback();

  EXPECT_TRUE(db.DoesTableExist("some_table"));
  EXPECT_TRUE(db.DoesTableExist("another_table"));
}

TEST(PrefetchStoreSchemaPreconditionTest, TestSqliteAlterTableIsTransactional) {
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());
  EXPECT_TRUE(db.Execute(kSomeTableCreationSql));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db.Execute("ALTER TABLE some_table ADD new_column VARCHAR NULL"));
  EXPECT_TRUE(db.Execute("ALTER TABLE some_table RENAME TO another_table"));
  transaction.Rollback();

  EXPECT_TRUE(db.DoesTableExist("some_table"));
  EXPECT_FALSE(db.DoesColumnExist("some_table", "new_column"));
  EXPECT_FALSE(db.DoesTableExist("another_table"));
}

TEST(PrefetchStoreSchemaPreconditionTest,
     TestCommonMigrationCodeIsTransactional) {
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());
  EXPECT_TRUE(db.Execute(kSomeTableCreationSql));

  sql::Transaction transaction(&db);
  ASSERT_TRUE(transaction.Begin());
  EXPECT_TRUE(db.Execute("ALTER TABLE some_table RENAME TO another_table"));
  EXPECT_TRUE(db.Execute(kSomeTableCreationSql));
  EXPECT_TRUE(db.Execute("DROP TABLE another_table"));
  transaction.Rollback();

  EXPECT_TRUE(db.DoesTableExist("some_table"));
  EXPECT_FALSE(db.DoesTableExist("another_table"));
  EXPECT_TRUE(db.DoesColumnExist("some_table", "value"));
}

TEST(PrefetchStoreSchemaTest, TestInvalidMetaTable) {
  // Verify CreateOrUpgradeIfNeeded will raze the db if it can't be used.
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());
  sql::MetaTable meta;
  meta.Init(&db, 100, 99);  // Some future version.
  ASSERT_TRUE(db.Execute("CREATE TABLE prefetch_items (x INTEGER DEFAULT 1);"));
  ASSERT_TRUE(PrefetchStoreSchema::CreateOrUpgradeIfNeeded(&db));

  ExpectDbIsCurrent(&db);
}

// Verify the latest v#.sql accurately represents the current schema.
//
// Note: We keep the creation code for the current schema version duplicated in
// PrefetchStoreSchema and in the latest version test file so that when we move
// on from the current schema we already know it's represented correctly in the
// test.
TEST(PrefetchStoreSchemaTest, TestCurrentSqlFileIsAccurate) {
  // Create the database with the release code, and with v?.sql.
  sql::Database db;
  ASSERT_TRUE(db.OpenInMemory());
  ASSERT_TRUE(PrefetchStoreSchema::CreateOrUpgradeIfNeeded(&db));

  ExpectDbIsCurrent(&db);
}

// Tests database creation starting with all previous versions, or an empty
// state.
TEST(PrefetchStoreSchemaTest, TestCreateOrMigrate) {
  for (int i = 0; i <= PrefetchStoreSchema::kCurrentVersion; ++i) {
    SCOPED_TRACE(testing::Message() << "Testing migration from version " << i);
    std::unique_ptr<sql::Database> db;
    // When i==0, start from an empty state.
    const int version = i > 0 ? i : PrefetchStoreSchema::kCurrentVersion;
    if (i > 0) {
      db = CreateTablesWithSampleRows(i);
      // Executes the migration.
      EXPECT_TRUE(PrefetchStoreSchema::CreateOrUpgradeIfNeeded(db.get()));
    } else {
      db = std::make_unique<sql::Database>();
      ASSERT_TRUE(db->OpenInMemory());
      // Creation from scratch.
      EXPECT_TRUE(PrefetchStoreSchema::CreateOrUpgradeIfNeeded(db.get()));
      // Tables are already created, this will just insert rows.
      const std::string schema = ReadSchemaFile(
          base::StrCat({"v", base::NumberToString(version), ".sql"}));
      ASSERT_TRUE(db->Execute(schema.c_str()));
    }

    // Check schema.
    ExpectDbIsCurrent(db.get());

    // Check the database contents.
    std::string expected_data = ReadSchemaFile(
        base::StrCat({"v", base::NumberToString(version), ".data"}));
    EXPECT_EQ(expected_data, ReadTables(db.get()).ToString());
  }
}

// Test that the current database version can be used by all compatible
// versions.
TEST(PrefetchStoreSchemaTest, TestRevert) {
  static_assert(PrefetchStoreSchema::kCompatibleVersion == 1,
                "If compatible version is changed, add a test to verify the "
                "database is correctly razed and recreated!");

  // This test simply runs the insert operations in v*.sql on a database
  // with the current schema.
  for (int version = PrefetchStoreSchema::kCompatibleVersion;
       version < PrefetchStoreSchema::kCurrentVersion; ++version) {
    SCOPED_TRACE(testing::Message() << "Testing revert to version " << version);
    // First, extract the expected state after running v*.sql.
    DatabaseTables original_state;
    {
      std::unique_ptr<sql::Database> db = CreateTablesWithSampleRows(version);
      original_state = ReadTables(db.get());
    }

    // Create a new database at the current version.
    sql::Database db;
    ASSERT_TRUE(db.OpenInMemory());
    EXPECT_TRUE(PrefetchStoreSchema::CreateOrUpgradeIfNeeded(&db));

    // Attempt to insert a row using the old SQL.
    const std::string schema = ReadSchemaFile(
        base::StrCat({"v", base::NumberToString(version), ".sql"}));
    EXPECT_TRUE(db.Execute(schema.c_str()));

    // Check the database contents.
    // We should find every value from original_state present in the db.
    std::string expected_data = ReadSchemaFile(
        base::StrCat({"v", base::NumberToString(version), ".data"}));
    const DatabaseTables new_state = ReadTables(&db);
    for (auto name_and_table : original_state.tables) {
      const Table& original_table = name_and_table.second;
      ASSERT_EQ(1ul, new_state.tables.count(name_and_table.first));
      const Table& new_table =
          new_state.tables.find(name_and_table.first)->second;
      for (size_t row = 0; row < original_table.rows.size(); ++row) {
        for (const std::string& column_name : original_table.column_names) {
          const std::string* old_value =
              TableCell(original_table, column_name, row);
          const std::string* new_value = TableCell(new_table, column_name, row);
          ASSERT_TRUE(old_value);
          EXPECT_TRUE(new_value) << "new table does not have old value";
          if (new_value) {
            EXPECT_EQ(*old_value, *new_value);
          }
        }
      }
    }
  }
}

}  // namespace
}  // namespace offline_pages
