// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "sql/database.h"
#include "sql/init_status.h"

namespace history_embeddings {

inline constexpr base::FilePath::CharType kHistoryEmbeddingsName[] =
    FILE_PATH_LITERAL("HistoryEmbeddings");

// Wraps the Sqlite database that provides on-disk storage for History
// Embeddings component. This class is expected to live and die on a backend
// sequence owned by `HistoryEmbeddingsService`.
class SqlDatabase {
 public:
  // `storage_dir` will generally be the Profile directory.
  explicit SqlDatabase(const base::FilePath& storage_dir);
  SqlDatabase(const SqlDatabase&) = delete;
  SqlDatabase& operator=(const SqlDatabase&) = delete;
  ~SqlDatabase();

 private:
  // Initializes the database, if it's not already initialized. Returns true if
  // the initialization was successful (or already succeeded in the past).
  bool LazyInit(const base::FilePath& storage_dir);
  // Helper function for LazyInit(). Should only be called by LazyInit().
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  // The initialization status of the database. It's not set if never attempted.
  std::optional<sql::InitStatus> db_init_status_ = std::nullopt;

  // Verifies that all operations happen on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
