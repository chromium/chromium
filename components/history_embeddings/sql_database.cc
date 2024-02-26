// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace history_embeddings {

SqlDatabase::SqlDatabase(const base::FilePath& storage_dir) {
  // TODO(b/325524013): Move this to the beginning of individual operations.
  LazyInit(storage_dir);
}

SqlDatabase::~SqlDatabase() = default;

bool SqlDatabase::LazyInit(const base::FilePath& storage_dir) {
  // TODO(b/325524013): Decide on a number of retries for initialization.
  // TODO(b/325524013): Add metrics around lazy initialization success rate.
  if (db_init_status_.has_value()) {
    return *db_init_status_;
  }

  db_init_status_ = InitInternal(storage_dir);
  return *db_init_status_ = sql::InitStatus::INIT_OK;
}

sql::InitStatus SqlDatabase::InitInternal(const base::FilePath& storage_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.set_histogram_tag("HistoryEmbeddings");

  base::FilePath db_file_path = storage_dir.Append(kHistoryEmbeddingsName);

  if (!db_.Open(db_file_path)) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Raze old incompatible databases.
  constexpr int kLowestSupportedVersion = 1;
  constexpr int kCurrentVersion = 1;
  if (!sql::MetaTable::RazeIfIncompatible(&db_, kLowestSupportedVersion,
                                          kCurrentVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Wrap initialization in a transaction to make it atomic.
  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return sql::InitStatus::INIT_FAILURE;
  }

  // Initialize the current version meta table. Safest to leave the compatible
  // version equal to the current version - unless we know we're making a very
  // safe backwards-compatible schema change.
  sql::MetaTable meta_table;
  if (!meta_table.Init(&db_, kCurrentVersion,
                       /*compatible_version=*/kCurrentVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }
  if (meta_table.GetCompatibleVersionNumber() > kCurrentVersion) {
    LOG(ERROR) << "HistoryEmbeddings database is too new.";
    return sql::INIT_TOO_NEW;
  }

  return sql::InitStatus::INIT_OK;
}

}  // namespace history_embeddings
