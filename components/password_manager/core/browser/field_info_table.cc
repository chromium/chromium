// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_table.h"

#include "components/password_manager/core/browser/sql_table_builder.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {

namespace {

constexpr char kFieldInfoTableName[] = "field_info";

// Represents columns of the FieldInfoTable. Used with SQL queries that use all
// the columns.
enum class FieldInfoTableColumn {
  kFormSignature,
  kFieldSignature,
  kFieldType,
  kCreateTime,
};

// Casts the field info table column enum to its integer value.
int GetColumnNumber(FieldInfoTableColumn column) {
  return static_cast<int>(column);
}

// Teaches |builder| about the different DB schemes in different versions.
void InitializeFieldInfoBuilder(SQLTableBuilder* builder) {
  // Version 0.
  builder->AddColumnToUniqueKey("form_signature", "INTEGER NOT NULL");
  builder->AddColumnToUniqueKey("field_signature", "INTEGER NOT NULL");
  builder->AddColumn("field_type", "INTEGER NOT NULL");
  builder->AddColumn("create_time", "INTEGER NOT NULL");
  builder->AddIndex("field_info_index", {"form_signature", "field_signature"});
  builder->SealVersion();
}

// Returns a FieldInfo vector from the SQL statement.
std::vector<FieldInfo> StatementToFieldInfo(sql::Statement* s) {
  std::vector<FieldInfo> results;
  while (s->Step()) {
    results.emplace_back();
    results.back().form_signature =
        s->ColumnInt64(GetColumnNumber(FieldInfoTableColumn::kFormSignature));
    results.back().field_signature =
        s->ColumnInt(GetColumnNumber(FieldInfoTableColumn::kFieldSignature));
    results.back().field_type = static_cast<autofill::ServerFieldType>(
        s->ColumnInt(GetColumnNumber(FieldInfoTableColumn::kFieldType)));
    results.back().create_time = base::Time::FromDeltaSinceWindowsEpoch(
        (base::TimeDelta::FromMicroseconds(s->ColumnInt64(
            GetColumnNumber(FieldInfoTableColumn::kCreateTime)))));
  }
  return results;
}

}  // namespace

bool operator==(const FieldInfo& lhs, const FieldInfo& rhs) {
  return lhs.form_signature == rhs.form_signature &&
         lhs.field_signature == rhs.field_signature &&
         lhs.field_type == rhs.field_type && lhs.create_time == rhs.create_time;
}

void FieldInfoTable::Init(sql::Database* db) {
  db_ = db;
}

bool FieldInfoTable::CreateTableIfNecessary() {
  if (db_->DoesTableExist(kFieldInfoTableName))
    return true;
  SQLTableBuilder builder(kFieldInfoTableName);
  InitializeFieldInfoBuilder(&builder);
  return builder.CreateTable(db_);
}

bool FieldInfoTable::AddRow(const FieldInfo& field) {
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO field_info "
      "(form_signature, field_signature, field_type, create_time) "
      "VALUES (?, ?, ?, ?)"));
  s.BindInt64(GetColumnNumber(FieldInfoTableColumn::kFormSignature),
              field.form_signature);
  s.BindInt(GetColumnNumber(FieldInfoTableColumn::kFieldSignature),
            field.field_signature);
  s.BindInt(GetColumnNumber(FieldInfoTableColumn::kFieldType),
            field.field_type);
  s.BindInt64(GetColumnNumber(FieldInfoTableColumn::kCreateTime),
              field.create_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return s.Run();
}

bool FieldInfoTable::RemoveRowsByTime(base::Time remove_begin,
                                      base::Time remove_end) {
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "DELETE FROM field_info WHERE "
                              "create_time >= ? AND create_time < ?"));
  s.BindInt64(0, remove_begin.ToDeltaSinceWindowsEpoch().InMicroseconds());
  s.BindInt64(1, remove_end.ToDeltaSinceWindowsEpoch().InMicroseconds());
  return s.Run();
}

std::vector<FieldInfo> FieldInfoTable::GetAllRows() {
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT form_signature, field_signature, field_type, create_time FROM "
      "field_info"));
  return StatementToFieldInfo(&s);
}

// Returns all FieldInfo from the database which have |form_signature|.
std::vector<FieldInfo> FieldInfoTable::GetAllRowsForFormSignature(
    uint64_t form_signature) {
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT form_signature, field_signature, "
                              "field_type, create_time FROM field_info "
                              "WHERE form_signature = ?"));
  s.BindInt64(0, form_signature);
  return StatementToFieldInfo(&s);
}

}  // namespace password_manager
