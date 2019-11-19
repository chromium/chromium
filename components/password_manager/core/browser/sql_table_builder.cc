// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/sql_table_builder.h"

#include <algorithm>
#include <set>
#include <utility>

#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "sql/database.h"
#include "sql/transaction.h"

namespace password_manager {

namespace {

// Appends |name| to |list_of_names|, separating items with ", ".
void Append(const std::string& name, std::string* list_of_names) {
  if (list_of_names->empty())
    *list_of_names = name;
  else
    *list_of_names += ", " + name;
}

}  // namespace

// static
unsigned constexpr SQLTableBuilder::kInvalidVersion;

struct SQLTableBuilder::Column {
  std::string name;
  std::string type;
  // Whether this column is the table's PRIMARY KEY.
  bool is_primary_key;
  // Whether this column is part of the table's UNIQUE constraint.
  bool part_of_unique_key;
  // The first version this column is part of.
  unsigned min_version;
  // The last version this column is part of. The value of kInvalidVersion
  // means that it is part of all versions since |min_version|.
  unsigned max_version;
  // Renaming of a column is stored as a sequence of one removed and one added
  // column in |columns_|. To distinguish it from an unrelated removal and
  // addition, the following bit is set to true for the added columns which
  // are part of renaming. Those columns will get the data of their
  // predecessors. If the bit is false, the column will be filled with the
  // default value on creation.
  bool gets_previous_data;
};

struct SQLTableBuilder::Index {
  // The name of this index.
  std::string name;
  // The names of columns this index is built from.
  std::vector<std::string> columns;
  // The first version this index is part of.
  unsigned min_version;
  // The last version this index is part of. The value of kInvalidVersion
  // means that it is part of all versions since |min_version|.
  unsigned max_version;
};

SQLTableBuilder::SQLTableBuilder(const std::string& table_name)
    : table_name_(table_name) {}

SQLTableBuilder::~SQLTableBuilder() = default;

void SQLTableBuilder::AddColumn(std::string name, std::string type) {
  DCHECK(FindLastColumnByName(name) == columns_.rend());
  columns_.push_back({std::move(name), std::move(type), false, false,
                      sealed_version_ + 1, kInvalidVersion, false});
}

void SQLTableBuilder::AddPrimaryKeyColumn(std::string name) {
  for (const Column& column : columns_) {
    DCHECK(!column.is_primary_key);
  }
  AddColumn(std::move(name), "INTEGER");
  columns_.back().is_primary_key = true;
}

void SQLTableBuilder::AddColumnToUniqueKey(std::string name, std::string type) {
  AddColumn(std::move(name), std::move(type));
  columns_.back().part_of_unique_key = true;
}

void SQLTableBuilder::RenameColumn(const std::string& old_name,
                                   const std::string& new_name) {
  auto old_column = FindLastColumnByName(old_name);
  DCHECK(old_column != columns_.rend());
  if (old_name == new_name)  // The easy case.
    return;

  DCHECK(FindLastColumnByName(new_name) == columns_.rend());
  // Check there is no index in the current version that references |old_name|.
  DCHECK(std::none_of(indices_.begin(), indices_.end(),
                      [&old_name](const Index& index) {
                        return index.max_version == kInvalidVersion &&
                               base::Contains(index.columns, old_name);
                      }));

  if (sealed_version_ != kInvalidVersion &&
      old_column->min_version <= sealed_version_) {
    // This column exists in the last sealed version. Therefore it cannot be
    // just replaced, it needs to be kept for generating the migration code.
    Column new_column = {new_name,
                         old_column->type,
                         old_column->is_primary_key,
                         old_column->part_of_unique_key,
                         sealed_version_ + 1,
                         kInvalidVersion,
                         true};
    old_column->max_version = sealed_version_;
    auto past_old =
        old_column.base();  // Points one element after |old_column|.
    columns_.insert(past_old, std::move(new_column));
  } else {
    // This column was just introduced in the currently unsealed version. To
    // rename it, it is enough just to modify the entry in columns_.
    old_column->name = new_name;
  }
}

// Removes column |name|. |name| must have been added in the past.
void SQLTableBuilder::DropColumn(const std::string& name) {
  auto column = FindLastColumnByName(name);
  DCHECK(column != columns_.rend());
  // Check there is no index in the current version that references |old_name|.
  DCHECK(std::none_of(indices_.begin(), indices_.end(),
                      [&name](const Index& index) {
                        return index.max_version == kInvalidVersion &&
                               base::Contains(index.columns, name);
                      }));
  if (sealed_version_ != kInvalidVersion &&
      column->min_version <= sealed_version_) {
    // This column exists in the last sealed version. Therefore it cannot be
    // just deleted, it needs to be kept for generating the migration code.
    column->max_version = sealed_version_;
  } else {
    // This column was just introduced in the currently unsealed version. It
    // can be just erased from |columns_|.
    columns_.erase(
        --(column.base()));  // base() points one element after |column|.
  }
}

void SQLTableBuilder::AddIndex(std::string name,
                               std::vector<std::string> columns) {
  DCHECK(!columns.empty());
  // Check if all entries of |columns| are unique.
  DCHECK_EQ(std::set<std::string>(columns.begin(), columns.end()).size(),
            columns.size());
  // |name| must not have been added before.
  DCHECK(FindLastIndexByName(name) == indices_.rend());
  // Check that all referenced columns are present in the last version by making
  // sure that the inner predicate applies to all columns names in |columns|.
  DCHECK(std::all_of(
      columns.begin(), columns.end(), [this](const std::string& column_name) {
        // Check if there is any column with the required name which is also
        // present in the latest version. Note that we don't require the last
        // version to be sealed.
        return std::any_of(columns_.begin(), columns_.end(),
                           [&column_name](const Column& column) {
                             return column.name == column_name &&
                                    column.max_version == kInvalidVersion;
                           });
      }));
  indices_.push_back({std::move(name), std::move(columns), sealed_version_ + 1,
                      kInvalidVersion});
}

std::string SQLTableBuilder::ComputeConstraints(unsigned version) const {
  std::string unique_key;
  for (const Column& column : columns_) {
    // Ignore dropped columns.
    if (column.max_version < version)
      continue;
    // Ignore columns columns from future versions.
    if (column.min_version > version)
      continue;
    if (column.part_of_unique_key)
      Append(column.name, &unique_key);
  }
  std::string constraints;
  if (!unique_key.empty())
    Append("UNIQUE (" + unique_key + ")", &constraints);
  return constraints;
}

unsigned SQLTableBuilder::SealVersion() {
  return ++sealed_version_;
}

bool SQLTableBuilder::MigrateFrom(unsigned old_version,
                                  sql::Database* db) const {
  for (; old_version < sealed_version_; ++old_version) {
    if (!MigrateToNextFrom(old_version, db))
      return false;
  }

  return true;
}

bool SQLTableBuilder::CreateTable(sql::Database* db) const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  if (db->DoesTableExist(table_name_.c_str()))
    return true;

  std::string constraints = ComputeConstraints(sealed_version_);
  DCHECK(!constraints.empty() || std::any_of(columns_.begin(), columns_.end(),
                                             [](const Column& column) {
                                               return column.is_primary_key;
                                             }));

  std::string names;  // Names and types of the current columns.
  for (const Column& column : columns_) {
    if (IsColumnInLastVersion(column)) {
      std::string suffix;
      if (column.is_primary_key)
        suffix = " PRIMARY KEY AUTOINCREMENT";
      Append(column.name + " " + column.type + suffix, &names);
    }
  }

  std::vector<std::string>
      create_index_sqls;  // CREATE INDEX statements for the current indices.
  for (const Index& index : indices_) {
    if (IsIndexInLastVersion(index)) {
      create_index_sqls.push_back(base::StringPrintf(
          "CREATE INDEX %s ON %s (%s)", index.name.c_str(), table_name_.c_str(),
          base::JoinString(index.columns, ", ").c_str()));
    }
  }

  std::string create_table_statement =
      constraints.empty()
          ? base::StringPrintf("CREATE TABLE %s (%s)", table_name_.c_str(),
                               names.c_str())
          : base::StringPrintf("CREATE TABLE %s (%s, %s)", table_name_.c_str(),
                               names.c_str(), constraints.c_str());

  sql::Transaction transaction(db);
  return transaction.Begin() && db->Execute(create_table_statement.c_str()) &&
         std::all_of(create_index_sqls.begin(), create_index_sqls.end(),
                     [&db](const std::string& sql) {
                       return db->Execute(sql.c_str());
                     }) &&
         transaction.Commit();
}

std::string SQLTableBuilder::ListAllColumnNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsColumnInLastVersion(column))
      Append(column.name, &result);
  }
  return result;
}

std::string SQLTableBuilder::ListAllNonuniqueKeyNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsColumnInLastVersion(column) &&
        !(column.is_primary_key || column.part_of_unique_key))
      Append(column.name + "=?", &result);
  }
  return result;
}

std::string SQLTableBuilder::ListAllUniqueKeyNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::string result;
  for (const Column& column : columns_) {
    if (IsColumnInLastVersion(column) && column.part_of_unique_key) {
      if (!result.empty())
        result += " AND ";
      result += column.name + "=?";
    }
  }
  return result;
}

std::vector<base::StringPiece> SQLTableBuilder::AllPrimaryKeyNames() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  std::vector<base::StringPiece> result;
  result.reserve(columns_.size());
  for (const Column& column : columns_) {
    if (IsColumnInLastVersion(column) && column.is_primary_key) {
      result.emplace_back(column.name);
    }
  }
  DCHECK(result.size() < 2);
  return result;
}

size_t SQLTableBuilder::NumberOfColumns() const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  return base::checked_cast<size_t>(std::count_if(
      columns_.begin(), columns_.end(),
      [this](const Column& column) { return IsColumnInLastVersion(column); }));
}

bool SQLTableBuilder::MigrateToNextFrom(unsigned old_version,
                                        sql::Database* db) const {
  DCHECK_LT(old_version, sealed_version_);
  DCHECK_GE(old_version, 0u);
  DCHECK(IsVersionLastAndSealed(sealed_version_));

  // Names of columns from old version, values of which are copied. This
  // contains only the names without their types.
  std::string old_names_of_existing_columns_without_types;
  // Names of columns in new version, except for added ones.
  std::string new_names_of_existing_columns;
  // Names of columns in new version, except for added ones. This contains only
  // the names without their types.
  std::string new_names_of_existing_columns_without_types;
  std::vector<std::string>
      names_of_new_columns_list;  // Names of added columns.

  // A temporary table will be needed if some columns are dropped or renamed,
  // because that is not supported by a single SQLite command.
  bool needs_temp_table = false;

  bool has_primary_key = false;

  for (auto column = columns_.begin(); column != columns_.end(); ++column) {
    if (column->max_version == old_version) {
      // This column was deleted after |old_version|. It can have two reasons:
      needs_temp_table = true;
      auto next_column = std::next(column);
      if (next_column != columns_.end() && next_column->gets_previous_data) {
        // (1) The column is being renamed.
        DCHECK_EQ(column->type, next_column->type);
        DCHECK_NE(column->name, next_column->name);
        Append(column->name, &old_names_of_existing_columns_without_types);
        Append(next_column->name + " " + next_column->type,
               &new_names_of_existing_columns);
        Append(next_column->name, &new_names_of_existing_columns_without_types);
        ++column;  // Avoid processing next_column in the next loop.
      } else {
        // (2) The column is being dropped.
      }
    } else if (column->min_version == old_version + 1) {
      // This column was added after old_version.
      if (column->is_primary_key || column->part_of_unique_key)
        needs_temp_table = true;
      std::string suffix;
      if (column->is_primary_key) {
        suffix = " PRIMARY KEY AUTOINCREMENT";
        has_primary_key = true;
      }
      names_of_new_columns_list.push_back(column->name + " " + column->type +
                                          suffix);
    } else if (column->min_version <= old_version &&
               (column->max_version == kInvalidVersion ||
                column->max_version > old_version)) {
      std::string suffix;
      if (column->is_primary_key) {
        suffix = " PRIMARY KEY AUTOINCREMENT";
        has_primary_key = true;
      }
      // This column stays.
      Append(column->name, &old_names_of_existing_columns_without_types);
      Append(column->name + " " + column->type + suffix,
             &new_names_of_existing_columns);
      Append(column->name, &new_names_of_existing_columns_without_types);
    }
  }

  if (old_names_of_existing_columns_without_types.empty()) {
    // Table didn't exist in this version, and nothing to migrate.
    return true;
  }

  if (needs_temp_table) {
    // Following the instructions from
    // https://www.sqlite.org/lang_altertable.html#otheralter, this code works
    // around the fact that SQLite does not allow dropping or renaming
    // columns. Instead, a new table is constructed, with the new column
    // names, and data from all but dropped columns from the current table are
    // copied into it. After that, the new table is renamed to the current
    // one.

    std::string constraints = ComputeConstraints(old_version + 1);
    DCHECK(has_primary_key || !constraints.empty());

    // Foreign key constraints are not enabled for the login database, so no
    // PRAGMA foreign_keys=off needed.
    const std::string temp_table_name = "temp_" + table_name_;

    std::string names_of_all_columns = new_names_of_existing_columns;
    for (const std::string& new_column : names_of_new_columns_list) {
      Append(new_column, &names_of_all_columns);
    }

    std::string create_table_statement =
        constraints.empty()
            ? base::StringPrintf("CREATE TABLE %s (%s)",
                                 temp_table_name.c_str(),
                                 names_of_all_columns.c_str())
            : base::StringPrintf(
                  "CREATE TABLE %s (%s, %s)", temp_table_name.c_str(),
                  names_of_all_columns.c_str(), constraints.c_str());

    sql::Transaction transaction(db);
    if (!(transaction.Begin() && db->Execute(create_table_statement.c_str()) &&
          db->Execute(base::StringPrintf(
                          "INSERT OR REPLACE INTO %s (%s) SELECT %s FROM %s",
                          temp_table_name.c_str(),
                          new_names_of_existing_columns_without_types.c_str(),
                          old_names_of_existing_columns_without_types.c_str(),
                          table_name_.c_str())
                          .c_str()) &&
          db->Execute(base::StringPrintf("DROP TABLE %s", table_name_.c_str())
                          .c_str()) &&
          db->Execute(base::StringPrintf("ALTER TABLE %s RENAME TO %s",
                                         temp_table_name.c_str(),
                                         table_name_.c_str())
                          .c_str()) &&
          transaction.Commit())) {
      return false;
    }
  } else if (!names_of_new_columns_list.empty()) {
    // If no new table has been created, we need to add the new columns here if
    // any.
    sql::Transaction transaction(db);
    if (!(transaction.Begin() &&
          std::all_of(names_of_new_columns_list.begin(),
                      names_of_new_columns_list.end(),
                      [this, &db](const std::string& name) {
                        return db->Execute(
                            base::StringPrintf("ALTER TABLE %s ADD COLUMN %s",
                                               table_name_.c_str(),
                                               name.c_str())
                                .c_str());
                      }) &&
          transaction.Commit())) {
      return false;
    }
  }

  return MigrateIndicesToNextFrom(old_version, db);
}

bool SQLTableBuilder::MigrateIndicesToNextFrom(unsigned old_version,
                                               sql::Database* db) const {
  DCHECK_LT(old_version, sealed_version_);
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  for (const auto& index : indices_) {
    std::string sql;
    if (index.max_version <= old_version) {
      // Index is not supposed to exist in the new version.
      sql = base::StringPrintf("DROP INDEX IF EXISTS %s", index.name.c_str());
    } else if (index.min_version <= old_version + 1) {
      // Index is supposed to exist in the new version.
      sql = base::StringPrintf("CREATE INDEX IF NOT EXISTS %s ON %s (%s)",
                               index.name.c_str(), table_name_.c_str(),
                               base::JoinString(index.columns, ", ").c_str());
    } else {
      continue;
    }

    if (!db->Execute(sql.c_str()))
      return false;
  }

  return transaction.Commit();
}

std::vector<SQLTableBuilder::Column>::reverse_iterator
SQLTableBuilder::FindLastColumnByName(const std::string& name) {
  return std::find_if(
      columns_.rbegin(), columns_.rend(),
      [&name](const Column& column) { return name == column.name; });
}

std::vector<SQLTableBuilder::Index>::reverse_iterator
SQLTableBuilder::FindLastIndexByName(const std::string& name) {
  return std::find_if(
      indices_.rbegin(), indices_.rend(),
      [&name](const Index& index) { return name == index.name; });
}

bool SQLTableBuilder::IsVersionLastAndSealed(unsigned version) const {
  // Is |version| the last sealed one?
  if (sealed_version_ != version)
    return false;
  // Is the current version the last sealed one? In other words, is there
  // neither a column or index added past the sealed version (min_version >
  // sealed) nor deleted one version after the sealed (max_version == sealed)?
  return std::none_of(columns_.begin(), columns_.end(),
                      [this](const Column& column) {
                        return column.min_version > sealed_version_ ||
                               column.max_version == sealed_version_;
                      }) &&
         std::none_of(indices_.begin(), indices_.end(),
                      [this](const Index& index) {
                        return index.min_version > sealed_version_ ||
                               index.max_version == sealed_version_;
                      });
}

bool SQLTableBuilder::IsColumnInLastVersion(const Column& column) const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  return (column.min_version <= sealed_version_ &&
          (column.max_version == kInvalidVersion ||
           column.max_version >= sealed_version_));
}

bool SQLTableBuilder::IsIndexInLastVersion(const Index& index) const {
  DCHECK(IsVersionLastAndSealed(sealed_version_));
  return (index.min_version <= sealed_version_ &&
          (index.max_version == kInvalidVersion ||
           index.max_version >= sealed_version_));
}

}  // namespace password_manager
