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

bool MigrateToVersion3(sql::Database& db, sql::MetaTable& meta_table) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kNewValuesTableSql[] =
      "CREATE TABLE new_values_mapping("
      "context_origin TEXT NOT NULL,"
      "key BLOB NOT NULL,"
      "value BLOB NOT NULL,"
      "last_used_time INTEGER NOT NULL,"
      "PRIMARY KEY(context_origin,key)) WITHOUT ROWID";
  if (!db.Execute(kNewValuesTableSql)) {
    return false;
  }

  static constexpr char kSelectPreviousValuesSql[] =
      "SELECT * FROM values_mapping";
  static constexpr char kInsertIntoNewValuesSql[] =
      "INSERT INTO new_values_mapping(context_origin, key, value, "
      "last_used_time) VALUES(?,?,?,?)";

  sql::Statement select_statement(
      db.GetCachedStatement(SQL_FROM_HERE, kSelectPreviousValuesSql));

  while (select_statement.Step()) {
    sql::Statement insert_statement(
        db.GetCachedStatement(SQL_FROM_HERE, kInsertIntoNewValuesSql));
    insert_statement.BindString(0, select_statement.ColumnString(0));
    insert_statement.BindBlob(1, select_statement.ColumnString16(1));
    insert_statement.BindBlob(2, select_statement.ColumnString16(2));
    insert_statement.BindTime(3, select_statement.ColumnTime(3));

    if (!insert_statement.Run()) {
      return false;
    }
  }

  if (!select_statement.Succeeded()) {
    return false;
  }

  static constexpr char kDropOldIndexSql[] =
      "DROP INDEX IF EXISTS values_mapping_last_used_time_idx";
  if (!db.Execute(kDropOldIndexSql)) {
    return false;
  }

  static constexpr char kDropOldValuesSql[] = "DROP TABLE values_mapping";
  if (!db.Execute(kDropOldValuesSql)) {
    return false;
  }

  static constexpr char kRenameValuesMapSql[] =
      "ALTER TABLE new_values_mapping RENAME TO values_mapping";
  if (!db.Execute(kRenameValuesMapSql)) {
    return false;
  }

  static constexpr char kCreateNewIndexSql[] =
      "CREATE INDEX values_mapping_last_used_time_idx "
      "ON values_mapping(last_used_time)";
  if (!db.Execute(kCreateNewIndexSql)) {
    return false;
  }

  return meta_table.SetVersionNumber(3) && transaction.Commit();
}

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
  if (meta_table.GetVersionNumber() == 2 &&
      !MigrateToVersion3(db, meta_table)) {
    return false;
  }
  return true;
}

}  // namespace storage
