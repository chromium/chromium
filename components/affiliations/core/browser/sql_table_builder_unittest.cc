// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/affiliations/core/browser/sql_table_builder.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/mock_callback.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace affiliations {
namespace {

using ::testing::Return;
using ::testing::UnorderedElementsAre;

constexpr char kChildTable[] = "child_table";
constexpr char kMyLoginTable[] = "my_logins_table";

using ColumnValue = absl::variant<int, std::string>;
using TableRow = std::vector<ColumnValue>;

void CheckTableContent(sql::Database& db,
                       const char table_name[],
                       const std::vector<TableRow>& expected_rows) {
  SCOPED_TRACE(testing::Message() << "table_name = " << table_name);
  sql::Statement table_check(
      db.GetUniqueStatement(base::StrCat({"SELECT * FROM ", table_name})));
  for (const TableRow& row : expected_rows) {
    EXPECT_TRUE(table_check.Step());
    for (unsigned col = 0; col < row.size(); ++col) {
      if (const int* int_value = absl::get_if<int>(&row[col])) {
        EXPECT_EQ(*int_value, table_check.ColumnInt(col)) << col;
      } else if (const std::string* string_value =
                     absl::get_if<std::string>(&row[col])) {
        EXPECT_EQ(*string_value, table_check.ColumnString(col)) << col;
      } else {
        EXPECT_TRUE(false) << "Unknown type " << col;
      }
    }
  }
  EXPECT_FALSE(table_check.Step());
}

class SQLTableBuilderTest : public testing::Test {
 public:
  SQLTableBuilderTest() : builder_(kMyLoginTable), child_builder_(kChildTable) {
    Init();
  }

  ~SQLTableBuilderTest() override = default;

 protected:
  // Checks whether a column with a given |name| is listed with the given
  // |type| in the database.
  bool IsColumnOfType(const std::string& name, const std::string& type);

  // Adds a primary key to 'my_logins_table' and creates a child table
  // referencing it.
  void SetupChildTable();

  sql::Database* db() { return &db_; }

  SQLTableBuilder* builder() { return &builder_; }
  SQLTableBuilder* child_builder() { return &child_builder_; }

 private:
  // Part of constructor, needs to be a void-returning function to use ASSERTs.
  void Init();

  // Error handler for the SQL connection, prints the error code and the
  // statement details.
  void PrintDBError(int code, sql::Statement* statement);

  sql::Database db_;
  SQLTableBuilder builder_;
  SQLTableBuilder child_builder_;
};

bool SQLTableBuilderTest::IsColumnOfType(const std::string& name,
                                         const std::string& type) {
  return db()->GetSchema().find(name + " " + type) != std::string::npos;
}

void SQLTableBuilderTest::SetupChildTable() {
  EXPECT_TRUE(db()->Execute("PRAGMA foreign_keys = ON"));
  builder()->AddPrimaryKeyColumn("id");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));

  child_builder_.AddColumn("name", "TEXT");
  child_builder_.AddColumnToUniqueKey("parent_id", "INTEGER",
                                      /*parent_table=*/kMyLoginTable,
                                      "foreign_key_index");
  EXPECT_EQ(0u, child_builder_.SealVersion());
  EXPECT_TRUE(child_builder_.CreateTable(db()));
}

void SQLTableBuilderTest::Init() {
  db_.set_error_callback(base::BindRepeating(&SQLTableBuilderTest::PrintDBError,
                                             base::Unretained(this)));
  ASSERT_TRUE(db_.OpenInMemory());
  // The following column must always be present, so let's add it here.
  builder_.AddColumnToUniqueKey("signon_realm", "VARCHAR NOT NULL");
}

void SQLTableBuilderTest::PrintDBError(int code, sql::Statement* statement) {
  VLOG(0) << "DB error encountered, code = " << code;
  if (statement) {
    VLOG(0) << "statement string = " << statement->GetSQLStatement();
    VLOG(0) << "statement is " << (statement->is_valid() ? "valid" : "invalid");
  }
}

TEST_F(SQLTableBuilderTest, SealVersion_0) {
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "signon_realm"));
  EXPECT_TRUE(IsColumnOfType("signon_realm", "VARCHAR NOT NULL"));
}

TEST_F(SQLTableBuilderTest, AddColumn) {
  builder()->AddColumn("password_value", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "signon_realm"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, AddIndex) {
  builder()->AddIndex("my_logins_table_signon", {"signon_realm"});
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesIndexExist("my_logins_table_signon"));
}

TEST_F(SQLTableBuilderTest, AddIndexOnMultipleColumns) {
  builder()->AddColumn("column_1", "BLOB");
  builder()->AddColumn("column_2", "BLOB");
  builder()->AddIndex("my_index", {"column_1", "column_2"});
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "column_1"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "column_2"));
  EXPECT_TRUE(db()->DoesIndexExist("my_index"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_InSameVersion) {
  builder()->AddColumn("old_name", "BLOB");
  builder()->RenameColumn("old_name", "password_value");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_InNextVersion) {
  builder()->AddColumn("old_name", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->RenameColumn("old_name", "password_value");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "password_value"));
  EXPECT_TRUE(IsColumnOfType("password_value", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_SameNameInSameVersion) {
  builder()->AddColumn("name", "BLOB");
  builder()->RenameColumn("name", "name");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "name"));
  EXPECT_TRUE(IsColumnOfType("name", "BLOB"));
}

TEST_F(SQLTableBuilderTest, RenameColumn_SameNameInNextVersion) {
  builder()->AddColumn("name", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->RenameColumn("name", "name");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "name"));
  EXPECT_TRUE(IsColumnOfType("name", "BLOB"));
}

TEST_F(SQLTableBuilderTest, DropColumn_InSameVersion) {
  builder()->AddColumn("password_value", "BLOB");
  builder()->DropColumn("password_value");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "password_value"));
}

TEST_F(SQLTableBuilderTest, DropColumn_InNextVersion) {
  builder()->AddColumn("password_value", "BLOB");
  EXPECT_EQ(0u, builder()->SealVersion());
  builder()->DropColumn("password_value");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "password_value"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom) {
  // First, create a table at version 0, with some columns.
  builder()->AddColumn("for_renaming", "INTEGER DEFAULT 100");
  builder()->AddColumn("for_deletion", "INTEGER");
  builder()->AddIndex("my_signon_index", {"signon_realm"});
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));
  EXPECT_TRUE(db()->DoesTableExist(kMyLoginTable));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "for_renaming"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "for_deletion"));
  EXPECT_TRUE(db()->DoesIndexExist("my_signon_index"));
  EXPECT_TRUE(
      db()->Execute("INSERT INTO my_logins_table (signon_realm, for_renaming, "
                    "for_deletion) VALUES ('abc', 123, 456)"));
  const char retrieval[] = "SELECT * FROM my_logins_table";
  sql::Statement first_check(
      db()->GetCachedStatement(SQL_FROM_HERE, retrieval));
  EXPECT_TRUE(first_check.Step());
  EXPECT_EQ(3, first_check.ColumnCount());
  EXPECT_EQ("abc", first_check.ColumnString(0));
  EXPECT_EQ(123, first_check.ColumnInt(1));
  EXPECT_EQ(456, first_check.ColumnInt(2));
  EXPECT_FALSE(first_check.Step());
  EXPECT_TRUE(first_check.Succeeded());

  // Now, specify some modifications for version 1.
  builder()->RenameColumn("for_renaming", "renamed");
  builder()->DropColumn("for_deletion");
  builder()->AddColumn("new_column", "INTEGER DEFAULT 789");
  builder()->AddIndex("my_changing_index_v1", {"renamed", "new_column"});
  EXPECT_EQ(1u, builder()->SealVersion());

  // The migration should have the following effect:
  // * The renamed column should keep its non-default value.
  // * The succession of column removal and addition should not result in the
  //   values from the deleted column to be copied to the added one.
  // * Only the signon index and the second version of the changing index should
  //   be present in the last version.
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "for_renaming"));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "for_deletion"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "renamed"));
  EXPECT_TRUE(IsColumnOfType("renamed", "INTEGER"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "new_column"));
  EXPECT_TRUE(db()->DoesIndexExist("my_signon_index"));
  EXPECT_TRUE(db()->DoesIndexExist("my_changing_index_v1"));
  sql::Statement second_check(
      db()->GetCachedStatement(SQL_FROM_HERE, retrieval));
  EXPECT_TRUE(second_check.Step());
  EXPECT_EQ(3, second_check.ColumnCount());
  EXPECT_EQ("abc", second_check.ColumnString(0));
  EXPECT_EQ(123, second_check.ColumnInt(1));
  EXPECT_EQ(789, second_check.ColumnInt(2));
  EXPECT_FALSE(second_check.Step());
  EXPECT_TRUE(second_check.Succeeded());
}

TEST_F(SQLTableBuilderTest, MigrateFrom_RenameAndAddColumns) {
  builder()->AddPrimaryKeyColumn("id");
  builder()->AddColumn("old_name", "INTEGER");
  EXPECT_EQ(0u, builder()->SealVersion());

  EXPECT_TRUE(builder()->CreateTable(db()));

  builder()->RenameColumn("old_name", "new_name");
  EXPECT_EQ(1u, builder()->SealVersion());

  builder()->AddColumn("added", "VARCHAR");
  EXPECT_EQ(2u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "old_name"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "id"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "added"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "new_name"));
  EXPECT_TRUE(IsColumnOfType("id", "INTEGER"));
  EXPECT_TRUE(IsColumnOfType("added", "VARCHAR"));
  EXPECT_TRUE(IsColumnOfType("new_name", "INTEGER"));
  EXPECT_EQ(4u, builder()->NumberOfColumns());
  EXPECT_EQ("signon_realm, id, new_name, added",
            builder()->ListAllColumnNames());
  EXPECT_EQ("new_name=?, added=?", builder()->ListAllNonuniqueKeyNames());
  EXPECT_EQ("signon_realm=?", builder()->ListAllUniqueKeyNames());
  EXPECT_THAT(builder()->AllPrimaryKeyNames(), UnorderedElementsAre("id"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_RenameAndAddAndDropColumns) {
  builder()->AddPrimaryKeyColumn("pk_1");
  builder()->AddColumnToUniqueKey("uni", "VARCHAR NOT NULL");
  builder()->AddColumn("old_name", "INTEGER");
  EXPECT_EQ(0u, builder()->SealVersion());

  EXPECT_TRUE(builder()->CreateTable(db()));

  builder()->RenameColumn("old_name", "new_name");
  EXPECT_EQ(1u, builder()->SealVersion());

  builder()->AddColumn("added", "VARCHAR");
  EXPECT_EQ(2u, builder()->SealVersion());

  builder()->DropColumn("added");
  EXPECT_EQ(3u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "old_name"));
  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "added"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "pk_1"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "uni"));
  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "new_name"));
  EXPECT_TRUE(IsColumnOfType("new_name", "INTEGER"));
  EXPECT_EQ(4u, builder()->NumberOfColumns());
  EXPECT_EQ("signon_realm, pk_1, uni, new_name",
            builder()->ListAllColumnNames());
  EXPECT_EQ("new_name=?", builder()->ListAllNonuniqueKeyNames());
  EXPECT_EQ("signon_realm=? AND uni=?", builder()->ListAllUniqueKeyNames());

  EXPECT_THAT(builder()->AllPrimaryKeyNames(), UnorderedElementsAre("pk_1"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_AddPrimaryKey) {
  builder()->AddColumnToUniqueKey("uni", "VARCHAR NOT NULL");
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_TRUE(builder()->CreateTable(db()));

  builder()->AddPrimaryKeyColumn("pk_1");
  EXPECT_EQ(1u, builder()->SealVersion());

  EXPECT_FALSE(db()->DoesColumnExist(kMyLoginTable, "pk_1"));
  EXPECT_TRUE(db()->GetSchema().find("PRIMARY KEY (pk_1)") ==
              std::string::npos);

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));

  EXPECT_TRUE(db()->DoesColumnExist(kMyLoginTable, "pk_1"));
  EXPECT_TRUE(
      db()->GetSchema().find("pk_1 INTEGER PRIMARY KEY AUTOINCREMENT") !=
      std::string::npos);
}

TEST_F(SQLTableBuilderTest, MigrateFromWithSuccessfulCallback) {
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_EQ(1u, builder()->SealVersion());

  base::MockCallback<base::RepeatingCallback<bool(sql::Database*, unsigned)>>
      migation_callback;

  EXPECT_CALL(migation_callback, Run(db(), 1u)).WillOnce(Return(true));
  EXPECT_TRUE(builder()->MigrateFrom(0, db(), migation_callback.Get()));
}

TEST_F(SQLTableBuilderTest, MigrateFromWithUnsuccessfulCallback) {
  EXPECT_EQ(0u, builder()->SealVersion());
  EXPECT_EQ(1u, builder()->SealVersion());

  base::MockCallback<base::RepeatingCallback<bool(sql::Database*, unsigned)>>
      migation_callback;

  EXPECT_CALL(migation_callback, Run(db(), 1u)).WillOnce(Return(false));
  EXPECT_FALSE(builder()->MigrateFrom(0, db(), migation_callback.Get()));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_AddColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  // Now, specify some modifications for version 1.
  builder()->AddColumn("new_column", "INTEGER DEFAULT 789");
  builder()->AddIndex("my_changing_index_v1", {"signon_realm", "new_column"});
  EXPECT_EQ(1u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));

  CheckTableContent(*db(), kChildTable, {{"Abc Co.", 1}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1, 789}});

  // The foreign key still works.
  EXPECT_FALSE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Co.', 15)", kChildTable)));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_RenameColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  // Now, specify some modifications for version 1.
  builder()->RenameColumn("signon_realm", "signon_real_realm");
  EXPECT_EQ(1u, builder()->SealVersion());

  EXPECT_TRUE(builder()->MigrateFrom(0, db()));

  CheckTableContent(*db(), kChildTable, {{"Abc Co.", 1}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1}});

  // The foreign key still works.
  EXPECT_FALSE(db()->Execute(
      "INSERT INTO child_table (name, parent_id) VALUES ('Co.', 15)"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_DropColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  // Now, specify some modifications for version 1.
  builder()->AddColumn("new_column", "INTEGER DEFAULT 789");
  EXPECT_EQ(1u, builder()->SealVersion());
  builder()->DropColumn("new_column");
  EXPECT_EQ(2u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));

  CheckTableContent(*db(), kChildTable, {{"Abc Co.", 1}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1}});

  // The foreign key still works.
  EXPECT_FALSE(db()->Execute(
      "INSERT INTO child_table (name, parent_id) VALUES ('Co.', 15)"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_PreventMigration) {
  SetupChildTable();

  builder()->AddColumnToUniqueKey("new_column", "TEXT");
  EXPECT_EQ(1u, builder()->SealVersion());
  EXPECT_TRUE(builder()->MigrateFrom(0, db()));

  EXPECT_TRUE(db()->Execute(
      base::StringPrintf("INSERT INTO %s (signon_realm, new_column) "
                         "VALUES ('abc.com', 'aaa')",
                         kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(
      base::StringPrintf("INSERT INTO %s (signon_realm, new_column) "
                         "VALUES ('abc.com', 'bbb')",
                         kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Bbc Co.', 2)", kChildTable)));

  builder()->DropColumn("new_column");
  EXPECT_EQ(2u, builder()->SealVersion());
  // The migration doesn't succeed because foreign key doesn't allow referenced
  // entries to be merged.
  EXPECT_FALSE(builder()->MigrateFrom(1, db()));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_ChildTable_AddColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  child_builder()->AddColumn("new_column", "TEXT");
  EXPECT_EQ(1u, child_builder()->SealVersion());
  EXPECT_TRUE(child_builder()->MigrateFrom(0, db()));

  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "UPDATE %s SET new_column='value' WHERE parent_id=1", kChildTable)));

  CheckTableContent(*db(), kChildTable, {{"Abc Co.", 1, "value"}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1}});

  // The foreign key still works.
  EXPECT_FALSE(db()->Execute(
      "INSERT INTO child_table (name, parent_id) VALUES ('Co.', 15)"));
}

TEST_F(SQLTableBuilderTest,
       MigrateFrom_WithForeignKey_ChildTable_RenameColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  child_builder()->RenameColumn("name", "new_name");
  EXPECT_EQ(1u, child_builder()->SealVersion());
  EXPECT_TRUE(child_builder()->MigrateFrom(0, db()));

  CheckTableContent(*db(), kChildTable, {{"Abc Co.", 1}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1}});

  // The foreign key still works.
  EXPECT_FALSE(db()->Execute(
      "INSERT INTO child_table (new_name, parent_id) VALUES ('Co.', 15)"));
}

TEST_F(SQLTableBuilderTest, MigrateFrom_WithForeignKey_ChildTable_DropColumn) {
  SetupChildTable();
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (signon_realm) VALUES ('abc.com')", kMyLoginTable)));
  EXPECT_TRUE(db()->Execute(base::StringPrintf(
      "INSERT INTO %s (name, parent_id) VALUES ('Abc Co.', 1)", kChildTable)));

  child_builder()->DropColumn("name");
  EXPECT_EQ(1u, child_builder()->SealVersion());
  EXPECT_TRUE(child_builder()->MigrateFrom(0, db()));

  CheckTableContent(*db(), kChildTable, {{1}});
  CheckTableContent(*db(), kMyLoginTable, {{"abc.com", 1}});

  // The foreign key still works.
  EXPECT_FALSE(
      db()->Execute("INSERT INTO child_table (parent_id) VALUES (15)"));
}

}  // namespace
}  // namespace affiliations
