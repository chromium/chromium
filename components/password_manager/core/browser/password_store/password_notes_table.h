// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_NOTES_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_NOTES_TABLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"

namespace sql {
class Database;
}

namespace password_manager {

class EncryptDecryptInterface;

// Represents the 'password_notes' table in the Login Database.
class PasswordNotesTable {
 public:
  static const char kTableName[];

  PasswordNotesTable() = default;

  PasswordNotesTable(const PasswordNotesTable&) = delete;
  PasswordNotesTable& operator=(const PasswordNotesTable&) = delete;

  ~PasswordNotesTable() = default;

  // Initializes `db_`. `db_` should not be null and outlive this class.
  void Init(sql::Database* db,
            EncryptDecryptInterface* encrypt_decrypt_intrface);

  // Migrates this table from `current_version` to `kCurrentVersionNumber`
  // defined in the login db.
  bool MigrateTable(int current_version, bool is_account_store);

  // Adds the note if it doesn't exist.
  // If it does, it removes the previous entry and adds the new one.
  bool InsertOrReplace(FormPrimaryKey parent_id, const PasswordNote& note);

  // Removes the notes corresponding to `parent_id`.
  bool RemovePasswordNotes(FormPrimaryKey parent_id);

  // Gets the notes in the database for `parent_id`.
  std::vector<PasswordNote> GetPasswordNotes(FormPrimaryKey parent_id) const;

  std::map<FormPrimaryKey, std::vector<PasswordNote>>
  GetAllPasswordNotesForTest() const;

 private:
  raw_ptr<sql::Database> db_ = nullptr;
  raw_ptr<EncryptDecryptInterface> encrypt_decrypt_intrface_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_PASSWORD_NOTES_TABLE_H_
