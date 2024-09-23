// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table_utils.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace autofill {

std::u16string Truncate(std::u16string_view data) {
  return std::u16string(data.substr(0, kMaxDataLengthForDatabase));
}

bool CreateTable(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key) {
  DCHECK(composite_primary_key.size() == 0 ||
         composite_primary_key.size() >= 2);

  std::vector<std::string> combined_names_and_types;
  combined_names_and_types.reserve(column_names_and_types.size());
  for (const auto& [name, type] : column_names_and_types) {
    combined_names_and_types.push_back(base::StrCat({name, " ", type}));
  }

  auto primary_key_clause =
      composite_primary_key.size() == 0
          ? ""
          : base::StrCat({", PRIMARY KEY (",
                          base::JoinString(composite_primary_key, ", "), ")"});

  return db->Execute(
      base::StrCat({"CREATE TABLE ", table_name, " (",
                    base::JoinString(combined_names_and_types, ", "),
                    primary_key_clause, ")"}));
}

bool CreateTableIfNotExists(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key) {
  return db->DoesTableExist(table_name) ||
         CreateTable(db, table_name, column_names_and_types,
                     composite_primary_key);
}

bool CreateIndex(sql::Database* db,
                 std::string_view table_name,
                 std::initializer_list<std::string_view> columns) {
  auto index_name =
      base::StrCat({table_name, "_", base::JoinString(columns, "_")});
  return db->Execute(
      base::StrCat({"CREATE INDEX ", index_name, " ON ", table_name, "(",
                    base::JoinString(columns, ", "), ")"}));
}

void InsertBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
                   bool or_replace) {
  auto insert_or_replace =
      base::StrCat({"INSERT ", or_replace ? "OR REPLACE " : ""});
  auto placeholders = base::JoinString(
      std::vector<std::string>(column_names.size(), "?"), ", ");
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({insert_or_replace, "INTO ", table_name, " (",
                    base::JoinString(column_names, ", "), ") VALUES (",
                    placeholders, ")"})));
}

bool RenameTable(sql::Database* db,
                 std::string_view from,
                 std::string_view to) {
  return db->Execute(base::StrCat({"ALTER TABLE ", from, " RENAME TO ", to}));
}

bool DoesColumnExist(sql::Database* db,
                     std::string_view table_name,
                     std::string_view column_name) {
  return db->DoesColumnExist(std::string(table_name), std::string(column_name));
}

bool AddColumn(sql::Database* db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type) {
  return db->Execute(base::StrCat(
      {"ALTER TABLE ", table_name, " ADD COLUMN ", column_name, " ", type}));
}

bool AddColumnIfNotExists(sql::Database* db,
                          std::string_view table_name,
                          std::string_view column_name,
                          std::string_view type) {
  return DoesColumnExist(db, table_name, column_name) ||
         AddColumn(db, table_name, column_name, type);
}

bool DropColumn(sql::Database* db,
                std::string_view table_name,
                std::string_view column_name) {
  return db->Execute(
      base::StrCat({"ALTER TABLE ", table_name, " DROP COLUMN ", column_name}));
  ;
}

bool DropColumnIfExists(sql::Database* db,
                        std::string_view table_name,
                        std::string_view column_name) {
  return !DoesColumnExist(db, table_name, column_name) ||
         DropColumn(db, table_name, column_name);
}

bool DropTableIfExists(sql::Database* db, std::string_view table_name) {
  return db->Execute(base::StrCat({"DROP TABLE IF EXISTS ", table_name}));
}

void DeleteBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause) {
  auto where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"DELETE FROM ", table_name, where})));
}

bool Delete(sql::Database* db,
            std::string_view table_name,
            std::string_view where_clause) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, where_clause);
  return statement.Run();
}

bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindString(0, value);
  return statement.Run();
}

bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value) {
  sql::Statement statement;
  DeleteBuilder(db, statement, table_name, base::StrCat({column, " = ?"}));
  statement.BindInt64(0, value);
  return statement.Run();
}

void UpdateBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
                   std::string_view where_clause) {
  auto columns_with_placeholders =
      base::JoinString(column_names, " = ?, ") + " = ?";
  auto where =
      where_clause.empty() ? "" : base::StrCat({" WHERE ", where_clause});
  statement.Assign(db->GetUniqueStatement(base::StrCat(
      {"UPDATE ", table_name, " SET ", columns_with_placeholders, where})));
}

void SelectBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> columns,
                   std::string_view modifiers) {
  statement.Assign(db->GetUniqueStatement(
      base::StrCat({"SELECT ", base::JoinString(columns, ", "), " FROM ",
                    table_name, " ", modifiers})));
}

bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  std::string_view table_name,
                  std::initializer_list<std::string_view> columns,
                  std::string_view guid) {
  SelectBuilder(db, statement, table_name, columns, "WHERE guid=?");
  statement.BindString(0, guid);
  return statement.is_valid() && statement.Step();
}

}  // namespace autofill
