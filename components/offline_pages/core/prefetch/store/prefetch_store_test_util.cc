// Copyright 2017 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_item.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_downloader_quota.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "url/gurl.h"

namespace offline_pages {
const int kPrefetchStoreCommandFailed = -1;

namespace {

// Comma separated list of all columns in the table, by convention following
// their natural order.
const char* kSqlAllColumnNames =
    "offline_id, "
    "state, "
    "generate_bundle_attempts, "
    "get_operation_attempts, "
    "download_initiation_attempts, "
    "archive_body_length, "
    "creation_time, "
    "freshness_time, "
    "error_code, "
    "guid, "
    "client_namespace, "
    "client_id, "
    "requested_url, "
    "final_archived_url, "
    "operation_name, "
    "archive_body_name, "
    "title, "
    "file_path, "
    "file_size, "
    "thumbnail_url, "
    "favicon_url, "
    "snippet, "
    "attribution";

bool InsertPrefetchItemSync(const PrefetchItem& item, sql::Database* db) {
  static const std::string kSql = base::StringPrintf(
      "INSERT INTO prefetch_items (%s)"
      " VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?,"
      " ?, ?, ?)",
      kSqlAllColumnNames);
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql.c_str()));
  statement.BindInt64(0, item.offline_id);
  statement.BindInt(1, static_cast<int>(item.state));
  statement.BindInt(2, item.generate_bundle_attempts);
  statement.BindInt(3, item.get_operation_attempts);
  statement.BindInt(4, item.download_initiation_attempts);
  statement.BindInt64(5, item.archive_body_length);
  statement.BindInt64(6, store_utils::ToDatabaseTime(item.creation_time));
  statement.BindInt64(7, store_utils::ToDatabaseTime(item.freshness_time));
  statement.BindInt(8, static_cast<int>(item.error_code));
  statement.BindString(9, item.guid);
  statement.BindString(10, item.client_id.name_space);
  statement.BindString(11, item.client_id.id);
  statement.BindString(12, item.url.spec());
  statement.BindString(13, item.final_archived_url.spec());
  statement.BindString(14, item.operation_name);
  statement.BindString(15, item.archive_body_name);
  statement.BindString16(16, item.title);
  statement.BindString(17, store_utils::ToDatabaseFilePath(item.file_path));
  statement.BindInt64(18, item.file_size);
  statement.BindString(19, item.thumbnail_url.spec());
  statement.BindString(20, item.favicon_url.spec());
  statement.BindString(21, item.snippet);
  statement.BindString(22, item.attribution);

  return statement.Run();
}

int CountPrefetchItemsSync(sql::Database* db) {
  // Not starting transaction as this is a single read.
  static const char kSql[] = "SELECT COUNT(offline_id) FROM prefetch_items";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  if (statement.Step())
    return statement.ColumnInt(0);

  return kPrefetchStoreCommandFailed;
}

// Populates the PrefetchItem with the data from the current row of the passed
// in statement following the natural column ordering.
void PopulatePrefetchItem(const sql::Statement& statement, PrefetchItem* item) {
  DCHECK_EQ(23, statement.ColumnCount());
  DCHECK(item);

  // Fields are assigned to the item in the order they are stored in the SQL
  // store (integer fields first).
  item->offline_id = statement.ColumnInt64(0);
  item->state = static_cast<PrefetchItemState>(statement.ColumnInt(1));
  item->generate_bundle_attempts = statement.ColumnInt(2);
  item->get_operation_attempts = statement.ColumnInt(3);
  item->download_initiation_attempts = statement.ColumnInt(4);
  item->archive_body_length = statement.ColumnInt64(5);
  item->creation_time = store_utils::FromDatabaseTime(statement.ColumnInt64(6));
  item->freshness_time =
      store_utils::FromDatabaseTime(statement.ColumnInt64(7));
  item->error_code = static_cast<PrefetchItemErrorCode>(statement.ColumnInt(8));
  item->guid = statement.ColumnString(9);
  item->client_id.name_space = statement.ColumnString(10);
  item->client_id.id = statement.ColumnString(11);
  item->url = GURL(statement.ColumnString(12));
  item->final_archived_url = GURL(statement.ColumnString(13));
  item->operation_name = statement.ColumnString(14);
  item->archive_body_name = statement.ColumnString(15);
  item->title = statement.ColumnString16(16);
  item->file_path =
      store_utils::FromDatabaseFilePath(statement.ColumnString(17));
  item->file_size = statement.ColumnInt64(18);
  item->thumbnail_url = GURL(statement.ColumnString(19));
  item->favicon_url = GURL(statement.ColumnString(20));
  item->snippet = statement.ColumnString(21);
  item->attribution = statement.ColumnString(22);
}

std::unique_ptr<PrefetchItem> GetPrefetchItemSync(int64_t offline_id,
                                                  sql::Database* db) {
  static const std::string kSql = base::StringPrintf(
      "SELECT %s FROM prefetch_items WHERE offline_id = ?", kSqlAllColumnNames);

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql.c_str()));
  statement.BindInt64(0, offline_id);

  if (!statement.Step())
    return nullptr;

  auto item = std::make_unique<PrefetchItem>();
  PopulatePrefetchItem(statement, item.get());
  return item;
}

std::set<PrefetchItem> GetAllItemsSync(sql::Database* db) {
  // Not starting transaction as this is a single read.
  std::set<PrefetchItem> items;
  static const std::string kSql =
      base::StringPrintf("SELECT %s FROM prefetch_items", kSqlAllColumnNames);
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql.c_str()));
  while (statement.Step()) {
    PrefetchItem loaded_item;
    PopulatePrefetchItem(statement, &loaded_item);
    items.insert(loaded_item);
  }
  return items;
}

int UpdateItemsStateSync(const std::string& name_space,
                         const std::string& url,
                         PrefetchItemState state,
                         sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state = ?"
      " WHERE client_namespace = ? AND requested_url = ?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(state));
  statement.BindString(1, name_space);
  statement.BindString(2, url);
  if (statement.Run())
    return db->GetLastChangeCount();

  return kPrefetchStoreCommandFailed;
}

int64_t GetPrefetchQuotaSync(base::Clock* clock, sql::Database* db) {
  PrefetchDownloaderQuota downloader_quota(db, clock);
  return downloader_quota.GetAvailableQuotaBytes();
}

bool SetPrefetchQuotaSync(int64_t available_quota,
                          base::Clock* clock,
                          sql::Database* db) {
  PrefetchDownloaderQuota downloader_quota(db, clock);
  return downloader_quota.SetAvailableQuotaBytes(available_quota);
}

}  // namespace

PrefetchStoreTestUtil::PrefetchStoreTestUtil(
    scoped_refptr<base::TestMockTimeTaskRunner> task_runner)
    : task_runner_(task_runner) {}

PrefetchStoreTestUtil::~PrefetchStoreTestUtil() = default;

void PrefetchStoreTestUtil::BuildStore() {
  if (!temp_directory_.CreateUniqueTempDir())
    DVLOG(1) << "temp_directory_ not created";

  owned_store_.reset(
      new PrefetchStore(task_runner_, temp_directory_.GetPath()));
  store_ = owned_store_.get();
}

void PrefetchStoreTestUtil::BuildStoreInMemory() {
  owned_store_.reset(new PrefetchStore(task_runner_));
  store_ = owned_store_.get();
}

std::unique_ptr<PrefetchStore> PrefetchStoreTestUtil::ReleaseStore() {
  return std::move(owned_store_);
}

void PrefetchStoreTestUtil::DeleteStore() {
  owned_store_.reset();
  if (temp_directory_.IsValid()) {
    if (!temp_directory_.Delete())
      DVLOG(1) << "temp_directory_ not created";
  }
  task_runner_->FastForwardUntilNoTasksRemain();
}

bool PrefetchStoreTestUtil::InsertPrefetchItem(const PrefetchItem& item) {
  bool success = false;
  store_->Execute(
      base::BindOnce(&InsertPrefetchItemSync, item),
      base::BindOnce([](bool* alias, bool s) { *alias = s; }, &success), false);
  RunUntilIdle();
  return success;
}

int PrefetchStoreTestUtil::CountPrefetchItems() {
  int count = 0;
  store_->Execute(
      base::BindOnce(&CountPrefetchItemsSync),
      base::BindOnce([](int* alias, int result) { *alias = result; }, &count),
      kPrefetchStoreCommandFailed);
  RunUntilIdle();
  return count;
}

std::unique_ptr<PrefetchItem> PrefetchStoreTestUtil::GetPrefetchItem(
    int64_t offline_id) {
  std::unique_ptr<PrefetchItem> item;
  store_->Execute(base::BindOnce(&GetPrefetchItemSync, offline_id),
                  base::BindOnce(
                      [](std::unique_ptr<PrefetchItem>* alias,
                         std::unique_ptr<PrefetchItem> result) {
                        *alias = std::move(result);
                      },
                      &item),
                  std::unique_ptr<PrefetchItem>());
  RunUntilIdle();
  return item;
}

std::size_t PrefetchStoreTestUtil::GetAllItems(
    std::set<PrefetchItem>* all_items) {
  DCHECK(all_items->empty());
  *all_items = GetAllItems();
  return all_items->size();
}

std::set<PrefetchItem> PrefetchStoreTestUtil::GetAllItems() {
  std::set<PrefetchItem> items;
  store_->Execute(
      base::BindOnce(&GetAllItemsSync),
      base::BindOnce(
          [](std::set<PrefetchItem>* alias, std::set<PrefetchItem> result) {
            *alias = std::move(result);
          },
          &items),
      std::set<PrefetchItem>());
  RunUntilIdle();
  return items;
}

std::string PrefetchStoreTestUtil::ToString() {
  std::string result = "PrefetchItems: [";
  std::set<PrefetchItem> items;
  GetAllItems(&items);
  for (const auto& item : items) {
    result += "\n";
    result += item.ToString();
  }
  result += "\n]";
  return result;
}

int PrefetchStoreTestUtil::ZombifyPrefetchItems(const std::string& name_space,
                                                const GURL& url) {
  int count = -1;
  store_->Execute(
      base::BindOnce(&UpdateItemsStateSync, name_space, url.spec(),
                     PrefetchItemState::ZOMBIE),
      base::BindOnce([](int* alias, int result) { *alias = result; }, &count),
      kPrefetchStoreCommandFailed);
  RunUntilIdle();
  return count;
}

void PrefetchStoreTestUtil::RunUntilIdle() {
  task_runner_->RunUntilIdle();
}

int PrefetchStoreTestUtil::LastCommandChangeCount() {
  int count = 0;
  store_->Execute(
      base::BindOnce([](sql::Database* connection) {
        return connection->GetLastChangeCount();
      }),
      base::BindOnce([](int* result, int count) { *result = count; }, &count),
      0);
  RunUntilIdle();
  return count;
}

int64_t PrefetchStoreTestUtil::GetPrefetchQuota() {
  int64_t result;
  store_->Execute(
      base::BindOnce(&GetPrefetchQuotaSync, clock()),
      base::BindOnce([](int64_t* result, int64_t quota) { *result = quota; },
                     &result),
      int64_t());
  RunUntilIdle();
  return result;
}

bool PrefetchStoreTestUtil::SetPrefetchQuota(int64_t available_quota) {
  bool result;
  store_->Execute(
      base::BindOnce(&SetPrefetchQuotaSync, available_quota, clock()),
      base::BindOnce([](bool* result, bool success) { *result = success; },
                     &result),
      false);
  RunUntilIdle();
  return result;
}

void PrefetchStoreTestUtil::SimulateInitializationError() {
  store_->initialization_status_ = InitializationStatus::FAILURE;
}

}  // namespace offline_pages
