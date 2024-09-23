// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/shared_storage/shared_storage_database_migrations.h"

#include "net/base/schemeful_site.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace storage {

namespace {

bool MigrateToVersion6(sql::Database& db, sql::MetaTable& meta_table) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kNewOriginTableSql[] =
      "CREATE TABLE new_per_origin_mapping("
      "context_origin TEXT NOT NULL PRIMARY KEY,"
      "creation_time INTEGER NOT NULL,"
      "num_bytes INTEGER NOT NULL) WITHOUT ROWID";
  if (!db.Execute(kNewOriginTableSql)) {
    return false;
  }

  static constexpr char kInsertSql[] =
      "INSERT INTO new_per_origin_mapping(context_origin, creation_time, "
      "num_bytes) "
      "SELECT context_origin, creation_time, num_bytes "
      "FROM per_origin_mapping";
  if (!db.Execute(kInsertSql)) {
    return false;
  }

  static constexpr char kDropOldOriginTableSql[] =
      "DROP TABLE per_origin_mapping";
  if (!db.Execute(kDropOldOriginTableSql)) {
    return false;
  }

  static constexpr char kRenameOriginTableSql[] =
      "ALTER TABLE new_per_origin_mapping RENAME TO per_origin_mapping";
  if (!db.Execute(kRenameOriginTableSql)) {
    return false;
  }

  static constexpr char kDropOldCreationTimeIndexSql[] =
      "DROP INDEX IF EXISTS per_origin_mapping_creation_time_idx";
  if (!db.Execute(kDropOldCreationTimeIndexSql)) {
    return false;
  }

  static constexpr char kAddCreationTimeIndexSql[] =
      "CREATE INDEX per_origin_mapping_creation_time_idx "
      "ON per_origin_mapping(creation_time)";
  if (!db.Execute(kAddCreationTimeIndexSql)) {
    return false;
  }

  return meta_table.SetVersionNumber(6) && transaction.Commit();
}

bool MigrateToVersion5(sql::Database& db, sql::MetaTable& meta_table) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kNewOriginTableSql[] =
      "CREATE TABLE new_per_origin_mapping("
      "context_origin TEXT NOT NULL PRIMARY KEY,"
      "creation_time INTEGER NOT NULL,"
      "length INTEGER NOT NULL,"
      "num_bytes INTEGER NOT NULL) WITHOUT ROWID";
  if (!db.Execute(kNewOriginTableSql)) {
    return false;
  }

  static constexpr char kInsertSql[] =
      "INSERT INTO new_per_origin_mapping(context_origin, creation_time, "
      "length, num_bytes) "
      "SELECT p.context_origin, p.creation_time, p.length, "
      "SUM(LENGTH(v.key) + LENGTH(v.value)) "
      "FROM per_origin_mapping p INNER JOIN values_mapping v "
      "ON p.context_origin == v.context_origin "
      "GROUP BY v.context_origin";
  if (!db.Execute(kInsertSql)) {
    return false;
  }

  static constexpr char kDropOldOriginTableSql[] =
      "DROP TABLE per_origin_mapping";
  if (!db.Execute(kDropOldOriginTableSql)) {
    return false;
  }

  static constexpr char kRenameOriginTableSql[] =
      "ALTER TABLE new_per_origin_mapping RENAME TO per_origin_mapping";
  if (!db.Execute(kRenameOriginTableSql)) {
    return false;
  }

  static constexpr char kDropOldCreationTimeIndexSql[] =
      "DROP INDEX IF EXISTS per_origin_mapping_creation_time_idx";
  if (!db.Execute(kDropOldCreationTimeIndexSql)) {
    return false;
  }

  static constexpr char kAddCreationTimeIndexSql[] =
      "CREATE INDEX per_origin_mapping_creation_time_idx "
      "ON per_origin_mapping(creation_time)";
  if (!db.Execute(kAddCreationTimeIndexSql)) {
    return false;
  }

  return meta_table.SetVersionNumber(5) && transaction.Commit();
}

bool MigrateToVersion4(sql::Database& db, sql::MetaTable& meta_table) {
  static constexpr char kCreateNewIndexSql[] =
      "CREATE INDEX IF NOT EXISTS budget_mapping_site_time_stamp_idx "
      "ON budget_mapping(context_site,time_stamp)";

  if (db.DoesColumnExist("budget_mapping", "context_site")) {
    // This situation can occur if we migrate a Version 1 database without the
    // `budget_mapping` table (since we added this table without doing a
    // database migration).
    CHECK(!db.DoesColumnExist("budget_mapping", "context_origin"));
    CHECK(!db.DoesIndexExist("budget_mapping_origin_time_stamp_idx"));

    sql::Transaction transaction(&db);
    if (!transaction.Begin()) {
      return false;
    }

    if (!db.Execute(kCreateNewIndexSql)) {
      return false;
    }

    return meta_table.SetVersionNumber(4) && transaction.Commit();
  }

  CHECK(db.DoesColumnExist("budget_mapping", "context_origin"));

  sql::Transaction transaction(&db);
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kNewBudgetMappingSql[] =
      "CREATE TABLE new_budget_mapping("
      "id INTEGER NOT NULL PRIMARY KEY,"
      "context_site TEXT NOT NULL,"
      "time_stamp INTEGER NOT NULL,"
      "bits_debit REAL NOT NULL)";
  if (!db.Execute(kNewBudgetMappingSql)) {
    return false;
  }

  static constexpr char kSelectPreviousBudgetSql[] =
      "SELECT id,context_origin,time_stamp,bits_debit FROM budget_mapping";
  static constexpr char kInsertIntoNewBudgetSql[] =
      "INSERT INTO new_budget_mapping(id, context_site, time_stamp, "
      "bits_debit) VALUES(?,?,?,?)";

  sql::Statement select_statement(
      db.GetCachedStatement(SQL_FROM_HERE, kSelectPreviousBudgetSql));

  while (select_statement.Step()) {
    net::SchemefulSite context_site =
        net::SchemefulSite::Deserialize(select_statement.ColumnString(1));
    if (context_site.opaque() ||
        context_site.GetURL().scheme() == url::kFileScheme) {
      continue;
    }

    sql::Statement insert_statement(
        db.GetCachedStatement(SQL_FROM_HERE, kInsertIntoNewBudgetSql));
    insert_statement.BindInt64(0, select_statement.ColumnInt64(0));
    insert_statement.BindString(1, context_site.Serialize());
    insert_statement.BindTime(2, select_statement.ColumnTime(2));
    insert_statement.BindDouble(3, select_statement.ColumnDouble(3));

    if (!insert_statement.Run()) {
      return false;
    }
  }

  if (!select_statement.Succeeded()) {
    return false;
  }

  static constexpr char kDropOldIndexSql[] =
      "DROP INDEX IF EXISTS budget_mapping_origin_time_stamp_idx";
  if (!db.Execute(kDropOldIndexSql)) {
    return false;
  }

  static constexpr char kDropOldBudgetSql[] = "DROP TABLE budget_mapping";
  if (!db.Execute(kDropOldBudgetSql)) {
    return false;
  }

  static constexpr char kRenameBudgetMapSql[] =
      "ALTER TABLE new_budget_mapping RENAME TO budget_mapping";
  if (!db.Execute(kRenameBudgetMapSql)) {
    return false;
  }

  if (!db.Execute(kCreateNewIndexSql)) {
    return false;
  }

  return meta_table.SetVersionNumber(4) && transaction.Commit();
}

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
  if (meta_table.GetVersionNumber() == 3 &&
      !MigrateToVersion4(db, meta_table)) {
    return false;
  }
  if (meta_table.GetVersionNumber() == 4 &&
      !MigrateToVersion5(db, meta_table)) {
    return false;
  }
  if (meta_table.GetVersionNumber() == 5 &&
      !MigrateToVersion6(db, meta_table)) {
    return false;
  }
  return true;
}

}  // namespace storage
