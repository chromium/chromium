// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/model/update_publish_id_task.h"

#include "base/bind.h"
#include "components/offline_pages/core/client_namespace_constants.h"
#include "components/offline_pages/core/model/offline_page_model_utils.h"
#include "components/offline_pages/core/offline_page_metadata_store.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

namespace {

bool UpdatePublishIdSync(const PublishedArchiveId& publish_id,
                         int64_t offline_id,
                         sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return false;

  // Update the file_path to point to the new path.
  static const char kSqlUpdate[] =
      "UPDATE OR IGNORE offlinepages_v1"
      " SET file_path=?,system_download_id=?"
      " WHERE offline_id=?";
  sql::Statement update_statement(
      db->GetCachedStatement(SQL_FROM_HERE, kSqlUpdate));
  update_statement.BindString(0, offline_pages::store_utils::ToDatabaseFilePath(
                                     publish_id.new_file_path));
  update_statement.BindInt64(1, publish_id.download_id);
  update_statement.BindInt64(2, offline_id);

  if (!update_statement.Run())
    return false;

  if (!transaction.Commit())
    return false;

  return true;
}

}  // namespace

UpdatePublishIdTask::UpdatePublishIdTask(
    OfflinePageMetadataStore* store,
    int64_t offline_id,
    const PublishedArchiveId& publish_id,
    base::OnceCallback<void(bool)> callback)
    : store_(store),
      offline_id_(offline_id),
      publish_id_(publish_id),
      callback_(std::move(callback)) {
  DCHECK(store_);
}

UpdatePublishIdTask::~UpdatePublishIdTask() {}

void UpdatePublishIdTask::Run() {
  store_->Execute(
      base::BindOnce(&UpdatePublishIdSync, publish_id_, offline_id_),
      base::BindOnce(&UpdatePublishIdTask::OnUpdatePublishIdDone,
                     weak_ptr_factory_.GetWeakPtr()),
      false);
}

void UpdatePublishIdTask::OnUpdatePublishIdDone(bool result) {
  // Forward the updated offline page to the callback
  std::move(callback_).Run(result);
  TaskComplete();
}

}  // namespace offline_pages
