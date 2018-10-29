// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/clear_storage_task.h"

#include <algorithm>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/core/client_policy_controller.h"
#include "components/offline_pages/core/offline_page_client_policy.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

using LifetimeType = LifetimePolicy::LifetimeType;
using ClearStorageResult = ClearStorageTask::ClearStorageResult;

namespace {

#define PAGE_INFO_PROJECTION                                            \
  " offline_id, client_namespace, client_id, file_size, creation_time," \
  " last_access_time, file_path "

// This struct needs to be in sync with |PAGE_INFO_PROJECTION|.
struct PageInfo {
  PageInfo();
  PageInfo(const PageInfo& other);
  int64_t offline_id;
  ClientId client_id;
  int64_t file_size;
  base::Time creation_time;
  base::Time last_access_time;
  base::FilePath file_path;
};

PageInfo::PageInfo() = default;
PageInfo::PageInfo(const PageInfo& other) = default;

// Maximum % of total available storage that will be occupied by offline pages
// before a storage clearup.
const double kOfflinePageStorageLimit = 0.3;
// The target % of storage usage we try to reach below when expiring pages.
const double kOfflinePageStorageClearThreshold = 0.1;

// Make sure this is in sync with |PAGE_INFO_PROJECTION| above.
PageInfo MakePageInfo(sql::Statement* statement) {
  PageInfo page_info;
  page_info.offline_id = statement->ColumnInt64(0);
  page_info.client_id =
      ClientId(statement->ColumnString(1), statement->ColumnString(2));
  page_info.file_size = statement->ColumnInt64(3);
  page_info.creation_time =
      store_utils::FromDatabaseTime(statement->ColumnInt64(4));
  page_info.last_access_time =
      store_utils::FromDatabaseTime(statement->ColumnInt64(5));
  page_info.file_path =
      store_utils::FromDatabaseFilePath(statement->ColumnString(6));
  return page_info;
}

std::unique_ptr<std::vector<PageInfo>> GetAllTemporaryPageInfos(
    const std::map<std::string, LifetimePolicy>& temp_namespace_policy_map,
    sql::Database* db) {
  auto result = std::make_unique<std::vector<PageInfo>>();

  static const char kSql[] = "SELECT " PAGE_INFO_PROJECTION
                             " FROM offlinepages_v1"
                             " WHERE client_namespace = ?";

  for (const auto& temp_namespace_policy : temp_namespace_policy_map) {
    std::string name_space = temp_namespace_policy.first;
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindString(0, name_space);
    while (statement.Step())
      result->emplace_back(MakePageInfo(&statement));
    if (!statement.Succeeded()) {
      result->clear();
      break;
    }
  }

  return result;
}

std::unique_ptr<std::vector<PageInfo>> GetPageInfosToClear(
    const std::map<std::string, LifetimePolicy>& temp_namespace_policy_map,
    const base::Time& start_time,
    const ArchiveManager::StorageStats& stats,
    sql::Database* db) {
  std::map<std::string, int> namespace_page_count;
  auto page_infos_to_delete = std::make_unique<std::vector<PageInfo>>();
  std::vector<PageInfo> pages_remaining;
  int64_t remaining_size = 0;

  // If the cached pages exceed the storage limit, we need to clear more than
  // just expired pages to make the storage usage below the threshold.
  bool quota_based_clearing =
      stats.temporary_archives_size >=
      (stats.temporary_archives_size + stats.internal_free_disk_space) *
          kOfflinePageStorageLimit;
  int64_t max_allowed_size =
      (stats.temporary_archives_size + stats.internal_free_disk_space) *
      kOfflinePageStorageClearThreshold;

  // Initialize the counting map with 0s.
  for (const auto& namespace_policy : temp_namespace_policy_map)
    namespace_page_count[namespace_policy.first] = 0;

  // Gets all temporary pages and sort by last accessed time.
  auto page_infos = GetAllTemporaryPageInfos(temp_namespace_policy_map, db);
  std::sort(page_infos->begin(), page_infos->end(),
            [](const PageInfo& a, const PageInfo& b) -> bool {
              return a.last_access_time > b.last_access_time;
            });

  for (const auto& page_info : *page_infos) {
    std::string name_space = page_info.client_id.name_space;
    LifetimePolicy policy = temp_namespace_policy_map.at(name_space);
    // If the page is expired, put it in the list to delete later.
    if (start_time - page_info.last_access_time >= policy.expiration_period) {
      page_infos_to_delete->push_back(page_info);
      continue;
    }

    // If the namespace of the page already has more pages than limit, this page
    // needs to be deleted.
    int page_limit = policy.page_limit;
    if (page_limit != kUnlimitedPages &&
        namespace_page_count[name_space] >= page_limit) {
      page_infos_to_delete->push_back(page_info);
      continue;
    }

    // Pages with no file can be removed.
    if (!base::PathExists(page_info.file_path)) {
      page_infos_to_delete->push_back(page_info);
      continue;
    }

    // If there's no quota, remove the pages.
    if (quota_based_clearing &&
        remaining_size + page_info.file_size > max_allowed_size) {
      page_infos_to_delete->push_back(page_info);
      continue;
    }

    // Otherwise the page needs to be kept, in case the storage usage is still
    // higher than the threshold, and we need to clear more pages.
    pages_remaining.push_back(page_info);
    remaining_size += page_info.file_size;
    namespace_page_count[name_space]++;
  }
  return page_infos_to_delete;
}

bool DeleteArchiveSync(const base::FilePath& file_path) {
  // Delete the file only, |false| for recursive.
  return base::DeleteFile(file_path, false);
}

// Deletes a page from the store by |offline_id|.
bool DeletePageEntryByOfflineIdSync(sql::Database* db, int64_t offline_id) {
  static const char kSql[] = "DELETE FROM offlinepages_v1 WHERE offline_id = ?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, offline_id);
  return statement.Run();
}

std::pair<size_t, DeletePageResult> ClearPagesSync(
    std::map<std::string, LifetimePolicy> temp_namespace_policy_map,
    const base::Time& start_time,
    const ArchiveManager::StorageStats& stats,
    sql::Database* db) {
  std::unique_ptr<std::vector<PageInfo>> page_infos =
      GetPageInfosToClear(temp_namespace_policy_map, start_time, stats, db);

  size_t pages_cleared = 0;
  for (const auto& page_info : *page_infos) {
    if (!base::PathExists(page_info.file_path) ||
        DeleteArchiveSync(page_info.file_path)) {
      if (DeletePageEntryByOfflineIdSync(db, page_info.offline_id)) {
        pages_cleared++;
        // Reports the time since creation in minutes.
        base::TimeDelta time_since_creation =
            start_time - page_info.creation_time;
        UMA_HISTOGRAM_CUSTOM_COUNTS(
            "OfflinePages.ClearTemporaryPages.TimeSinceCreation",
            time_since_creation.InMinutes(), 1,
            base::TimeDelta::FromDays(30).InMinutes(), 50);
      }
    }
  }

  return std::make_pair(pages_cleared, pages_cleared == page_infos->size()
                                           ? DeletePageResult::SUCCESS
                                           : DeletePageResult::STORE_FAILURE);
}

std::map<std::string, LifetimePolicy> GetTempNamespacePolicyMap(
    ClientPolicyController* policy_controller) {
  std::map<std::string, LifetimePolicy> result;
  for (const auto& name_space :
       policy_controller->GetNamespacesRemovedOnCacheReset()) {
    result.emplace(name_space,
                   policy_controller->GetPolicy(name_space).lifetime_policy);
  }
  return result;
}

}  // namespace

ClearStorageTask::ClearStorageTask(OfflinePageMetadataStore* store,
                                   ArchiveManager* archive_manager,
                                   ClientPolicyController* policy_controller,
                                   const base::Time& clearup_time,
                                   ClearStorageCallback callback)
    : store_(store),
      archive_manager_(archive_manager),
      policy_controller_(policy_controller),
      callback_(std::move(callback)),
      clearup_time_(clearup_time),
      weak_ptr_factory_(this) {
  DCHECK(store_);
  DCHECK(archive_manager_);
  DCHECK(policy_controller_);
  DCHECK(!callback_.is_null());
}

ClearStorageTask::~ClearStorageTask() {}

void ClearStorageTask::Run() {
  TRACE_EVENT_ASYNC_BEGIN0("offline_pages", "ClearStorageTask running", this);
  archive_manager_->GetStorageStats(
      base::BindOnce(&ClearStorageTask::OnGetStorageStatsDone,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ClearStorageTask::OnGetStorageStatsDone(
    const ArchiveManager::StorageStats& stats) {
  std::map<std::string, LifetimePolicy> temp_namespace_policy_map =
      GetTempNamespacePolicyMap(policy_controller_);
  store_->Execute(base::BindOnce(&ClearPagesSync, temp_namespace_policy_map,
                                 clearup_time_, stats),
                  base::BindOnce(&ClearStorageTask::OnClearPagesDone,
                                 weak_ptr_factory_.GetWeakPtr()),
                  {0, DeletePageResult::STORE_FAILURE});
}

void ClearStorageTask::OnClearPagesDone(
    std::pair<size_t, DeletePageResult> result) {
  if (result.first == 0 && result.second == DeletePageResult::SUCCESS) {
    InformClearStorageDone(result.first, ClearStorageResult::UNNECESSARY);
    return;
  }

  ClearStorageResult clear_result = ClearStorageResult::SUCCESS;
  if (result.second != DeletePageResult::SUCCESS)
    clear_result = ClearStorageResult::DELETE_FAILURE;
  InformClearStorageDone(result.first, clear_result);
}

void ClearStorageTask::InformClearStorageDone(size_t pages_cleared,
                                              ClearStorageResult result) {
  std::move(callback_).Run(pages_cleared, result);
  TaskComplete();
  TRACE_EVENT_ASYNC_END2("offline_pages", "ClearStorageTask running", this,
                         "result", static_cast<int>(result), "pages_cleared",
                         pages_cleared);
}

}  // namespace offline_pages
