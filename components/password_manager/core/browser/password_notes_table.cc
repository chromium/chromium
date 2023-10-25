// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_notes_table.h"

#include <string>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/login_database.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/sql_table_builder.h"
#include "components/password_manager/core/browser/sync/password_store_sync.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_IOS)
#import <Security/Security.h>
#endif  // BUILDFLAG(IS_IOS)

namespace password_manager {
namespace {

#if BUILDFLAG(IS_IOS)
using metrics_util::PasswordNotesMigrationToOSCrypt;
using metrics_util::RecordPasswordNotesMigrationToOSCryptStatus;
#endif

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

bool PasswordNotesTable::MigrateTable(int current_version,
                                      bool is_account_store) {
  CHECK(db_);
  CHECK(db_->DoesTableExist(kTableName));

#if BUILDFLAG(IS_IOS)
  if (current_version < 40) {
    RecordPasswordNotesMigrationToOSCryptStatus(
        is_account_store, PasswordNotesMigrationToOSCrypt::kStarted);
    // In version 39 passwords encryption on iOS was migrated to OSCrypt.
    // In version 40 password notes encryption on iOS is migrated as well.
    sql::Statement get_notes_statement(
        db_->GetUniqueStatement("SELECT id, value FROM password_notes"));

    int deleted_notes = 0, migrated_notes = 0;
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
          RecordPasswordNotesMigrationToOSCryptStatus(
              is_account_store,
              PasswordNotesMigrationToOSCrypt::kFailedToDelete);
          return false;
        }
        deleted_notes++;
      } else if (retrieval_status != errSecSuccess) {
        // Stop migration with any other error.
        base::UmaHistogramSparse(
            base::StrCat({"PasswordManager.PasswordNotesMigrationToOSCrypt.",
                          is_account_store ? "AccountStore" : "ProfileStore",
                          ".KeychainRetrievalError"}),
            static_cast<int>(retrieval_status));
        RecordPasswordNotesMigrationToOSCryptStatus(
            is_account_store,
            PasswordNotesMigrationToOSCrypt::kFailedToDecryptFromKeychain);
        return false;
      } else {
        // Encrypt note using OSCrypt.
        std::string encrypted_note;
        if (LoginDatabase::EncryptedString(plaintext_note, &encrypted_note) !=
            LoginDatabase::ENCRYPTION_RESULT_SUCCESS) {
          RecordPasswordNotesMigrationToOSCryptStatus(
              is_account_store,
              PasswordNotesMigrationToOSCrypt::kFailedToEncrypt);
          return false;
        }

        // Updated note in the database.
        sql::Statement password_note_update(db_->GetUniqueStatement(
            "UPDATE password_notes SET value = ? WHERE id = ?"));
        password_note_update.BindBlob(0, encrypted_note);
        password_note_update.BindInt(1, id);
        if (!password_note_update.Run()) {
          RecordPasswordNotesMigrationToOSCryptStatus(
              is_account_store,
              PasswordNotesMigrationToOSCrypt::kFailedToUpdate);
          return false;
        }
        migrated_notes++;
      }
    }

    RecordPasswordNotesMigrationToOSCryptStatus(
        is_account_store, PasswordNotesMigrationToOSCrypt::kSuccess);
    base::StringPiece infix_for_store =
        is_account_store ? "AccountStore" : "ProfileStore";
    base::UmaHistogramCounts1000(
        base::StrCat({"PasswordManager.PasswordNotesMigrationToOSCrypt.",
                      infix_for_store, ".DeletedNotesCount"}),
        deleted_notes);
    base::UmaHistogramCounts1000(
        base::StrCat({"PasswordManager.PasswordNotesMigrationToOSCrypt.",
                      infix_for_store, ".MigratedNotesCount"}),
        migrated_notes);
  }
#endif
  return true;
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
