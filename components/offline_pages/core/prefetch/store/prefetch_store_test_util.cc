// Copyright 2017 The Chromium Authors. All rights reserved.
// // Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/store/prefetch_store_test_util.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind_test_util.h"
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
base::Optional<PrefetchItem> ReadPrefetchItem(const sql::Statement& statement) {
  PrefetchItem item;
  DCHECK_EQ(23, statement.ColumnCount());

  // Fields are assigned to the item in the order they are stored in the SQL
  // store (integer fields first).
  item.offline_id = statement.ColumnInt64(0);
  base::Optional<PrefetchItemState> state =
      ToPrefetchItemState(statement.ColumnInt(1));
  if (!state)
    return base::nullopt;
  item.state = state.value();
  item.generate_bundle_attempts = statement.ColumnInt(2);
  item.get_operation_attempts = statement.ColumnInt(3);
  item.download_initiation_attempts = statement.ColumnInt(4);
  item.archive_body_length = statement.ColumnInt64(5);
  item.creation_time = store_utils::FromDatabaseTime(statement.ColumnInt64(6));
  item.freshness_time = store_utils::FromDatabaseTime(statement.ColumnInt64(7));
  base::Optional<PrefetchItemErrorCode> error_code =
      ToPrefetchItemErrorCode(statement.ColumnInt(8));
  if (!error_code)
    return base::nullopt;
  item.error_code = error_code.value();
  item.guid = statement.ColumnString(9);
  item.client_id.name_space = statement.ColumnString(10);
  item.client_id.id = statement.ColumnString(11);
  item.url = GURL(statement.ColumnString(12));
  item.final_archived_url = GURL(statement.ColumnString(13));
  item.operation_name = statement.ColumnString(14);
  item.archive_body_name = statement.ColumnString(15);
  item.title = statement.ColumnString16(16);
  item.file_path =
      store_utils::FromDatabaseFilePath(statement.ColumnString(17));
  item.file_size = statement.ColumnInt64(18);
  item.thumbnail_url = GURL(statement.ColumnString(19));
  item.favicon_url = GURL(statement.ColumnString(20));
  item.snippet = statement.ColumnString(21);
  item.attribution = statement.ColumnString(22);
  return item;
}

base::Optional<PrefetchItem> GetPrefetchItemSync(int64_t offline_id,
                                                 sql::Database* db) {
  static const std::string kSql = base::StringPrintf(
      "SELECT %s FROM prefetch_items WHERE offline_id = ?", kSqlAllColumnNames);

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql.c_str()));
  statement.BindInt64(0, offline_id);

  if (!statement.Step())
    return base::nullopt;

  return ReadPrefetchItem(statement);
}

std::set<PrefetchItem> GetAllItemsSync(sql::Database* db) {
  // Not starting transaction as this is a single read.
  std::set<PrefetchItem> items;
  static const std::string kSql =
      base::StringPrintf("SELECT %s FROM prefetch_items", kSqlAllColumnNames);
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql.c_str()));
  while (statement.Step()) {
    base::Optional<PrefetchItem> item = ReadPrefetchItem(statement);
    if (item)
      items.insert(std::move(item).value());
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

PrefetchStoreTestUtil::PrefetchStoreTestUtil() = default;

PrefetchStoreTestUtil::~PrefetchStoreTestUtil() = default;

void PrefetchStoreTestUtil::BuildStore() {
  if (!temp_directory_.CreateUniqueTempDir())
    DVLOG(1) << "temp_directory_ not created";

  owned_store_.reset(new PrefetchStore(base::ThreadTaskRunnerHandle::Get(),
                                       temp_directory_.GetPath()));
  store_ = owned_store_.get();
}

void PrefetchStoreTestUtil::BuildStoreInMemory() {
  owned_store_.reset(new PrefetchStore(base::ThreadTaskRunnerHandle::Get()));
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
  // The actual deletion happens in a task. So wait until all have been
  // processed.
  base::RunLoop().RunUntilIdle();
}

bool PrefetchStoreTestUtil::InsertPrefetchItem(const PrefetchItem& item) {
  base::RunLoop run_loop;
  bool success = false;
  store_->Execute(base::BindOnce(&InsertPrefetchItemSync, item),
                  base::BindOnce(base::BindLambdaForTesting([&](bool s) {
                    success = s;
                    run_loop.Quit();
                  })),
                  false);
  run_loop.Run();
  return success;
}

int PrefetchStoreTestUtil::CountPrefetchItems() {
  base::RunLoop run_loop;
  int count = 0;
  store_->Execute(base::BindOnce(&CountPrefetchItemsSync),
                  base::BindOnce(base::BindLambdaForTesting([&](int result) {
                    count = result;
                    run_loop.Quit();
                  })),
                  kPrefetchStoreCommandFailed);
  run_loop.Run();
  return count;
}

std::unique_ptr<PrefetchItem> PrefetchStoreTestUtil::GetPrefetchItem(
    int64_t offline_id) {
  base::RunLoop run_loop;
  std::unique_ptr<PrefetchItem> item;
  store_->Execute(
      base::BindOnce(&GetPrefetchItemSync, offline_id),
      base::BindOnce(
          base::BindLambdaForTesting([&](base::Optional<PrefetchItem> result) {
            if (result) {
              item = std::make_unique<PrefetchItem>(std::move(result).value());
            }
            run_loop.Quit();
          })),
      base::Optional<PrefetchItem>());
  run_loop.Run();
  return item;
}

std::size_t PrefetchStoreTestUtil::GetAllItems(
    std::set<PrefetchItem>* all_items) {
  DCHECK(all_items->empty());
  *all_items = GetAllItems();
  return all_items->size();
}

std::set<PrefetchItem> PrefetchStoreTestUtil::GetAllItems() {
  base::RunLoop run_loop;
  std::set<PrefetchItem> items;
  store_->Execute(base::BindOnce(&GetAllItemsSync),
                  base::BindOnce(base::BindLambdaForTesting(
                      [&](std::set<PrefetchItem> result) {
                        items = std::move(result);
                        run_loop.Quit();
                      })),
                  std::set<PrefetchItem>());
  run_loop.Run();
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
  base::RunLoop run_loop;
  int count = -1;
  store_->Execute(base::BindOnce(&UpdateItemsStateSync, name_space, url.spec(),
                                 PrefetchItemState::ZOMBIE),
                  base::BindOnce(base::BindLambdaForTesting([&](int result) {
                    count = result;
                    run_loop.Quit();
                  })),
                  kPrefetchStoreCommandFailed);
  run_loop.Run();
  return count;
}

int PrefetchStoreTestUtil::LastCommandChangeCount() {
  base::RunLoop run_loop;
  int count = 0;
  store_->Execute(base::BindOnce([](sql::Database* connection) {
                    return connection->GetLastChangeCount();
                  }),
                  base::BindOnce(base::BindLambdaForTesting([&](int result) {
                    count = result;
                    run_loop.Quit();
                  })),
                  0);
  run_loop.Run();
  return count;
}

int64_t PrefetchStoreTestUtil::GetPrefetchQuota() {
  base::RunLoop run_loop;
  int64_t result;
  store_->Execute(base::BindOnce(&GetPrefetchQuotaSync, clock()),
                  base::BindOnce(base::BindLambdaForTesting([&](int64_t quota) {
                    result = quota;
                    run_loop.Quit();
                  })),
                  int64_t());
  run_loop.Run();
  return result;
}

bool PrefetchStoreTestUtil::SetPrefetchQuota(int64_t available_quota) {
  base::RunLoop run_loop;
  bool result;
  store_->Execute(
      base::BindOnce(&SetPrefetchQuotaSync, available_quota, clock()),
      base::BindOnce(base::BindLambdaForTesting([&](bool success) {
        result = success;
        run_loop.Quit();
      })),
      false);
  run_loop.Run();
  return result;
}

void PrefetchStoreTestUtil::SimulateInitializationError() {
  store_->SetInitializationStatusForTesting(
      SqlStoreBase::InitializationStatus::kFailure, false);
}

}  // namespace offline_pages
