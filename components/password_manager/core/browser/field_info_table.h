// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/common/signatures.h"

namespace sql {
class Database;
}

namespace password_manager {

struct FieldInfo {
  autofill::FormSignature form_signature;
  autofill::FieldSignature field_signature;
  autofill::ServerFieldType field_type = autofill::UNKNOWN_TYPE;
  // The date when the record was created.
  base::Time create_time;
};

bool operator==(const FieldInfo& lhs, const FieldInfo& rhs);

// Manages field types deduced from the user local actions. On Android these
// types are not reliable, so this class does nothing.
class FieldInfoTable {
 public:
  FieldInfoTable() = default;

  FieldInfoTable(const FieldInfoTable&) = delete;
  FieldInfoTable& operator=(const FieldInfoTable&) = delete;

  ~FieldInfoTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Creates the table if it doesn't exist. Returns true if the table already
  // exists or was created successfully.
  bool CreateTableIfNecessary();

  bool DropTableIfExists();

  // Adds information about the field. Returns true if the SQL completed
  // successfully.
  bool AddRow(const FieldInfo& field);

  // Removes all records created between |remove_begin| inclusive and
  // |remove_end| exclusive.
  bool RemoveRowsByTime(base::Time remove_begin, base::Time remove_end);

  // Returns all FieldInfo from the database.
  std::vector<FieldInfo> GetAllRows();

  // Returns all FieldInfo from the database which have |form_signature|.
  std::vector<FieldInfo> GetAllRowsForFormSignature(uint64_t form_signature);

 private:
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_
