// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/import_archives_task.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace offline_pages {
namespace {

std::unique_ptr<std::vector<PrefetchArchiveInfo>> GetArchivesSync(
    sql::Database* db) {
  static const char kSql[] =
      "SELECT offline_id,client_namespace,guid,requested_url,"
      "final_archived_url,title,file_path,file_size,favicon_url,snippet,"
      "attribution"
      " FROM prefetch_items"
      " WHERE state=?";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::DOWNLOADED));

  std::unique_ptr<std::vector<PrefetchArchiveInfo>> archives;
  while (statement.Step()) {
    PrefetchArchiveInfo archive;
    archive.offline_id = statement.ColumnInt64(0);
    archive.client_id.name_space = statement.ColumnString(1);
    // The client ID is the GUID of the download so that it can be shown
    // consistently in Downloads Home.
    archive.client_id.id = statement.ColumnString(2);
    archive.url = GURL(statement.ColumnString(3));
    archive.final_archived_url = GURL(statement.ColumnString(4));
    archive.title = statement.ColumnString16(5);
    archive.file_path =
        store_utils::FromDatabaseFilePath(statement.ColumnString(6));
    archive.file_size = statement.ColumnInt64(7);
    archive.favicon_url = GURL(statement.ColumnString(8));
    archive.snippet = statement.ColumnString(9);
    archive.attribution = statement.ColumnString(10);
    if (!archives)
      archives = std::make_unique<std::vector<PrefetchArchiveInfo>>();
    archives->push_back(archive);
  }

  return archives;
}

bool UpdateToImportingStateSync(int64_t offline_id, sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items"
      " SET state=?"
      " WHERE offline_id=?";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::IMPORTING));
  statement.BindInt64(1, offline_id);

  return statement.Run();
}

std::unique_ptr<std::vector<PrefetchArchiveInfo>>
GetArchivesAndUpdateToImportingStateSync(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return nullptr;

  std::unique_ptr<std::vector<PrefetchArchiveInfo>> archives =
      GetArchivesSync(db);
  if (!archives)
    return nullptr;

  for (const auto& archive : *archives) {
    if (!UpdateToImportingStateSync(archive.offline_id, db))
      return nullptr;
  }

  if (!transaction.Commit())
    return nullptr;

  return archives;
}

}  // namespace

ImportArchivesTask::ImportArchivesTask(PrefetchStore* prefetch_store,
                                       PrefetchImporter* prefetch_importer)
    : prefetch_store_(prefetch_store), prefetch_importer_(prefetch_importer) {}

ImportArchivesTask::~ImportArchivesTask() {}

void ImportArchivesTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(&GetArchivesAndUpdateToImportingStateSync),
      base::BindOnce(&ImportArchivesTask::OnArchivesRetrieved,
                     weak_ptr_factory_.GetWeakPtr()),
      std::unique_ptr<std::vector<PrefetchArchiveInfo>>());
}

void ImportArchivesTask::OnArchivesRetrieved(
    std::unique_ptr<std::vector<PrefetchArchiveInfo>> archives) {
  if (archives) {
    for (const auto& archive : *archives)
      prefetch_importer_->ImportArchive(archive);
  }

  TaskComplete();
}

}  // namespace offline_pages
