// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/offline_page_metadata_store.h"

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
  static const char kSql[] =
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
  return db->Execute(kSql);
}

bool UpgradeWithQuery(sql::Database* db, const char* upgrade_sql) {
  if (!db->Execute("ALTER TABLE " OFFLINE_PAGES_TABLE_NAME
                   " RENAME TO temp_" OFFLINE_PAGES_TABLE_NAME)) {
    return false;
  }
  if (!CreateOfflinePagesTable(db))
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
  static const char kSql[] =
      "CREATE TABLE IF NOT EXISTS page_thumbnails"
      " (offline_id INTEGER PRIMARY KEY NOT NULL,"
      " expiration INTEGER NOT NULL,"
      " thumbnail BLOB NOT NULL"
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
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  static const char kSql[] = "UPDATE " OFFLINE_PAGES_TABLE_NAME
                             " SET upgrade_attempt = 5 "
                             " WHERE client_namespace = 'async_loading'"
                             " OR client_namespace = 'download'"
                             " OR client_namespace = 'ntp_suggestions'"
                             " OR client_namespace = 'browser_actions'";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (!statement.Run())
    return false;

  meta_table->SetVersionNumber(2);
  return transaction.Commit();
}

bool UpgradeFromVersion2ToVersion3(sql::Database* db,
                                   sql::MetaTable* meta_table) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  if (!CreatePageThumbnailsTable(db)) {
    return false;
  }
  meta_table->SetVersionNumber(3);
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
      case OfflinePageMetadataStore::kCurrentVersion:
        return true;
      default:
        return false;
    }
  }
}

bool PrepareDirectory(const base::FilePath& path) {
  base::File::Error error = base::File::FILE_OK;
  if (!base::DirectoryExists(path.DirName())) {
    if (!base::CreateDirectoryAndGetError(path.DirName(), &error)) {
      LOG(ERROR) << "Failed to create offline pages db directory: "
                 << base::File::ErrorToString(error);
      return false;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("OfflinePages.SQLStorage.CreateDirectoryResult",
                            -error, -base::File::FILE_ERROR_MAX);

  return true;
}

bool InitDatabase(sql::Database* db,
                  const base::FilePath& path,
                  bool in_memory) {
  db->set_page_size(4096);
  db->set_cache_size(500);
  db->set_histogram_tag("OfflinePageMetadata");
  db->set_exclusive_locking();

  if (!in_memory && !PrepareDirectory(path))
    return false;

  bool open_db_result = false;
  if (in_memory)
    open_db_result = db->OpenInMemory();
  else
    open_db_result = db->Open(path);

  if (!open_db_result) {
    LOG(ERROR) << "Failed to open database, in memory: " << in_memory;
    return false;
  }
  db->Preload();

  return CreateSchema(db);
}

void CloseDatabaseSync(
    sql::Database* db,
    scoped_refptr<base::SingleThreadTaskRunner> callback_runner,
    base::OnceClosure callback) {
  if (db)
    db->Close();
  callback_runner->PostTask(FROM_HERE, std::move(callback));
}

}  // anonymous namespace

// static
constexpr base::TimeDelta OfflinePageMetadataStore::kClosingDelay;

OfflinePageMetadataStore::OfflinePageMetadataStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : background_task_runner_(std::move(background_task_runner)),
      in_memory_(true),
      state_(StoreState::NOT_LOADED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

OfflinePageMetadataStore::OfflinePageMetadataStore(
    scoped_refptr<base::SequencedTaskRunner> background_task_runner,
    const base::FilePath& path)
    : background_task_runner_(std::move(background_task_runner)),
      in_memory_(false),
      db_file_path_(path.AppendASCII("OfflinePages.db")),
      state_(StoreState::NOT_LOADED),
      weak_ptr_factory_(this),
      closing_weak_ptr_factory_(this) {}

OfflinePageMetadataStore::~OfflinePageMetadataStore() {
  if (db_.get() &&
      !background_task_runner_->DeleteSoon(FROM_HERE, db_.release())) {
    DLOG(WARNING) << "SQL database will not be deleted.";
  }
}

StoreState OfflinePageMetadataStore::GetStateForTesting() const {
  return state_;
}

void OfflinePageMetadataStore::SetStateForTesting(StoreState state,
                                                  bool reset_db) {
  state_ = state;
  if (reset_db)
    db_.reset(nullptr);
}

void OfflinePageMetadataStore::InitializeInternal(
    base::OnceClosure pending_command) {
  TRACE_EVENT_ASYNC_BEGIN1("offline_pages", "Metadata Store", this, "is reopen",
                           !last_closing_time_.is_null());
  DCHECK_EQ(state_, StoreState::NOT_LOADED);

  if (!last_closing_time_.is_null()) {
    ReportStoreEvent(OfflinePagesStoreEvent::kReopened);
    UMA_HISTOGRAM_CUSTOM_TIMES("OfflinePages.SQLStorage.TimeFromCloseToOpen",
                               base::Time::Now() - last_closing_time_,
                               base::TimeDelta::FromMilliseconds(10),
                               base::TimeDelta::FromMinutes(10),
                               50 /* buckets */);
  } else {
    ReportStoreEvent(OfflinePagesStoreEvent::kOpenedFirstTime);
  }

  state_ = StoreState::INITIALIZING;
  db_.reset(new sql::Database());
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&InitDatabase, db_.get(), db_file_path_, in_memory_),
      base::BindOnce(&OfflinePageMetadataStore::OnInitializeInternalDone,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(pending_command)));
}

void OfflinePageMetadataStore::OnInitializeInternalDone(
    base::OnceClosure pending_command,
    bool success) {
  TRACE_EVENT_ASYNC_STEP_PAST1("offline_pages", "Metadata Store", this,
                               "Initializing", "succeeded", success);
  // TODO(fgorski): DCHECK initialization is in progress, once we have a
  // relevant value for the store state.
  if (success) {
    state_ = StoreState::LOADED;
  } else {
    state_ = StoreState::FAILED_LOADING;
    db_.reset();
    TRACE_EVENT_ASYNC_END0("offline_pages", "Metadata Store", this);
  }

  CHECK(!pending_command.is_null());
  std::move(pending_command).Run();

  // Execute other pending commands.
  for (auto command_iter = pending_commands_.begin();
       command_iter != pending_commands_.end();) {
    std::move(*command_iter++).Run();
  }

  pending_commands_.clear();

  if (state_ == StoreState::FAILED_LOADING)
    state_ = StoreState::NOT_LOADED;
}

void OfflinePageMetadataStore::CloseInternal() {
  if (state_ != StoreState::LOADED) {
    ReportStoreEvent(OfflinePagesStoreEvent::kCloseSkipped);
    return;
  }
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Metadata Store", this, "Open");

  last_closing_time_ = base::Time::Now();
  ReportStoreEvent(OfflinePagesStoreEvent::kClosed);

  state_ = StoreState::NOT_LOADED;
  background_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &CloseDatabaseSync, db_.get(), base::ThreadTaskRunnerHandle::Get(),
          base::BindOnce(&OfflinePageMetadataStore::CloseInternalDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(db_))));
}

void OfflinePageMetadataStore::CloseInternalDone(
    std::unique_ptr<sql::Database> db) {
  db.reset();
  TRACE_EVENT_ASYNC_STEP_PAST0("offline_pages", "Metadata Store", this,
                               "Closing");
  TRACE_EVENT_ASYNC_END0("offline_pages", "Metadata Store", this);
}

}  // namespace offline_pages
