// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table_utils.h"

#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>

#include "base/strings/strcat.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/table_management_helpers.h"

namespace autofill {

std::u16string Truncate(std::u16string_view data) {
  return std::u16string(data.substr(0, kMaxDataLengthForDatabase));
}

std::u16string EscapeLikePattern(std::u16string_view pattern,
                                 char16_t escape_char) {
  std::u16string escaped_pattern;
  escaped_pattern.reserve(pattern.size());
  for (char16_t c : pattern) {
    if (c == escape_char || c == u'%' || c == u'_') {
      escaped_pattern.push_back(escape_char);
    }
    escaped_pattern.push_back(c);
  }
  return escaped_pattern;
}

bool CreateTableIfNotExists(
    sql::Database* db,
    std::string_view table_name,
    std::initializer_list<
        const std::pair<const std::string_view, const std::string_view>>
        column_names_and_types,
    std::initializer_list<const std::string_view> composite_primary_key) {
  return db->DoesTableExist(table_name) ||
         sql::CreateTable(*db, table_name, column_names_and_types,
                          composite_primary_key);
}

bool DoesColumnExist(sql::Database* db,
                     std::string_view table_name,
                     std::string_view column_name) {
  return db->DoesColumnExist(std::string(table_name), std::string(column_name));
}

bool AddColumnIfNotExists(sql::Database* db,
                          std::string_view table_name,
                          std::string_view column_name,
                          std::string_view type) {
  return DoesColumnExist(db, table_name, column_name) ||
         sql::AddColumn(*db, table_name, column_name, type);
}

bool DropColumnIfExists(sql::Database* db,
                        std::string_view table_name,
                        std::string_view column_name) {
  return !DoesColumnExist(db, table_name, column_name) ||
         sql::DropColumn(*db, table_name, column_name);
}

bool DropTableIfExists(sql::Database* db, std::string_view table_name) {
  return db->Execute(base::StrCat({"DROP TABLE IF EXISTS ", table_name}));
}

bool SelectByGuid(sql::Database* db,
                  sql::Statement& statement,
                  std::string_view table_name,
                  std::initializer_list<const std::string_view> columns,
                  std::string_view guid) {
  sql::SelectBuilder(*db, statement, table_name, columns, "WHERE guid=?");
  statement.BindString(0, guid);
  return statement.is_valid() && statement.Step();
}

}  // namespace autofill
