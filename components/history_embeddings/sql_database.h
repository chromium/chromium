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
#include "base/time/time.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_embeddings/embedder.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/history_embeddings/vector_database.h"
#include "components/os_crypt/async/common/encryptor.h"
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

  // Provides embedder metadata to the database. The database cannot be
  // initialized until valid metadata is provided.
  void SetEmbedderMetadata(EmbedderMetadata embedder_metadata,
                           os_crypt_async::Encryptor encryptor);

  // Inserts or replaces `passages` keyed by `url_id`. `visit_id` and
  // `visit_time` are needed too, to respect History deletions and expirations.
  // If there are existing passages for `url_id`, they are replaced. Returns
  // whether this operation was successful.
  bool InsertOrReplacePassages(const UrlPassages& url_passages);

  // Store embeddings; this is part of the implementation for `AddUrlData`.
  bool InsertOrReplaceEmbeddings(const UrlEmbeddings& url_embeddings);

  // Gets the passages associated with `url_id`. Returns nullopt if there's
  // nothing available.
  std::optional<proto::PassagesValue> GetPassages(history::URLID url_id);

  // Gets passages and embeddings for given `url_id` if data is found.
  std::optional<UrlPassagesEmbeddings> GetUrlData(history::URLID url_id);

  // Gets all rows from passages where a corresponding row in embeddings
  // does not exist, keyed on url_id.
  std::vector<UrlPassages> GetUrlPassagesWithoutEmbeddings();

  // VectorDatabase:
  size_t GetEmbeddingDimensions() const override;
  bool AddUrlData(UrlPassagesEmbeddings url_passages_embeddings) override;
  std::unique_ptr<UrlDataIterator> MakeUrlDataIterator(
      std::optional<base::Time> time_range_start) override;

  // These three methods are used to keep the on-disk persistence in sync with
  // History deletions, either from user action or time-based expiration.
  bool DeleteDataForUrlId(history::URLID url_id);
  bool DeleteDataForVisitId(history::VisitID visit_id);

  // This is used to delete data for all URLs, either all data for history
  // deletion, or selectively for testing.
  bool DeleteAllData(bool delete_passages, bool delete_embeddings);

 private:
  // Initializes the database, if it's not already initialized. Returns true if
  // the initialization was successful (or already succeeded in the past).
  // If `force_init_for_deletion` is true, then some initialization requirements
  // are bypassed. In that case, embeddings are not guaranteed to be compatible
  // if the model version changes, so the database should be closed as soon as
  // deletion completes; then a normal full initialization can be done later
  // for typical data usage.
  bool LazyInit(bool force_init_for_deletion = false);
  // Helper function for LazyInit(). Should only be called by LazyInit().
  sql::InitStatus InitInternal(const base::FilePath& storage_dir,
                               bool force_init_for_deletion);
  // Close the database and reset lazy init status so that LazyInit will work as
  // normal with full initialization the next time it's called.  This doesn't
  // need to be called proactively unless `LazyInit` was called with
  // `force_init_for_deletion` set to true; see `LazyInit` comment.
  void Close();

  // Callback for database errors.
  void DatabaseErrorCallback(int extended_error, sql::Statement* statement);

  // Deletes passages and embeddings for visits before `expiration_time`.
  void DeleteExpiredData(base::Time expiration_time);

  // The directory storing the database.
  const base::FilePath storage_dir_;

  // Metadata of the embeddings model.
  std::optional<EmbedderMetadata> embedder_metadata_;

  std::optional<os_crypt_async::Encryptor> encryptor_;

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
