// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_notes_table.h"

#include <string>

#include "base/bind.h"
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

// Helper function to return a password note map from the SQL statement.
std::map<FormPrimaryKey, PasswordNote> StatementToPasswordNotes(
    sql::Statement* s) {
  std::map<FormPrimaryKey, PasswordNote> results;
  while (s->Step()) {
    std::string encrypted_value;
    s->ColumnBlobAsString(1, &encrypted_value);
    std::u16string decrypted_value;
    if (LoginDatabase::DecryptedString(encrypted_value, &decrypted_value) !=
        LoginDatabase::ENCRYPTION_RESULT_SUCCESS) {
      continue;
    }
    base::Time date_created = base::Time::FromDeltaSinceWindowsEpoch(
        base::Microseconds(s->ColumnInt64(2)));

    results.emplace(
        FormPrimaryKey(s->ColumnInt(0)),
        PasswordNote(std::move(decrypted_value), std::move(date_created)));
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
                         "date_created) VALUES (?, ?, ?, ?)",
                         kTableName)
          .c_str()));

  s.BindInt(0, parent_id.value());
  // Key column is not used but added for future compatibility.
  s.BindString(1, "");
  s.BindString(2, encrypted_value);
  s.BindInt64(3, note.date_created.ToDeltaSinceWindowsEpoch().InMicroseconds());

  return s.Run() && db_->GetLastChangeCount();
}

bool PasswordNotesTable::RemovePasswordNote(FormPrimaryKey parent_id) {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("DELETE FROM %s WHERE parent_id = ?", kTableName)
          .c_str()));
  s.BindInt(0, parent_id.value());

  return s.Run() && db_->GetLastChangeCount();
}

absl::optional<PasswordNote> PasswordNotesTable::GetPasswordNote(
    FormPrimaryKey parent_id) const {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("SELECT parent_id, value, date_created "
                                        "FROM %s WHERE parent_id = ? ",
                                        kTableName)
                         .c_str()));
  s.BindInt(0, parent_id.value());
  auto notes = StatementToPasswordNotes(&s);
  return notes.empty() ? absl::optional<PasswordNote>() : notes[parent_id];
}

std::map<FormPrimaryKey, PasswordNote>
PasswordNotesTable::GetAllPasswordNotesForTest() const {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE, base::StringPrintf("SELECT parent_id, value, date_created "
                                        "FROM %s",
                                        kTableName)
                         .c_str()));
  return StatementToPasswordNotes(&s);
}
}  // namespace password_manager
