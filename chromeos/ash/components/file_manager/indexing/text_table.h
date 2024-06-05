// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TEXT_TABLE_H_
#define CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TEXT_TABLE_H_

#include <memory>
#include <optional>
#include <string>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace ash::file_manager {

// A table that maintains a mapping from a unique value ID to the value text.
// This is the base class for two tables that are built on top of it: term
// table and URL table. This class is not meant to be used by itself.
class COMPONENT_EXPORT(FILE_MANAGER) TextTable {
 public:
  // Initializes the table. Returns true on success, and false on failure.
  bool Init();

 protected:
  // Creates a new table and passes the pointer to the SQL database to it. The
  // caller must make sure it owns both the sql::Database object and this table.
  // The caller also must make sure that the sql::Database outlives the table.
  TextTable(sql::Database* db, const std::string& table_name);
  virtual ~TextTable();

  TextTable(const TextTable&) = delete;
  TextTable& operator=(const TextTable&) = delete;

  // Deletes the given value from the table. Returns -1, if the value was not
  // found. Otherwise, returns the ID that the value was assigned.
  int64_t DeleteValue(const std::string& term);

  // Gets the value ID for the given value. If the value does not exists, this
  // method returns -1. Otherwise it returns the unique ID assigned to the
  // value.
  int64_t GetValueId(const std::string& value) const;

  // Gets or creates an ID for the given value. If the value does not exist,
  // it is inserted into the table and the unique key assigned to is returned.
  int64_t GetOrCreateValueId(const std::string& value);

  // Populates the value with the value for the given ID, if found. If not
  // found, returns -1 and leaves `value` unchanged.
  std::optional<std::string> GetValue(int64_t value_id) const;

  // Without changing the ID associated with the value, it changes it from
  // from string, to to string.
  int64_t ChangeValue(const std::string& from, const std::string& to);

  // To be overridden by the extending classes.

  // Returns a statement that gets text ID by value.
  virtual std::unique_ptr<sql::Statement> MakeGetValueIdStatement() const = 0;
  // Returns a statement that gets text value by its ID.
  virtual std::unique_ptr<sql::Statement> MakeGetValueStatement() const = 0;
  // Returns a statement that inserts text value into the table.
  virtual std::unique_ptr<sql::Statement> MakeInsertStatement() const = 0;
  // Returns a statement that deletes a value by its ID.
  virtual std::unique_ptr<sql::Statement> MakeDeleteStatement() const = 0;
  // Returns a statement that creates a table that maps text ID to text value.
  virtual std::unique_ptr<sql::Statement> MakeCreateTableStatement() const = 0;
  // Returns a statement that creates an index for the table; may return null.
  virtual std::unique_ptr<sql::Statement> MakeCreateIndexStatement() const = 0;
  // Returns a statement that changes a value to a new value.
  virtual std::unique_ptr<sql::Statement> MakeChangeValueStatement() const = 0;

  // The name of the concrete table built on top of this class.
  const std::string table_name_;

  // The pointer to a database owned by the whoever created this table.
  raw_ptr<sql::Database> db_;
};

}  // namespace ash::file_manager

#endif  // CHROMEOS_ASH_COMPONENTS_FILE_MANAGER_INDEXING_TEXT_TABLE_H_
