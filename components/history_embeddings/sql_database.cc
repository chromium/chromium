// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "components/history_embeddings/passages_util.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace history_embeddings {

namespace {

[[nodiscard]] bool InitSchema(sql::Database& db) {
  static constexpr char kSqlCreateTablePassages[] =
      "CREATE TABLE IF NOT EXISTS passages("
      // The URL associated with these passages, as stored in History.
      "url_id INTEGER PRIMARY KEY NOT NULL,"
      // The Visit from which these passages were extracted. This is to allow
      // us to properly expire and delete passage data when the associated
      // visit is deleted.
      "visit_id INTEGER NOT NULL,"
      // Store the associated visit time too, so we have a way to scrub expired
      // entries if we ever miss deletion events from History. This can happen
      // if Chrome shuts down unexpectedly or if History DB is razed.
      "visit_time INTEGER NOT NULL,"
      // An opaque encrypted blob of passages.
      "passages_blob BLOB NOT NULL);";
  if (!db.Execute(kSqlCreateTablePassages)) {
    return false;
  }

  // Create an index over visit_id so we can quickly delete passages associated
  // with visits that get deleted.
  if (!db.Execute("CREATE INDEX IF NOT EXISTS index_passages_visit_id ON "
                  "passages (visit_id)")) {
    return false;
  }

  return true;
}

}  // namespace

SqlDatabase::SqlDatabase(const base::FilePath& storage_dir)
    : storage_dir_(storage_dir) {}

SqlDatabase::~SqlDatabase() = default;

bool SqlDatabase::LazyInit() {
  // TODO(b/325524013): Decide on a number of retries for initialization.
  // TODO(b/325524013): Add metrics around lazy initialization success rate.
  if (!db_init_status_.has_value()) {
    db_init_status_ = InitInternal(storage_dir_);
  }

  return *db_init_status_ == sql::InitStatus::INIT_OK;
}

sql::InitStatus SqlDatabase::InitInternal(const base::FilePath& storage_dir) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_.set_histogram_tag("HistoryEmbeddings");
  // base::Unretained is okay because `this` owns and outlives `db_`.
  db_.set_error_callback(base::BindRepeating(
      &SqlDatabase::DatabaseErrorCallback, base::Unretained(this)));

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

  if (!InitSchema(db_)) {
    return sql::INIT_FAILURE;
  }

  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  return sql::InitStatus::INIT_OK;
}

bool SqlDatabase::InsertOrReplacePassages(
    history::URLID url_id,
    history::VisitID visit_id,
    base::Time visit_time,
    const proto::PassagesValue& passages) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!LazyInit()) {
    return false;
  }

  constexpr char kSqlInsertOrReplacePassages[] =
      "INSERT OR REPLACE INTO passages "
      "(url_id, visit_id, visit_time, passages_blob) "
      "VALUES (?,?,?,?)";
  DCHECK(db_.IsSQLValid(kSqlInsertOrReplacePassages));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlInsertOrReplacePassages));
  statement.BindInt64(0, url_id);
  statement.BindInt64(1, visit_id);
  statement.BindTime(2, visit_time);

  std::vector<uint8_t> blob = PassagesProtoToBlob(passages);
  if (blob.empty()) {
    return false;
  }
  statement.BindBlob(3, blob);

  return statement.Run();
}

// Gets the passages associated with `url_id`. Returns nullopt if there's
// nothing available.
std::optional<proto::PassagesValue> SqlDatabase::GetPassages(
    history::URLID url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit()) {
    return std::nullopt;
  }

  constexpr char kSqlSelectPassages[] =
      "SELECT passages_blob FROM passages WHERE url_id = ?";
  DCHECK(db_.IsSQLValid(kSqlSelectPassages));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectPassages));
  statement.BindInt64(0, url_id);

  if (statement.Step()) {
    return PassagesBlobToProto(statement.ColumnBlob(0));
  }

  return std::nullopt;
}

void SqlDatabase::DatabaseErrorCallback(int extended_error,
                                        sql::Statement* statement) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(b/325524013): Handle razing the database on catastrophic error.

  // The default handling is to assert on debug and to ignore on release.
  // This is because database errors happen in the wild due to faulty hardware,
  // or are sometimes transitory, and we want Chrome to carry on when possible.
  if (!sql::Database::IsExpectedSqliteError(extended_error)) {
    DLOG(FATAL) << db_.GetErrorMessage();
  }
}

}  // namespace history_embeddings
