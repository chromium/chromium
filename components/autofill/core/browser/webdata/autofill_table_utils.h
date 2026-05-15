// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_

#include <stddef.h>

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

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

// Wrapper around `sql::CreateTable()` that conditions the creation on the
// `table_name` not existing.
// Returns true if the table now exists.
bool CreateTableIfNotExists(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<
        const std::pair<const std::string_view, const std::string_view>>
        column_names_and_types,
    std::initializer_list<const std::string_view> composite_primary_key = {});

// Wrapper around `sql::Database::DoesColumnExist()`, because that function
// only accepts const char* parameters.
bool DoesColumnExist(sql::Database* db,
                     std::string_view table_name,
                     std::string_view column_name);

// Like `sql::AddColumn()`, but conditioned on `column` not existing in
// `table_name`. Returns true if the column is now part of the table.
bool AddColumnIfNotExists(sql::Database* db,
                          std::string_view table_name,
                          std::string_view column_name,
                          std::string_view type);

// Drops the column named `column_name` from `table_name` if it exists and
// returns true if the column does not exist or if it was dropped successfully.
bool DropColumnIfExists(sql::Database* db,
                        std::string_view table_name,
                        std::string_view column_name);

// Drops `table_name`, if the table exists. Returns true if the statement
// finishes successfully, independently of whether a table was actually dropped.
bool DropTableIfExists(sql::Database* db, std::string_view table_name);

// Wrapper around `sql::SelectBuilder()` that restricts the it to the provided
// `guid`. Returns `statement.is_valid() && statement.Step()`.
bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  std::string_view table_name,
                  std::initializer_list<const std::string_view> columns,
                  std::string_view guid);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOFILL_TABLE_UTILS_H_
