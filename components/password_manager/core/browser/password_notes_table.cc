// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_notes_table.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_store_sync.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace password_manager {
namespace {

// Helper function to return a password notes map from the SQL statement.
std::map<FormPrimaryKey, std::vector<PasswordNote>> StatementToPasswordNotes(
    sql::Statement* s) {
  std::map<FormPrimaryKey, std::vector<PasswordNote>> results;
  while (s->Step()) {
    std::u16string unique_display_name = s->ColumnString16(1);
    std::string encrypted_value;
    s->ColumnBlobAsString(2, &encrypted_value);
    std::u16string decrypted_value;
    if (LoginDatabase::DecryptedString(encrypted_value, &decrypted_value) !=
        LoginDatabase::ENCRYPTION_RESULT_SUCCESS) {
      continue;
    }
    base::Time date_created = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s->ColumnInt64(3)));
    bool hide_by_default = s->ColumnBool(4);

    std::vector<PasswordNote>& notes = results[FormPrimaryKey(s->ColumnInt(0))];
    notes.emplace_back(std::move(unique_display_name),
                       std::move(decrypted_value), date_created,
                       hide_by_default);
  }
  return results;
}

}  // namespace

const char PasswordNotesTable::kTableName[] = "password_notes";

void PasswordNotesTable::Init(sql::Database* db) {
  db_ = db;
}

bool PasswordNotesTable::InsertOrReplace(FormPrimaryKey parent_id,
                                         const PasswordNote& note) {
  DCHECK(db_);
  std::string encrypted_value;
  if (LoginDatabase::EncryptedString(note.value, &encrypted_value) !=
      LoginDatabase::ENCRYPTION_RESULT_SUCCESS) {
    return false;
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT OR REPLACE INTO %s (parent_id, key, value, "
                         "date_created, confidential) VALUES (?, ?, ?, ?, ?)",
                         kTableName)
          .c_str()));

  s.BindInt(0, parent_id.value());
  s.BindString16(1, note.unique_display_name);
  s.BindString(2, encrypted_value);
  s.BindInt64(3, note.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());
  s.BindBool(4, note.hide_by_default);

  return s.Run() && db_->GetLastChangeCount();
}

bool PasswordNotesTable::RemovePasswordNotes(FormPrimaryKey parent_id) {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE parent_id = ?", kTableName)
          .c_str()));
  s.BindInt(0, parent_id.value());

  return s.Run() && db_->GetLastChangeCount();
}

std::vector<PasswordNote> PasswordNotesTable::GetPasswordNotes(
    FormPrimaryKey parent_id) const {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT parent_id, key, value, date_created, confidential "
          "FROM %s WHERE parent_id = ? ",
          kTableName)
          .c_str()));
  s.BindInt(0, parent_id.value());
  return StatementToPasswordNotes(&s)[parent_id];
}

std::map<FormPrimaryKey, std::vector<PasswordNote>>
PasswordNotesTable::GetAllPasswordNotesForTest() const {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT parent_id, key, value, date_created, confidential "
          "FROM %s",
          kTableName)
          .c_str()));
  return StatementToPasswordNotes(&s);
}
}  // namespace password_manager
