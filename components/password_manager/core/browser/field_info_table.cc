// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/field_info_table.h"

#include "build/build_config.h"
#include "components/autofill/core/common/signatures.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace password_manager {

namespace {

constexpr char kFieldInfoTableName[] = "field_info";

#if !BUILDFLAG(IS_ANDROID)
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
    results.back().form_signature = autofill::FormSignature(
        s->ColumnInt64(GetColumnNumber(FieldInfoTableColumn::kFormSignature)));
    results.back().field_signature = autofill::FieldSignature(
        s->ColumnInt(GetColumnNumber(FieldInfoTableColumn::kFieldSignature)));
    results.back().field_type = static_cast<autofill::ServerFieldType>(
        s->ColumnInt(GetColumnNumber(FieldInfoTableColumn::kFieldType)));
    results.back().create_time =
        s->ColumnTime(GetColumnNumber(FieldInfoTableColumn::kCreateTime));
  }
  return results;
}
#endif

}  // namespace

bool operator==(const FieldInfo& lhs, const FieldInfo& rhs) {
  return lhs.form_signature == rhs.form_signature &&
         lhs.field_signature == rhs.field_signature &&
         lhs.field_type == rhs.field_type && lhs.create_time == rhs.create_time;
}

void FieldInfoTable::Init(sql::Database* db) {
  db_ = db;
#if BUILDFLAG(IS_ANDROID)
  // Local predictions on Android are not reliable, so they are not used now.
  // Remove the table which might have created in the old versions.
  // TODO(https://crbug.com/1051914): remove this after M-83.
  DropTableIfExists();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool FieldInfoTable::CreateTableIfNecessary() {
#if BUILDFLAG(IS_ANDROID)
  return true;
#else
  if (db_->DoesTableExist(kFieldInfoTableName))
    return true;
  SQLTableBuilder builder(kFieldInfoTableName);
  InitializeFieldInfoBuilder(&builder);
  return builder.CreateTable(db_);
#endif  // BUILDFLAG(IS_ANDROID)
}

bool FieldInfoTable::DropTableIfExists() {
  if (!db_->DoesTableExist(kFieldInfoTableName))
    return false;
  return db_->Execute("DROP TABLE field_info");
}

bool FieldInfoTable::AddRow(const FieldInfo& field) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR IGNORE INTO field_info "
      "(form_signature, field_signature, field_type, create_time) "
      "VALUES (?, ?, ?, ?)"));
  s.BindInt64(GetColumnNumber(FieldInfoTableColumn::kFormSignature),
              field.form_signature.value());
  s.BindInt64(GetColumnNumber(FieldInfoTableColumn::kFieldSignature),
              field.field_signature.value());
  s.BindInt(GetColumnNumber(FieldInfoTableColumn::kFieldType),
            field.field_type);
  s.BindTime(GetColumnNumber(FieldInfoTableColumn::kCreateTime),
             field.create_time);
  return s.Run();
#endif  // BUILDFLAG(IS_ANDROID)
}

bool FieldInfoTable::RemoveRowsByTime(base::Time remove_begin,
                                      base::Time remove_end) {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "DELETE FROM field_info WHERE "
                              "create_time >= ? AND create_time < ?"));
  s.BindTime(0, remove_begin);
  s.BindTime(1, remove_end);
  return s.Run();
#endif  // BUILDFLAG(IS_ANDROID)
}

std::vector<FieldInfo> FieldInfoTable::GetAllRows() {
#if BUILDFLAG(IS_ANDROID)
  return std::vector<FieldInfo>();
#else
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT form_signature, field_signature, field_type, create_time FROM "
      "field_info"));
  return StatementToFieldInfo(&s);
#endif  // BUILDFLAG(IS_ANDROID)
}

// Returns all FieldInfo from the database which have |form_signature|.
std::vector<FieldInfo> FieldInfoTable::GetAllRowsForFormSignature(
    uint64_t form_signature) {
#if BUILDFLAG(IS_ANDROID)
  return std::vector<FieldInfo>();
#else
  sql::Statement s(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT form_signature, field_signature, "
                              "field_type, create_time FROM field_info "
                              "WHERE form_signature = ?"));
  s.BindInt64(0, form_signature);
  return StatementToFieldInfo(&s);
#endif  // BUILDFLAG(IS_ANDROID)
}

}  // namespace password_manager
