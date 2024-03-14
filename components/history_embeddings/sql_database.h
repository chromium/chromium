// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_

#include <optional>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/history_embeddings/vector_database.h"
#include "sql/database.h"
#include "sql/init_status.h"

namespace history_embeddings {

inline constexpr base::FilePath::CharType kHistoryEmbeddingsName[] =
    FILE_PATH_LITERAL("HistoryEmbeddings");

// Wraps the SQLite database that provides on-disk storage for History
// Embeddings component. This class is expected to live and die on a backend
// sequence owned by `HistoryEmbeddingsService`.
class SqlDatabase : public VectorDatabase {
 public:
  // `storage_dir` will generally be the Profile directory.
  explicit SqlDatabase(const base::FilePath& storage_dir);
  SqlDatabase(const SqlDatabase&) = delete;
  SqlDatabase& operator=(const SqlDatabase&) = delete;
  ~SqlDatabase() override;

  // Inserts or replaces `passages` keyed by `url_id`. `visit_id` and
  // `visit_time` are needed too, to respect History deletions and expirations.
  // If there are existing passages for `url_id`, they are replaced. Returns
  // whether this operation was successful.
  bool InsertOrReplacePassages(const UrlPassages& url_passages);

  // Gets the passages associated with `url_id`. Returns nullopt if there's
  // nothing available.
  std::optional<proto::PassagesValue> GetPassages(history::URLID url_id);

  // VectorDatabase:
  size_t GetEmbeddingDimensions() const override;
  bool AddUrlEmbeddings(const UrlEmbeddings& url_embeddings) override;
  std::unique_ptr<EmbeddingsIterator> MakeEmbeddingsIterator() override;

 private:
  // Initializes the database, if it's not already initialized. Returns true if
  // the initialization was successful (or already succeeded in the past).
  bool LazyInit();
  // Helper function for LazyInit(). Should only be called by LazyInit().
  sql::InitStatus InitInternal(const base::FilePath& storage_dir);

  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* statement);

  // The directory storing the database.
  const base::FilePath storage_dir_;

  // The underlying SQL database.
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);

  // An iteration statement with lifetime bounded by above `db_`.
  // Only one iterator can be used at a time.
  std::unique_ptr<sql::Statement> iteration_statement_;

  // The initialization status of the database. It's not set if never attempted.
  std::optional<sql::InitStatus> db_init_status_ = std::nullopt;

  // Verifies that all operations happen on the same sequence.
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SqlDatabase> weak_ptr_factory_;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_SQL_DATABASE_H_
