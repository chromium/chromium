// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_

#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace autofill {

// Max length of values stored in address and payments related tables. This
// limit is not enforced for autocomplete values.
inline constexpr size_t kMaxDataLengthForDatabase = 1024;

// Truncates `data` to `kMaxDataLengthForDatabase`.
std::u16string Truncate(std::u16string_view data);

// Helper functions to construct SQL statements from string constants.
// - Functions with names corresponding to SQL keywords execute the statement
//   directly and return if it was successful.
// - Builder functions only assign the statement, which enables binding
//   values to placeholders before running it.

// Executes a CREATE TABLE statement on `db` which the provided
// `table_name`. The columns are described in `column_names_and_types` as
// pairs of (name, type), where type can include modifiers such as NOT NULL.
// By specifying `compositive_primary_key`, a PRIMARY KEY (col1, col2, ..)
// clause is generated.
// Returns true if successful.
bool CreateTable(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key = {});

// Wrapper around `CreateTable()` that condition the creation on the
// `table_name` not existing.
// Returns true if the table now exists.
bool CreateTableIfNotExists(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        column_names_and_types,
    std::initializer_list<std::string_view> composite_primary_key = {});

// Creates and index on `table_name` for the provided `columns`.
// The index is named after the table and columns, separated by '_'.
// Returns true if successful.
bool CreateIndex(sql::Database* db,
                 std::string_view table_name,
                 std::initializer_list<std::string_view> columns);

// Initializes `statement` with INSERT INTO `table_name`, with placeholders for
// all `column_names`.
// By setting `or_replace`, INSERT OR REPLACE INTO is used instead.
void InsertBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
                   bool or_replace = false);

// Renames the table `from` into `to` and returns true if successful.
bool RenameTable(sql::Database* db, std::string_view from, std::string_view to);

// Wrapper around `sql::Database::DoesColumnExist()`, because that function
// only accepts const char* parameters.
bool DoesColumnExist(sql::Database* db,
                     std::string_view table_name,
                     std::string_view column_name);

// Adds a column named `column_name` of `type` to `table_name` and returns true
// if successful.
bool AddColumn(sql::Database* db,
               std::string_view table_name,
               std::string_view column_name,
               std::string_view type);

// Like `AddColumn()`, but conditioned on `column` not existing in `table_name`.
// Returns true if the column is now part of the table
bool AddColumnIfNotExists(sql::Database* db,
                          std::string_view table_name,
                          std::string_view column_name,
                          std::string_view type);

// Drops the column named `column_name` from `table_name` and returns true if
// successful.
bool DropColumn(sql::Database* db,
                std::string_view table_name,
                std::string_view column_name);

// Drops the column named `column_name` from `table_name` if it exists and
// returns true if the column does not exist or if it was dropped successfully.
bool DropColumnIfExists(sql::Database* db,
                        std::string_view table_name,
                        std::string_view column_name);

// Drops `table_name`, if the table exists. Returns true if the statement
// finishes successfully, independently of whether a table was actually dropped.
bool DropTableIfExists(sql::Database* db, std::string_view table_name);

// Initializes `statement` with DELETE FROM `table_name`. A WHERE clause
// can optionally be specified in `where_clause`.
void DeleteBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::string_view where_clause = "");

// Like `DeleteBuilder()`, but runs the statement and returns true if it was
// successful.
bool Delete(sql::Database* db,
            std::string_view table_name,
            std::string_view where_clause = "");

// Wrapper around `DeleteBuilder()`, which initializes the where clause as
// `column` = `value`.
// Runs the statement and returns true if it was successful.
bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         std::string_view value);

// Wrapper around `DeleteBuilder()`, which initializes the where clause as
// `column` = `value` for int64_t type.
// Runs the statement and returns true if it was successful.
bool DeleteWhereColumnEq(sql::Database* db,
                         std::string_view table_name,
                         std::string_view column,
                         int64_t value);

// Initializes `statement` with UPDATE `table_name` SET `column_names` = ?, with
// a placeholder for every `column_names`. A WHERE clause can optionally be
// specified in `where_clause`.
void UpdateBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> column_names,
                   std::string_view where_clause = "");

// Initializes `statement` with SELECT `columns` FROM `table_name` and
// optionally further `modifiers`, such as WHERE, ORDER BY, etc.
void SelectBuilder(sql::Database* db,
                   sql::Statement& statement,
                   std::string_view table_name,
                   std::initializer_list<std::string_view> columns,
                   std::string_view modifiers = "");

// Wrapper around `SelectBuilder()` that restricts the it to the provided
// `guid`. Returns `statement.is_valid() && statement.Step()`.
bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  std::string_view table_name,
                  std::initializer_list<std::string_view> columns,
                  std::string_view guid);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_
