// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/add_page_task.h"

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/task/task.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {

namespace {

// Converts an ItemActionStatus to AddPageResult.
AddPageResult ItemActionStatusToAddPageResult(ItemActionStatus status) {
  switch (status) {
    case ItemActionStatus::SUCCESS:
      return AddPageResult::SUCCESS;
    case ItemActionStatus::ALREADY_EXISTS:
      return AddPageResult::ALREADY_EXISTS;
    case ItemActionStatus::STORE_ERROR:
      return AddPageResult::STORE_FAILURE;
    case ItemActionStatus::NOT_FOUND:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return AddPageResult::STORE_FAILURE;
}

ItemActionStatus AddOfflinePageSync(const OfflinePageItem& item,
                                    sql::Database* db) {
  static const char kSql[] =
      "INSERT OR IGNORE INTO offlinepages_v1"
      " (offline_id,online_url,client_namespace,client_id,file_path,file_size,"
      "creation_time,last_access_time,access_count,title,original_url,"
      "request_origin,system_download_id,file_missing_time,digest,"
      "snippet,attribution)"
      " VALUES "
      "(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt64(0, item.offline_id);
  statement.BindString(1, item.url.spec());
  statement.BindString(2, item.client_id.name_space);
  statement.BindString(3, item.client_id.id);
  statement.BindString(4, store_utils::ToDatabaseFilePath(item.file_path));
  statement.BindInt64(5, item.file_size);
  statement.BindTime(6, item.creation_time);
  statement.BindTime(7, item.last_access_time);
  statement.BindInt(8, item.access_count);
  statement.BindString16(9, item.title);
  statement.BindString(10, item.original_url_if_different.spec());
  statement.BindString(11, item.request_origin);
  statement.BindInt64(12, item.system_download_id);
  statement.BindTime(13, item.file_missing_time);
  statement.BindString(14, item.digest);
  statement.BindString(15, item.snippet);
  statement.BindString(16, item.attribution);

  if (!statement.Run())
    return ItemActionStatus::STORE_ERROR;
  if (db->GetLastChangeCount() == 0)
    return ItemActionStatus::ALREADY_EXISTS;
  return ItemActionStatus::SUCCESS;
}

}  // namespace

AddPageTask::AddPageTask(OfflinePageMetadataStore* store,
                         const OfflinePageItem& offline_page,
                         AddPageTaskCallback callback)
    : store_(store),
      offline_page_(offline_page),
      callback_(std::move(callback)) {
  DCHECK(!callback_.is_null());
}

AddPageTask::~AddPageTask() = default;

void AddPageTask::Run() {
  if (!store_) {
    InformAddPageDone(AddPageResult::STORE_FAILURE);
    return;
  }
  store_->Execute(base::BindOnce(&AddOfflinePageSync, offline_page_),
                  base::BindOnce(&AddPageTask::OnAddPageDone,
                                 weak_ptr_factory_.GetWeakPtr()),
                  ItemActionStatus::STORE_ERROR);
}

void AddPageTask::OnAddPageDone(ItemActionStatus status) {
  AddPageResult result = ItemActionStatusToAddPageResult(status);
  InformAddPageDone(result);
}

void AddPageTask::InformAddPageDone(AddPageResult result) {
  std::move(callback_).Run(result);
  TaskComplete();
}

}  // namespace offline_pages
