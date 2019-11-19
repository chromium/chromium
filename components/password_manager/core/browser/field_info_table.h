// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_

#include "base/macros.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/field_types.h"

namespace sql {
class Database;
}

namespace password_manager {

struct FieldInfo {
  uint64_t form_signature = 0u;
  uint32_t field_signature = 0u;
  autofill::ServerFieldType field_type = autofill::UNKNOWN_TYPE;
  // The date when the record was created.
  base::Time create_time;
};

bool operator==(const FieldInfo& lhs, const FieldInfo& rhs);

class FieldInfoTable {
 public:
  FieldInfoTable() = default;
  ~FieldInfoTable() = default;

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Creates the table if it doesn't exist. Returns true if the table already
  // exists or was created successfully.
  bool CreateTableIfNecessary();

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
  sql::Database* db_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(FieldInfoTable);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_FIELD_INFO_TABLE_H_
