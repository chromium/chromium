// Copyright 2016 The Chromium Authors. All rights reserved.  Use of this source
// code is governed by a BSD-style license that can be found in the LICENSE
// file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_

#include <limits>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece.h"

namespace sql {
class Database;
}

namespace password_manager {

// Use this class to represent the versioned evolution of a SQLite table
// structure and generate creating and migrating statements for it.
//
// Usage example:
//
// SQLTableBuilder builder("logins");
//
// // First describe a couple of versions:
// builder.AddColumnToPrimaryKey("id", "INTEGER");
// builder.AddColumn("name", "VARCHAR");
// builder.AddColumn("icon", "VARCHAR");
// builder.AddColumn("password", "VARCHAR NOT NULL");
// unsigned version = builder.SealVersion();  // Version 0 is sealed.
// DCHECK_EQ(0u, version);
// builder.RenameColumn("icon", "avatar");
// version = builder.SealVersion();  // Version 1 is sealed.
// DCHECK_EQ(1u, version);
//
// // Now, assuming that |db| has a table "logins" in a state corresponding
// // version 0, this will migrate it to the latest version:
// sql::Database* db = ...;
// builder.MigrateFrom(0, db);
//
// // And assuming |db| has no table called "logins", this will create one
// // in a state corresponding the latest sealed version:
// builder.CreateTable(db);
class SQLTableBuilder {
 public:
  // Create the builder for an arbitrary table name.
  explicit SQLTableBuilder(const std::string& table_name);

  ~SQLTableBuilder();

  // Adds a column in the table description, with |name| and |type|. |name|
  // must not have been added to the table in this version before.
  void AddColumn(std::string name, std::string type);

  // As AddColumn but also adds column |name| to the primary key of the table.
  // The column must be of type "INTEGER" and will be AUTO INCREMENT. This
  // method can be called only once.
  void AddPrimaryKeyColumn(std::string name);

  // As AddColumn but also adds column |name| to the unique key of the table.
  void AddColumnToUniqueKey(std::string name, std::string type);

  // Renames column |old_name| to |new_name|. |new_name| can not exist already.
  // |old_name| must have been added in the past. Furthermore, there must be no
  // index in this version referencing |old_name|.
  void RenameColumn(const std::string& old_name, const std::string& new_name);

  // Removes column |name|. |name| must have been added in the past.
  // Furthermore, there must be no index in this version referencing |name|.
  void DropColumn(const std::string& name);

  // Adds an index in the table description, with |name| and on columns
  // |columns|. |name| must not have been added to the table in this version
  // before. Furthermore, |columns| must be non-empty, and every column
  // referenced in |columns| must be unique and exist in the current version.
  void AddIndex(std::string name, std::vector<std::string> columns);

  // Increments the internal version counter and marks the current state of the
  // table as that version. Returns the sealed version. Calling any of the
  // *Column* and *Index* methods above will result in starting a new version
  // which is not considered sealed.
  unsigned SealVersion();

  // Assuming that the database connected through |db| contains a table called
  // |table_name_| in a state described by version |old_version|, migrates it to
  // the current version, which must be sealed. Returns true on success.
  bool MigrateFrom(unsigned old_version, sql::Database* db) const;

  // If |db| connects to a database where table |table_name_| already exists,
  // this is a no-op and returns true. Otherwise, |table_name_| is created in a
  // state described by the current version known to the builder. The current
  // version must be sealed. Returns true on success. At least one call
  // to AddColumnToUniqueKey must have been done before this is called the first
  // time.
  bool CreateTable(sql::Database* db) const;

  // Returns the comma-separated list of all column names present in the last
  // version. The last version must be sealed.
  std::string ListAllColumnNames() const;

  // Same as ListAllColumnNames, but for non-unique key names only (i.e. keys
  // that are part of neither the PRIMARY KEY nor the UNIQUE constraint), and
  // with names followed by " = ?".
  std::string ListAllNonuniqueKeyNames() const;

  // Same as ListAllNonuniqueKeyNames, but for unique key names without the
  // primary key names and separated by " AND ".
  std::string ListAllUniqueKeyNames() const;

  // Returns a vector of all PRIMARY KEY names that are present in the last
  // version. The last version must be sealed.
  std::vector<base::StringPiece> AllPrimaryKeyNames() const;

  // Returns the number of all columns present in the last version. The last
  // version must be sealed.
  size_t NumberOfColumns() const;

 private:
  // Stores the information about one column (name, type, etc.).
  struct Column;

  // Stores the information about one index (name, columns, etc.).
  struct Index;

  static unsigned constexpr kInvalidVersion =
      std::numeric_limits<unsigned>::max();

  // Computes the SQL CREATE TABLE constraints for given |version|.
  std::string ComputeConstraints(unsigned version) const;

  // Assuming that the database connected through |db| contains a table called
  // |table_name_| in a state described by version |old_version|, migrates it to
  // version |old_version + 1|. The current version known to the builder must be
  // at least |old_version + 1| and sealed. Returns true on success.
  bool MigrateToNextFrom(unsigned old_version, sql::Database* db) const;

  // Assuming that the database connected through |db| contains a table called
  // |table_name_| in a state described by version |old_version|, migrates it
  // indices to version |old_version + 1|. The current version known to the
  // builder must be at least |old_version + 1| and sealed. Returns true on
  // success.
  bool MigrateIndicesToNextFrom(unsigned old_version, sql::Database* db) const;

  // Looks up column named |name| in |columns_|. If present, returns the last
  // one.
  std::vector<Column>::reverse_iterator FindLastColumnByName(
      const std::string& name);

  // Looks up index named |name| in |indices_|. If present, returns the last
  // one.
  std::vector<Index>::reverse_iterator FindLastIndexByName(
      const std::string& name);

  // Returns whether the last version is |version| and whether it was sealed
  // (by calling SealVersion with no table modifications afterwards).
  bool IsVersionLastAndSealed(unsigned version) const;

  // Whether |column| is present in the last version. The last version must be
  // sealed.
  bool IsColumnInLastVersion(const Column& column) const;

  // Whether |index| is present in the last version. The last version must be
  // sealed.
  bool IsIndexInLastVersion(const Index& index) const;

  // Last sealed version, kInvalidVersion means "none".
  unsigned sealed_version_ = kInvalidVersion;

  std::vector<Column> columns_;  // Columns of the table, across all versions.

  std::vector<Index> indices_;  // Indices of the table, across all versions.

  // The name of the table.
  const std::string table_name_;

  DISALLOW_COPY_AND_ASSIGN(SQLTableBuilder);
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_SQL_TABLE_BUILDER_H_
