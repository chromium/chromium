// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database_migrations.h"

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace storage {

namespace {

bool MigrateToVersion2(sql::Database& db,
                       sql::MetaTable& meta_table,
                       base::Clock* clock) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  static constexpr char kNewValuesTableSql[] =
      "CREATE TABLE new_values_mapping("
      "context_origin TEXT NOT NULL,"
      "key TEXT NOT NULL,"
      "value TEXT,"
      "last_used_time INTEGER NOT NULL,"
      "PRIMARY KEY(context_origin,key)) WITHOUT ROWID";
  if (!db.Execute(kNewValuesTableSql))
    return false;

  static constexpr char kInsertSql[] =
      "INSERT INTO new_values_mapping(context_origin, key, value, "
      "last_used_time) "
      "SELECT context_origin, key, value, ? "
      "FROM values_mapping";

  sql::Statement statement(db.GetCachedStatement(SQL_FROM_HERE, kInsertSql));
  statement.BindTime(0, clock->Now());
  if (!statement.Run())
    return false;

  static constexpr char kDropOldValuesSql[] = "DROP TABLE values_mapping";
  if (!db.Execute(kDropOldValuesSql))
    return false;

  static constexpr char kRenameValuesMapSql[] =
      "ALTER TABLE new_values_mapping RENAME TO values_mapping";
  if (!db.Execute(kRenameValuesMapSql))
    return false;

  static constexpr char kRenameCreationColumnSql[] =
      "ALTER TABLE per_origin_mapping RENAME COLUMN last_used_time TO "
      "creation_time";
  if (!db.Execute(kRenameCreationColumnSql))
    return false;

  static constexpr char kAddValuesLastUsedTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS values_mapping_last_used_time_idx "
      "ON values_mapping(last_used_time)";
  if (!db.Execute(kAddValuesLastUsedTimeIndexSql))
    return false;

  static constexpr char kAddCreationTimeIndexSql[] =
      "CREATE INDEX IF NOT EXISTS per_origin_mapping_creation_time_idx "
      "ON per_origin_mapping(creation_time)";
  if (!db.Execute(kAddCreationTimeIndexSql))
    return false;

  static constexpr char kDropLastUsedTimeIndexSql[] =
      "DROP INDEX IF EXISTS per_origin_mapping_last_used_time_idx";
  if (!db.Execute(kDropLastUsedTimeIndexSql))
    return false;

  return meta_table.SetVersionNumber(2) && transaction.Commit();
}

}  // namespace

bool UpgradeSharedStorageDatabaseSchema(sql::Database& db,
                                        sql::MetaTable& meta_table,
                                        base::Clock* clock) {
  if (meta_table.GetVersionNumber() == 1 &&
      !MigrateToVersion2(db, meta_table, clock)) {
    return false;
  }

  return true;
}

}  // namespace storage
