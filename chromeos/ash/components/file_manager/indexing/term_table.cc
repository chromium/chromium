// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/file_manager/indexing/term_table.h"

#include "sql/statement.h"

namespace ash::file_manager {

namespace {

#define TERM_TABLE "term_table"
#define TERM_ID "term_id"
#define FIELD_NAME "field_name"
#define TOKEN_ID "token_id"

// The statement used to create the term table.
static constexpr char kCreateTermTableQuery[] =
    // clang-format off
    "CREATE TABLE IF NOT EXISTS " TERM_TABLE "("
      TERM_ID " INTEGER PRIMARY KEY AUTOINCREMENT,"
      FIELD_NAME " TEXT NOT NULL,"
      TOKEN_ID " INTEGER NOT NULL REFERENCES term_table(token_id),"
      "UNIQUE(" FIELD_NAME ", " TOKEN_ID "))";
// clang-format on

// The statement used to insert a new term into the table.
static constexpr char kInsertTermQuery[] =
    // clang-format off
    "INSERT OR REPLACE INTO " TERM_TABLE "(" FIELD_NAME ", "
    TOKEN_ID ") VALUES (?, ?) RETURNING " TERM_ID;
// clang-format on

// The statement used to delete an term ID from the database by term_id.
static constexpr char kDeleteTermQuery[] =
    // clang-format off
    "DELETE FROM " TERM_TABLE " WHERE " TERM_ID "=? "
    "RETURNING " TERM_ID;
// clang-format on

// The statement used fetch the term ID by field name and token ID.
static constexpr char kGetTermIdQuery[] =
    // clang-format off
    "SELECT " TERM_ID " FROM " TERM_TABLE " "
    "WHERE " FIELD_NAME "=? AND " TOKEN_ID "=?";
// clang-format on

}  // namespace

TermTable::TermTable(sql::Database* db) : db_(db) {}
TermTable::~TermTable() = default;

bool TermTable::Init() {
  if (!db_->is_open()) {
    LOG(WARNING) << "Faield to initialize term_table "
                 << "due to closed database";
    return false;
  }
  sql::Statement create_table(
      db_->GetCachedStatement(SQL_FROM_HERE, kCreateTermTableQuery));
  DCHECK(create_table.is_valid()) << "Invalid create the table statement: \""
                                  << create_table.GetSQLStatement() << "\"";
  if (!create_table.Run()) {
    LOG(ERROR) << "Failed to create term_table";
    return false;
  }
  return true;
}

int64_t TermTable::GetTermId(const std::string& field_name,
                             int64_t token_id) const {
  sql::Statement get_term_id(
      db_->GetCachedStatement(SQL_FROM_HERE, kGetTermIdQuery));
  DCHECK(get_term_id.is_valid()) << "Invalid get term ID statement: \""
                                 << get_term_id.GetSQLStatement() << "\"";
  get_term_id.BindString(0, field_name);
  get_term_id.BindInt64(1, token_id);
  if (get_term_id.Step()) {
    return get_term_id.ColumnInt64(0);
  }
  return -1;
}

int64_t TermTable::GetOrCreateTermId(const std::string& field_name,
                                     int64_t token_id) {
  int64_t term_id = GetTermId(field_name, token_id);
  if (term_id != -1) {
    return term_id;
  }
  sql::Statement insert_term(
      db_->GetCachedStatement(SQL_FROM_HERE, kInsertTermQuery));
  DCHECK(insert_term.is_valid()) << "Invalid insert term statement: \""
                                 << insert_term.GetSQLStatement() << "\"";
  insert_term.BindString(0, field_name);
  insert_term.BindInt64(1, token_id);
  if (insert_term.Step()) {
    return insert_term.ColumnInt64(0);
  }
  LOG(ERROR) << "Failed to insert term " << field_name << ":" << token_id;
  return -1;
}

int64_t TermTable::DeleteTermById(int64_t term_id) {
  sql::Statement delete_term(
      db_->GetCachedStatement(SQL_FROM_HERE, kDeleteTermQuery));
  DCHECK(delete_term.is_valid()) << "Invalid delete term statement: \""
                                 << delete_term.GetSQLStatement() << "\"";
  delete_term.BindInt64(0, term_id);
  if (!delete_term.Step()) {
    if (!delete_term.Succeeded()) {
      LOG(ERROR) << "Failed to delete term " << term_id;
    }
    return -1;
  }
  return delete_term.ColumnInt64(0);
}

}  // namespace ash::file_manager
