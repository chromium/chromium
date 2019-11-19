// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

const int OfflinePageMetadataStore::kFirstPostLegacyVersion;
const int OfflinePageMetadataStore::kCurrentVersion;
const int OfflinePageMetadataStore::kCompatibleVersion;

namespace {

// This is a macro instead of a const so that
// it can be used inline in other SQL statements below.
#define OFFLINE_PAGES_TABLE_NAME "offlinepages_v1"

void ReportStoreEvent(OfflinePagesStoreEvent event) {
  UMA_HISTOGRAM_ENUMERATION("OfflinePages.SQLStorage.StoreEvent", event);
}

bool CreateOfflinePagesTable(sql::Database* db) {
  static const char kCreateLatestOfflinePagesTableSql[] =
      "CREATE TABLE IF NOT EXISTS " OFFLINE_PAGES_TABLE_NAME
      "(offline_id INTEGER PRIMARY KEY NOT NULL,"
      " creation_time INTEGER NOT NULL,"
      " file_size INTEGER NOT NULL,"
      " last_access_time INTEGER NOT NULL,"
      " access_count INTEGER NOT NULL,"
      " system_download_id INTEGER NOT NULL DEFAULT 0,"
      " file_missing_time INTEGER NOT NULL DEFAULT 0,"
      // upgrade_attempt is deprecated, and should be removed next time the
      // schema needs to be updated.
      " upgrade_attempt INTEGER NOT NULL DEFAULT 0,"
      " client_namespace VARCHAR NOT NULL,"
      " client_id VARCHAR NOT NULL,"
      " online_url VARCHAR NOT NULL,"
      " file_path VARCHAR NOT NULL,"
      " title VARCHAR NOT NULL DEFAULT '',"
      " original_url VARCHAR NOT NULL DEFAULT '',"
      " request_origin VARCHAR NOT NULL DEFAULT '',"
      " digest VARCHAR NOT NULL DEFAULT '',"
      " snippet VARCHAR NOT NULL DEFAULT '',"
      " attribution VARCHAR NOT NULL DEFAULT ''"
      ")";
  return db->Execute(kCreateLatestOfflinePagesTableSql);
}

bool UpgradeWithQuery(sql::Database* db, const char* upgrade_sql) {
  if (!db->Execute("ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                   " RENAME TO temp_" OFFLINE_PAGES_TABLE_NAME)) {
    return false;
  }
  static const char kCreateOfflinePagesTableVersion1Sql[] =
      "CREATE TABLE IF NOT EXISTS " OFFLINE_PAGES_TABLE_NAME
      "(offline_id INTEGER PRIMARY KEY NOT NULL,"
      " creation_time INTEGER NOT NULL,"
      " file_size INTEGER NOT NULL,"
      " last_access_time INTEGER NOT NULL,"
      " access_count INTEGER NOT NULL,"
      " system_download_id INTEGER NOT NULL DEFAULT 0,"
      " file_missing_time INTEGER NOT NULL DEFAULT 0,"
      " upgrade_attempt INTEGER NOT NULL DEFAULT 0,"
      " client_namespace VARCHAR NOT NULL,"
      " client_id VARCHAR NOT NULL,"
      " online_url VARCHAR NOT NULL,"
      " file_path VARCHAR NOT NULL,"
      " title VARCHAR NOT NULL DEFAULT '',"
      " original_url VARCHAR NOT NULL DEFAULT '',"
      " request_origin VARCHAR NOT NULL DEFAULT '',"
      " digest VARCHAR NOT NULL DEFAULT ''"
      ")";
  if (!db->Execute(kCreateOfflinePagesTableVersion1Sql))
    return false;
  if (!db->Execute(upgrade_sql))
    return false;
  if (!db->Execute("DROP TABLE IF EXISTS temp_" OFFLINE_PAGES_TABLE_NAME))
    return false;
  return true;
}

bool UpgradeFrom52(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, "
      "online_url, file_path) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, "
      "online_url, file_path "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom53(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom54(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom55(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom56(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom57(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool UpgradeFrom61(sql::Database* db) {
  static const char kSql[] =
      "INSERT INTO " OFFLINE_PAGES_TABLE_NAME
      " (offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url, request_origin) "
      "SELECT "
      "offline_id, creation_time, file_size, last_access_time, "
      "access_count, client_namespace, client_id, online_url, "
      "file_path, title, original_url, request_origin "
      "FROM temp_" OFFLINE_PAGES_TABLE_NAME;
  return UpgradeWithQuery(db, kSql);
}

bool CreatePageThumbnailsTable(sql::Database* db) {
  // TODO: The next schema change that modifies existing columns on this table
  // should also add "DEFAULT x''" to the definition of the "thumbnail" column.
  static const char kSql[] =
      "CREATE TABLE IF NOT EXISTS page_thumbnails"
      " (offline_id INTEGER PRIMARY KEY NOT NULL,"
      " expiration INTEGER NOT NULL,"
      " thumbnail BLOB NOT NULL,"
      " favicon BLOB NOT NULL DEFAULT x''"
      ")";
  return db->Execute(kSql);
}

bool CreateLatestSchema(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // First time database initialization.
  if (!CreateOfflinePagesTable(db))
    return false;
  if (!CreatePageThumbnailsTable(db))
    return false;

  sql::MetaTable meta_table;
  if (!meta_table.Init(db, OfflinePageMetadataStore::kCurrentVersion,
                       OfflinePageMetadataStore::kCompatibleVersion))
    return false;

  return transaction.Commit();
}

// Upgrades the database from before the database version was stored in the
// MetaTable. This function should never need to be modified.
bool UpgradeFromLegacyVersion(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Legacy upgrade section. Details are described in the header file.
  if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "expiration_time") &&
      !db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "title")) {
    if (!UpgradeFrom52(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "title")) {
    if (!UpgradeFrom53(db))
      return false;
  } else if (db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "offline_url")) {
    if (!UpgradeFrom54(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "original_url")) {
    if (!UpgradeFrom55(db))
      return false;
  } else if (db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "expiration_time")) {
    if (!UpgradeFrom56(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "request_origin")) {
    if (!UpgradeFrom57(db))
      return false;
  } else if (!db->DoesColumnExist(OFFLINE_PAGES_TABLE_NAME, "digest")) {
    if (!UpgradeFrom61(db))
      return false;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(db, OfflinePageMetadataStore::kFirstPostLegacyVersion,
                       OfflinePageMetadataStore::kCompatibleVersion))
    return false;

  return transaction.Commit();
}

bool UpgradeFromVersion1ToVersion2(sql::Database* db,
                                   sql::MetaTable* meta_table) {
  meta_table->SetVersionNumber(2);
  // No actual changes necessary, because upgrade_attempt was deprecated.
  return true;
}

bool UpgradeFromVersion2ToVersion3(sql::Database* db,
                                   sql::MetaTable* meta_table) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static const char kCreatePageThumbnailsSql[] =
      "CREATE TABLE IF NOT EXISTS page_thumbnails"
      " (offline_id INTEGER PRIMARY KEY NOT NULL,"
      "expiration INTEGER NOT NULL,"
      "thumbnail BLOB NOT NULL"
      ")";
  if (!db->Execute(kCreatePageThumbnailsSql))
    return false;

  meta_table->SetVersionNumber(3);
  return transaction.Commit();
}

bool UpgradeFromVersion3ToVersion4(sql::Database* db,
                                   sql::MetaTable* meta_table) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  const char kSql[] = "ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                      " ADD COLUMN snippet VARCHAR NOT NULL DEFAULT ''; "
                      "ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                      " ADD COLUMN attribution VARCHAR NOT NULL DEFAULT '';";
  if (!db->Execute(kSql))
    return false;

  const char kUpgradeThumbnailsTableSql[] =
      "ALTER TABLE page_thumbnails"
      " ADD COLUMN favicon BLOB NOT NULL DEFAULT x''";
  if (!db->Execute(kUpgradeThumbnailsTableSql))
    return false;

  meta_table->SetVersionNumber(4);
  return transaction.Commit();
}

bool CreateSchema(sql::Database* db) {
  if (!sql::MetaTable::DoesTableExist(db)) {
    // If this looks like a completely empty DB, simply start from scratch.
    if (!db->DoesTableExist(OFFLINE_PAGES_TABLE_NAME))
      return CreateLatestSchema(db);

    // Otherwise we need to run a legacy upgrade.
    if (!UpgradeFromLegacyVersion(db))
      return false;
  }

  sql::MetaTable meta_table;
  if (!meta_table.Init(db, OfflinePageMetadataStore::kCurrentVersion,
                       OfflinePageMetadataStore::kCompatibleVersion))
    return false;

  for (;;) {
    switch (meta_table.GetVersionNumber()) {
      case 1:
        if (!UpgradeFromVersion1ToVersion2(db, &meta_table))
          return false;
        break;
      case 2:
        if (!UpgradeFromVersion2ToVersion3(db, &meta_table))
          return false;
        break;
      case 3:
        if (!UpgradeFromVersion3ToVersion4(db, &meta_table))
          return false;
        break;
      case OfflinePageMetadataStore::kCurrentVersion:
        return true;
      default:
        return false;
    }
  }
}

StoreState InitializationStatusToStoreState(
    SqlStoreBase::InitializationStatus status) {
  switch (status) {
    case SqlStoreBase::InitializationStatus::kNotInitialized:
      return StoreState::NOT_LOADED;
    case SqlStoreBase::InitializationStatus::kInProgress:
      return StoreState::INITIALIZING;
    case SqlStoreBase::InitializationStatus::kSuccess:
      return StoreState::LOADED;
    case SqlStoreBase::InitializationStatus::kFailure:
      return StoreState::FAILED_LOADING;
  }
}

}  // anonymous namespace

OfflinePageMetadataStore::OfflinePageMetadataStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : SqlStoreBase("OfflinePageMetadata",
                   std::move(background_task_runner),
                   base::FilePath()) {}

OfflinePageMetadataStore::OfflinePageMetadataStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : SqlStoreBase("OfflinePageMetadata",
                   std::move(background_task_runner),
                   path.AppendASCII("OfflinePages.db")) {}

OfflinePageMetadataStore::~OfflinePageMetadataStore() = default;

base::OnceCallback<bool(sql::Database* db)>
OfflinePageMetadataStore::GetSchemaInitializationFunction() {
  return base::BindOnce(&CreateSchema);
}

StoreState OfflinePageMetadataStore::GetStateForTesting() const {
  return InitializationStatusToStoreState(initialization_status_for_testing());
}

void OfflinePageMetadataStore::OnOpenStart(base::TimeTicks last_closing_time) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Metadata Store", this, "is reopen",
                           !last_closing_time.is_null());
  ReportStoreEvent(last_closing_time.is_null()
                       ? OfflinePagesStoreEvent::kOpenedFirstTime
                       : OfflinePagesStoreEvent::kReopened);
}

void OfflinePageMetadataStore::OnOpenDone(bool success) {
  TRACE_EVENT_ASYNC_STEP_PAST1("offline_pages", "Metadata Store", this,
                               "Initializing", "succeeded", success);
  if (!success) {
    TRACE_EVENT_ASYNC_END0("offline_pages", "Metadata Store", this);
  }
}

void OfflinePageMetadataStore::OnTaskBegin(bool is_initialized) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Metadata Store: task execution",
                           this, "is store loaded", is_initialized);
}

void OfflinePageMetadataStore::OnTaskRunComplete() {
  // Note: the time recorded for this trace step will include thread hop wait
  // times to the background thread and back.
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages",
                               "Metadata Store: task execution", this, "Task");
}

void OfflinePageMetadataStore::OnTaskReturnComplete() {
  TRACE_EVENT_ASYNC_STEP_PAST0(
      "offline_pages", "Metadata Store: task execution", this, "Callback");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Metadata Store: task execution",
                         this);
}

void OfflinePageMetadataStore::OnCloseStart(
    InitializationStatus status_before_close) {
  if (status_before_close != InitializationStatus::kSuccess) {
    ReportStoreEvent(OfflinePagesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Metadata Store", this, "Open");

  ReportStoreEvent(OfflinePagesStoreEvent::kClosed);
}

void OfflinePageMetadataStore::OnCloseComplete() {
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Metadata Store", this,
                               "Closing");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Metadata Store", this);
}

}  // namespace offline_pages
