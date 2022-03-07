// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_NOTES_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_NOTES_TABLE_H_

#include <string>

#include "base/time/time.h"
#include "components/password_manager/core/browser/password_form.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace sql {
class Database;
}

namespace password_manager {

// Represents a note attached to a particular credential.
struct PasswordNote {
  PasswordNote();
  PasswordNote(std::u16string value, base::Time date_created);
  PasswordNote(const PasswordNote& rhs);
  PasswordNote(PasswordNote&& rhs);
  PasswordNote& operator=(const PasswordNote& rhs);
  PasswordNote& operator=(PasswordNote&& rhs);
  ~PasswordNote();

  // The value of the note.
  std::u16string value;
  // The date when the note was created.
  base::Time date_created;
};

bool operator==(const PasswordNote& lhs, const PasswordNote& rhs);

// Represents the 'password_notes' table in the Login Database.
class PasswordNotesTable {
 public:
  static const char kTableName[];

  PasswordNotesTable() = default;

  PasswordNotesTable(const PasswordNotesTable&) = delete;
  PasswordNotesTable& operator=(const PasswordNotesTable&) = delete;

  ~PasswordNotesTable() = default;

  // Initializes `db_`. `db_` should not be null and outlive this class.
  void Init(sql::Database* db);

  // Adds the note if it doesn't exist.
  // If it does, it removes the previous entry and adds the new one.
  // Note that it sets the key column as empty string.
  bool InsertOrReplace(FormPrimaryKey parent_id, const PasswordNote& note);

  // Removes the note corresponding to `parent_id`.
  bool RemovePasswordNote(FormPrimaryKey parent_id);

  // Gets the note in the database for `parent_id`.
  absl::optional<PasswordNote> GetPasswordNote(FormPrimaryKey parent_id) const;

  std::map<FormPrimaryKey, PasswordNote> GetAllPasswordNotesForTest() const;

 private:
  sql::Database* db_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_NOTES_TABLE_H_
