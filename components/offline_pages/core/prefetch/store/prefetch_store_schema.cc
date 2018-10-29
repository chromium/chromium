// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_schema.h"

#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace offline_pages {

// Schema versions changelog:
// * 1: Initial version with prefetch_items and prefetch_downloader_quota
//   tables.
// * 2: Changes prefetch_items.file_size to have a default value of -1 (instead
//   of 0).
// * 3: Add thumbnail_url, favicon_url, snippet, and attribution.

// static
constexpr int PrefetchStoreSchema::kCurrentVersion;
// static
constexpr int PrefetchStoreSchema::kCompatibleVersion;

namespace {

// TODO(https://crbug.com/765282): remove MetaTable internal values and helper
// methods once its getters and setters for version information allow the caller
// to be informed about internal errors.

// From MetaTable internals.
const char kVersionKey[] = "version";
const char kCompatibleVersionKey[] = "last_compatible_version";

const int kVersionError = -1;

bool SetVersionNumber(sql::MetaTable* meta_table, int version) {
  return meta_table->SetValue(kVersionKey, version);
}

bool SetCompatibleVersionNumber(sql::MetaTable* meta_table, int version) {
  return meta_table->SetValue(kCompatibleVersionKey, version);
}

int GetVersionNumber(sql::MetaTable* meta_table) {
  int version;
  if (meta_table->GetValue(kVersionKey, &version))
    return version;
  return kVersionError;
}

int GetCompatibleVersionNumber(sql::MetaTable* meta_table) {
  int version;
  if (meta_table->GetValue(kCompatibleVersionKey, &version))
    return version;
  return kVersionError;
}

// IMPORTANT #1: when making changes to these columns please also reflect them
// into:
// - PrefetchItem: update existing fields and all method implementations
//   (operator==, ToString, etc).
// - PrefetchItemTest, PrefetchStoreTestUtil: update test related code to cover
//   the changed set of columns and PrefetchItem members.
// - MockPrefetchItemGenerator: so that its generated items consider all fields.
// IMPORTANT #2: Commonly used columns should appear first, as SQLite can stop
//  reading the row early if later columns are not being read.
static const char kItemsTableCreationSql[] =
    // Fixed length columns come first.
    R"sql(
CREATE TABLE IF NOT EXISTS prefetch_items(
offline_id INTEGER PRIMARY KEY NOT NULL,
state INTEGER NOT NULL DEFAULT 0,
generate_bundle_attempts INTEGER NOT NULL DEFAULT 0,
get_operation_attempts INTEGER NOT NULL DEFAULT 0,
download_initiation_attempts INTEGER NOT NULL DEFAULT 0,
archive_body_length INTEGER_NOT_NULL DEFAULT -1,
creation_time INTEGER NOT NULL,
freshness_time INTEGER NOT NULL,
error_code INTEGER NOT NULL DEFAULT 0,
file_size INTEGER NOT NULL DEFAULT -1,
guid VARCHAR NOT NULL DEFAULT '',
client_namespace VARCHAR NOT NULL DEFAULT '',
client_id VARCHAR NOT NULL DEFAULT '',
requested_url VARCHAR NOT NULL DEFAULT '',
final_archived_url VARCHAR NOT NULL DEFAULT '',
operation_name VARCHAR NOT NULL DEFAULT '',
archive_body_name VARCHAR NOT NULL DEFAULT '',
title VARCHAR NOT NULL DEFAULT '',
file_path VARCHAR NOT NULL DEFAULT '',
thumbnail_url VARCHAR NOT NULL DEFAULT '',
favicon_url VARCHAR NOT NULL DEFAULT '',
snippet VARCHAR NOT NULL DEFAULT '',
attribution VARCHAR NOT NULL DEFAULT ''
)
)sql";

bool CreatePrefetchItemsTable(sql::Database* db) {
  return db->Execute(kItemsTableCreationSql);
}

static const char kQuotaTableCreationSql[] =
    R"sql(
CREATE TABLE IF NOT EXISTS prefetch_downloader_quota(
quota_id INTEGER PRIMARY KEY NOT NULL DEFAULT 1,
update_time INTEGER NOT NULL,
available_quota INTEGER NOT NULL DEFAULT 0
)
)sql";

bool CreatePrefetchQuotaTable(sql::Database* db) {
  return db->Execute(kQuotaTableCreationSql);
}

bool CreateLatestSchema(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!CreatePrefetchItemsTable(db) || !CreatePrefetchQuotaTable(db))
    return false;

  // This would be a great place to add indices when we need them.
  return transaction.Commit();
}

int MigrateFromVersion1To2(sql::Database* db, sql::MetaTable* meta_table) {
  // Version 2 simply changes the default value of file_size from 0 to -1.
  // Because SQLite doesn't support removing or modifying columns, we create
  // a new table and insert data from the old table.
  const int target_version = 2;
  const int target_compatible_version = 1;
  // 1. Rename the existing items table.
  // 2. Create the new items table.
  // 3. Copy existing rows to the new items table.
  // 4. Drop the old items table.
  static const char kVersion1ToVersion2MigrationSql[] =
      R"sql(
ALTER TABLE prefetch_items RENAME TO prefetch_items_old;

CREATE TABLE prefetch_items(
offline_id INTEGER PRIMARY KEY NOT NULL,
state INTEGER NOT NULL DEFAULT 0,
generate_bundle_attempts INTEGER NOT NULL DEFAULT 0,
get_operation_attempts INTEGER NOT NULL DEFAULT 0,
download_initiation_attempts INTEGER NOT NULL DEFAULT 0,
archive_body_length INTEGER_NOT_NULL DEFAULT -1,
creation_time INTEGER NOT NULL,
freshness_time INTEGER NOT NULL,
error_code INTEGER NOT NULL DEFAULT 0,
file_size INTEGER NOT NULL DEFAULT -1,
guid VARCHAR NOT NULL DEFAULT '',
client_namespace VARCHAR NOT NULL DEFAULT '',
client_id VARCHAR NOT NULL DEFAULT '',
requested_url VARCHAR NOT NULL DEFAULT '',
final_archived_url VARCHAR NOT NULL DEFAULT '',
operation_name VARCHAR NOT NULL DEFAULT '',
archive_body_name VARCHAR NOT NULL DEFAULT '',
title VARCHAR NOT NULL DEFAULT '',
file_path VARCHAR NOT NULL DEFAULT ''
);

INSERT INTO prefetch_items
(offline_id, state, generate_bundle_attempts, get_operation_attempts,
download_initiation_attempts, archive_body_length, creation_time,
freshness_time, error_code, file_size, guid, client_namespace,
client_id, requested_url, final_archived_url, operation_name,
archive_body_name, title, file_path)
SELECT
offline_id, state, generate_bundle_attempts, get_operation_attempts,
download_initiation_attempts, archive_body_length, creation_time,
freshness_time, error_code, file_size, guid, client_namespace,
client_id, requested_url, final_archived_url, operation_name,
archive_body_name, title, file_path
FROM prefetch_items_old;

DROP TABLE prefetch_items_old;
)sql";

  sql::Transaction transaction(db);
  if (transaction.Begin() && db->Execute(kVersion1ToVersion2MigrationSql) &&
      SetVersionNumber(meta_table, target_version) &&
      SetCompatibleVersionNumber(meta_table, target_compatible_version) &&
      transaction.Commit()) {
    return target_version;
  }

  return kVersionError;
}

int MigrateFromVersion2To3(sql::Database* db, sql::MetaTable* meta_table) {
  const int target_version = 3;
  const int target_compatible_version = 1;
  static const char k2To3Sql[] = R"sql(
ALTER TABLE prefetch_items ADD COLUMN thumbnail_url VARCHAR NOT NULL DEFAULT '';
ALTER TABLE prefetch_items ADD COLUMN favicon_url VARCHAR NOT NULL DEFAULT '';
ALTER TABLE prefetch_items ADD COLUMN snippet VARCHAR NOT NULL DEFAULT '';
ALTER TABLE prefetch_items ADD COLUMN attribution VARCHAR NOT NULL DEFAULT '';
)sql";

  sql::Transaction transaction(db);
  if (transaction.Begin() && db->Execute(k2To3Sql) &&
      SetVersionNumber(meta_table, target_version) &&
      SetCompatibleVersionNumber(meta_table, target_compatible_version) &&
      transaction.Commit())
    return target_version;

  return kVersionError;
}

// Returns true if the database has previously been initialized to a compatible
// version.
bool DatabaseIsValid(sql::Database* db,
                     sql::MetaTable* meta_table,
                     int* current_version,
                     int* compatible_version) {
  if (!sql::MetaTable::DoesTableExist(db) ||
      !meta_table->Init(db, PrefetchStoreSchema::kCurrentVersion,
                        PrefetchStoreSchema::kCompatibleVersion))
    return false;
  *compatible_version = GetCompatibleVersionNumber(meta_table);
  *current_version = GetVersionNumber(meta_table);
  return  // Sanity checks.
      *current_version != kVersionError &&
      *compatible_version != kVersionError && *current_version >= 1 &&
      *compatible_version >= 1 &&
      // This can be false when Chrome is downgraded after a db change that's
      // not backwards-compatible.
      *compatible_version <= PrefetchStoreSchema::kCurrentVersion;
}

}  // namespace

// static
bool PrefetchStoreSchema::CreateOrUpgradeIfNeeded(sql::Database* db) {
  // TODO(harringtond): Add UMA to track errors and important actions here.
  DCHECK(db);
  sql::MetaTable meta_table;
  int current_version, compatible_version;
  if (!DatabaseIsValid(db, &meta_table, &current_version,
                       &compatible_version)) {
    // Raze the database to get back to a working state. Note that this is
    // considered dangerous, and may lose data that could be recovered later.
    // The benefit of this is that we won't be stuck in a bad state. For
    // prefetch, loss of the data in the database is not terrible: prefetching
    // is best effort, and the user will get more prefetch suggestions later.
    if (!db->Raze())
      return false;
    sql::Transaction transaction(db);
    sql::MetaTable new_meta;
    return transaction.Begin() &&
           new_meta.Init(db, PrefetchStoreSchema::kCurrentVersion,
                         PrefetchStoreSchema::kCompatibleVersion) &&
           CreateLatestSchema(db) && transaction.Commit();
  }

  // Schema upgrade code starts here.
  //
  // Note #1: A series of if-else blocks was chosen to allow for more
  // flexibility in the upgrade logic than a single switch-case block would.
  // Note #2: Be very mindful when referring any constants from inside upgrade
  // code as they might change as the schema evolve whereas the upgrade code
  // should not. For instance, one should never refer to kCurrentVersion or
  // kCompatibleVersion when setting values for the current and compatible
  // versions as these are definitely going to change with each schema change.
  if (current_version == 1)
    current_version = MigrateFromVersion1To2(db, &meta_table);

  if (current_version == 2)
    current_version = MigrateFromVersion2To3(db, &meta_table);

  return current_version == kCurrentVersion;
}

// static
std::string PrefetchStoreSchema::GetItemTableCreationSqlForTesting() {
  return kItemsTableCreationSql;
}

}  // namespace offline_pages
