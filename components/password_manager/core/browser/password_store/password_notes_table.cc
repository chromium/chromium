// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_store/password_notes_table.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/affiliations/core/browser/sql_table_builder.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/encrypt_decrypt_intrface.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "sql/database.h"
#include "sql/statement.h"

#if BUILDFLAG(IS_IOS)
#import <Security/Security.h>

#include "components/password_manager/core/browser/password_store/login_database.h"
#endif  // BUILDFLAG(IS_IOS)

namespace password_manager {
namespace {

// Helper function to return a password notes map from the SQL statement.
std::map<FormPrimaryKey, std::vector<PasswordNote>> StatementToPasswordNotes(
    sql::Statement* s,
    EncryptDecryptInterface* decryptor) {
  std::map<FormPrimaryKey, std::vector<PasswordNote>> results;
  while (s->Step()) {
    std::u16string unique_display_name = s->ColumnString16(1);
    std::string encrypted_value;
    s->ColumnBlobAsString(2, &encrypted_value);
    std::u16string decrypted_value;
    if (decryptor->DecryptedString(encrypted_value, &decrypted_value) !=
        EncryptionResult::kSuccess) {
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

void PasswordNotesTable::Init(
    sql::Database* db,
    EncryptDecryptInterface* encrypt_decrypt_intrface) {
  db_ = db;
  encrypt_decrypt_intrface_ = encrypt_decrypt_intrface;
}

bool PasswordNotesTable::MigrateTable(int current_version,
                                      bool is_account_store) {
  CHECK(db_);
  CHECK(db_->DoesTableExist(kTableName));

#if BUILDFLAG(IS_IOS)
  if (current_version < 40) {
    // In version 39 passwords encryption on iOS was migrated to OSCrypt.
    // In version 40 password notes encryption on iOS is migrated as well.
    sql::Statement get_notes_statement(
        db_->GetUniqueStatement("SELECT id, value FROM password_notes"));

    // Update each note value with the new BLOB.
    while (get_notes_statement.Step()) {
      int id = get_notes_statement.ColumnInt(0);
      std::string keychain_identifier;
      get_notes_statement.ColumnBlobAsString(1, &keychain_identifier);
      if (keychain_identifier.empty()) {
        continue;
      }

      // First get decrypted note value using old method.
      std::u16string plaintext_note;
      OSStatus retrieval_status =
          GetTextFromKeychainIdentifier(keychain_identifier, &plaintext_note);

      // Note no longer exists in the keychain meaning it's lost forever. Delete
      // the entry and continue the migration.
      if (retrieval_status == errSecItemNotFound) {
        sql::Statement note_delete(
            db_->GetUniqueStatement("DELETE FROM password_notes WHERE id = ?"));
        note_delete.BindInt(0, id);
        if (!note_delete.Run()) {
          return false;
        }
      } else if (retrieval_status != errSecSuccess) {
        // Stop migration with any other error.
        return false;
      } else {
        // Encrypt note using OSCrypt.
        std::string encrypted_note;
        if (encrypt_decrypt_intrface_->EncryptedString(plaintext_note,
                                                       &encrypted_note) !=
            EncryptionResult::kSuccess) {
          return false;
        }

        // Updated note in the database.
        sql::Statement password_note_update(db_->GetUniqueStatement(
            "UPDATE password_notes SET value = ? WHERE id = ?"));
        password_note_update.BindBlob(0, encrypted_note);
        password_note_update.BindInt(1, id);
        if (!password_note_update.Run()) {
          return false;
        }
      }
    }
  }
#endif
  return true;
}

bool PasswordNotesTable::InsertOrReplace(FormPrimaryKey parent_id,
                                         const PasswordNote& note) {
  DCHECK(db_);
  std::string encrypted_value;
  if (encrypt_decrypt_intrface_->EncryptedString(
          note.value, &encrypted_value) != EncryptionResult::kSuccess) {
    return false;
  }

  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf("INSERT OR REPLACE INTO %s (parent_id, key, value, "
                         "date_created, confidential) VALUES (?, ?, ?, ?, ?)",
                         kTableName)));

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
      base::StringPrintf("DELETE FROM %s WHERE parent_id = ?", kTableName)));
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
          kTableName)));
  s.BindInt(0, parent_id.value());
  return StatementToPasswordNotes(&s,
                                  encrypt_decrypt_intrface_.get())[parent_id];
}

std::map<FormPrimaryKey, std::vector<PasswordNote>>
PasswordNotesTable::GetAllPasswordNotesForTest() const {
  DCHECK(db_);
  sql::Statement s(db_->GetCachedStatement(
      SQL_FROM_HERE,
      base::StringPrintf(
          "SELECT parent_id, key, value, date_created, confidential "
          "FROM %s",
          kTableName)));
  return StatementToPasswordNotes(&s, encrypt_decrypt_intrface_.get());
}
}  // namespace password_manager
