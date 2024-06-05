// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/text_table.h"

#include "base/logging.h"
#include "sql/statement.h"

namespace ash::file_manager {

TextTable::TextTable(sql::Database* db, const std::string& table_name)
    : table_name_(table_name), db_(db) {}
TextTable::~TextTable() {}

bool TextTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize " << table_name_
                 << " due to closed database";
    return false;
  }
  auto create_table = MakeCreateTableStatement();
  DCHECK(create_table->is_valid()) << "Invalid create table statement: \""
                                   << create_table->GetSQLStatement() << "\"";
  if (!create_table->Run()) {
    LOG(ERROR) << "Failed to create the table";
    return false;
  }

  auto create_index = MakeCreateIndexStatement();
  if (!create_index) {
    return true;
  }
  DCHECK(create_index->is_valid()) << "Invalid create index statement: \""
                                   << create_index->GetSQLStatement() << "\"";
  if (!create_index->Run()) {
    LOG(ERROR) << "Failed to create the index";
    return false;
  }
  return true;
}

int64_t TextTable::DeleteValue(const std::string& value) {
  int64_t value_id = GetValueId(value);
  if (value_id < 0) {
    return value_id;
  }

  auto delete_value_by_id = MakeDeleteStatement();
  DCHECK(delete_value_by_id->is_valid())
      << "Invalid delete statement: \"" << delete_value_by_id->GetSQLStatement()
      << "\"";
  delete_value_by_id->BindInt64(0, value_id);
  if (!delete_value_by_id->Run()) {
    LOG(ERROR) << "Failed to delete value " << value;
    return -1;
  }
  return value_id;
}

std::optional<std::string> TextTable::GetValue(int64_t value_id) const {
  auto get_value = MakeGetValueStatement();
  DCHECK(get_value->is_valid()) << "Invalid get value statement: \""
                                << get_value->GetSQLStatement() << "\"";
  get_value->BindInt64(0, value_id);
  if (get_value->Step()) {
    return get_value->ColumnString(0);
  }
  return std::nullopt;
}

int64_t TextTable::ChangeValue(const std::string& from, const std::string& to) {
  if (from == to) {
    return GetValueId(from);
  }
  auto change_value = MakeChangeValueStatement();
  DCHECK(change_value->is_valid()) << "Invalid change value statement: \""
                                   << change_value->GetSQLStatement() << "\"";
  change_value->BindString(0, to);
  change_value->BindString(1, from);
  if (change_value->Step()) {
    return change_value->ColumnInt64(0);
  }
  return -1;
}

int64_t TextTable::GetValueId(const std::string& value) const {
  auto get_value_id = MakeGetValueIdStatement();
  DCHECK(get_value_id->is_valid()) << "Invalid get value ID statement: \""
                                   << get_value_id->GetSQLStatement() << "\"";
  get_value_id->BindString(0, value);
  if (get_value_id->Step()) {
    return get_value_id->ColumnInt64(0);
  }
  return -1;
}

int64_t TextTable::GetOrCreateValueId(const std::string& value) {
  int64_t value_id = GetValueId(value);
  if (value_id != -1) {
    return value_id;
  }
  auto insert_value = MakeInsertStatement();
  DCHECK(insert_value->is_valid()) << "Invalid insert term statement: \""
                                   << insert_value->GetSQLStatement() << "\"";
  insert_value->BindString(0, value);
  if (insert_value->Step()) {
    return insert_value->ColumnInt64(0);
  }
  LOG(ERROR) << "Failed to insert value " << value;
  return -1;
}

}  // namespace ash::file_manager
