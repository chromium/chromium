// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history_embeddings/sql_database.h"

#include <algorithm>

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/metrics/histogram_functions.h"
#include "base/sequence_checker.h"
#include "components/history/core/browser/history_backend.h"
#include "components/history_embeddings/history_embeddings_features.h"
#include "components/history_embeddings/passages_util.h"
#include "components/history_embeddings/proto/history_embeddings.pb.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"
#include "sql/transaction.h"

namespace history_embeddings {

// These database versions should roll together unless we develop migrations.
constexpr int kLowestSupportedDatabaseVersion = 1;
constexpr int kCurrentDatabaseVersion = 1;

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
                  "passages(visit_id)")) {
    return false;
  }

  static constexpr char kSqlCreateTableEmbeddings[] =
      "CREATE TABLE IF NOT EXISTS embeddings("
      // The URL associated with these embeddings, as stored in History.
      "url_id INTEGER PRIMARY KEY NOT NULL,"
      // The Visit from which these embeddings were computed. This is to allow
      // us to properly expire and delete embedding data when the associated
      // visit is deleted.
      "visit_id INTEGER NOT NULL,"
      // Store the associated visit time too, so we have a way to scrub expired
      // entries if we ever miss deletion events from History. This can happen
      // if Chrome shuts down unexpectedly or if History DB is razed.
      "visit_time INTEGER NOT NULL,"
      // A serialized proto::EmbeddingsValue message containing all embedding
      // vectors from this URL/visit source.
      "embeddings_blob BLOB NOT NULL);";
  if (!db.Execute(kSqlCreateTableEmbeddings)) {
    return false;
  }

  // Create an index over visit_id so we can quickly delete embeddings
  // associated with visits that get deleted.
  if (!db.Execute("CREATE INDEX IF NOT EXISTS index_embeddings_visit_id ON "
                  "embeddings(visit_id)")) {
    return false;
  }

  return true;
}

}  // namespace

SqlDatabase::SqlDatabase(const base::FilePath& storage_dir)
    : storage_dir_(storage_dir), weak_ptr_factory_(this) {}

SqlDatabase::~SqlDatabase() = default;

void SqlDatabase::SetEmbedderMetadata(EmbedderMetadata embedder_metadata,
                                      os_crypt_async::Encryptor encryptor) {
  embedder_metadata_ = embedder_metadata;
  CHECK(!encryptor_.has_value()) << "Cannot call SetEmbedderMetadata twice.";
  encryptor_.emplace(std::move(encryptor));
}

bool SqlDatabase::LazyInit(bool force_init_for_deletion) {
  // Only use `force_init_for_deletion` if normal full initialization fails
  // and a deletion request is being applied. Never use if normal init succeeds.
  CHECK(!force_init_for_deletion || !db_init_status_.has_value());

  if (!db_init_status_.has_value()) {
    // Don't attempt initialization until ready, unless forced for the
    // data deletion flow.
    if (!embedder_metadata_ && !force_init_for_deletion) {
      return false;
    }

    db_init_status_ = InitInternal(storage_dir_, force_init_for_deletion);
    base::UmaHistogramBoolean("History.Embeddings.DatabaseInitialized",
                              *db_init_status_ == sql::InitStatus::INIT_OK);
  }

  return *db_init_status_ == sql::InitStatus::INIT_OK;
}

sql::InitStatus SqlDatabase::InitInternal(const base::FilePath& storage_dir,
                                          bool force_init_for_deletion) {
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
  if (sql::MetaTable::RazeIfIncompatible(&db_, kLowestSupportedDatabaseVersion,
                                         kCurrentDatabaseVersion) ==
      sql::RazeIfIncompatibleResult::kFailed) {
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
  if (!meta_table.Init(&db_, kCurrentDatabaseVersion,
                       /*compatible_version=*/kCurrentDatabaseVersion)) {
    return sql::InitStatus::INIT_FAILURE;
  }
  if (meta_table.GetCompatibleVersionNumber() > kCurrentDatabaseVersion) {
    LOG(ERROR) << "HistoryEmbeddings database is too new.";
    return sql::INIT_TOO_NEW;
  }

  if (!InitSchema(db_)) {
    return sql::INIT_FAILURE;
  }

  if (!transaction.Commit()) {
    return sql::INIT_FAILURE;
  }

  // Delete passages and embeddings for visits that are beyond the data
  // retention window. The history system automatically expires data while
  // Chrome is running, but it's possible to miss events or start Chrome after
  // some down time, so this prevents long term accidental retention edge cases.
  DeleteExpiredData(/*expiration_time=*/base::Time::Now() -
                    base::Days(history::HistoryBackend::kExpireDaysThreshold));

  // It's possible to get here without `embedder_metadata_` if forcing for
  // data deletion. In that case, don't check or change meta table.
  if (embedder_metadata_.has_value()) {
    constexpr char kKeyModelVersion[] = "model_version";
    int model_version = 0;
    meta_table.GetValue(kKeyModelVersion, &model_version);
    if (model_version != embedder_metadata_->model_version ||
        kDeleteEmbeddings.Get()) {
      // Old version embeddings can't be used with new model. Simply delete them
      // all and set new version. Passages can be used for reconstruction later.
      constexpr char kSqlDeleteFromEmbeddings[] = "DELETE FROM embeddings;";
      if (!db_.Execute(kSqlDeleteFromEmbeddings) ||
          !meta_table.SetValue(kKeyModelVersion,
                               embedder_metadata_->model_version)) {
        return sql::InitStatus::INIT_FAILURE;
      }
    }
  }

  return sql::InitStatus::INIT_OK;
}

void SqlDatabase::Close() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  db_.Close();
  db_.reset_error_callback();
  db_init_status_.reset();
}

bool SqlDatabase::InsertOrReplacePassages(const UrlPassages& url_passages) {
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
  statement.BindInt64(0, url_passages.url_id);
  statement.BindInt64(1, url_passages.visit_id);
  statement.BindTime(2, url_passages.visit_time);

  std::vector<uint8_t> blob =
      PassagesProtoToBlob(url_passages.passages, *encryptor_);
  if (blob.empty()) {
    return false;
  }
  statement.BindBlob(3, blob);

  return statement.Run();
}

bool SqlDatabase::InsertOrReplaceEmbeddings(
    const UrlEmbeddings& url_embeddings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (url_embeddings.embeddings.size() == 0) {
    return false;
  }

  if (!LazyInit()) {
    return false;
  }

  constexpr char kSqlInsertOrReplaceEmbeddings[] =
      "INSERT OR REPLACE INTO embeddings "
      "(url_id, visit_id, visit_time, embeddings_blob) "
      "VALUES (?,?,?,?)";
  DCHECK(db_.IsSQLValid(kSqlInsertOrReplaceEmbeddings));
  sql::Statement statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kSqlInsertOrReplaceEmbeddings));
  statement.BindInt64(0, url_embeddings.url_id);
  statement.BindInt64(1, url_embeddings.visit_id);
  statement.BindTime(2, url_embeddings.visit_time);

  proto::EmbeddingsValue value;
  for (const Embedding& embedding : url_embeddings.embeddings) {
    CHECK_EQ(GetEmbeddingDimensions(), embedding.Dimensions());
    proto::EmbeddingVector* vector = value.add_vectors();
    for (float f : embedding.GetData()) {
      vector->add_floats(f);
    }
    vector->set_passage_word_count(embedding.GetPassageWordCount());
  }
  statement.BindBlob(3, value.SerializeAsString());

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
    return PassagesBlobToProto(statement.ColumnBlob(0), *encryptor_);
  }

  return std::nullopt;
}

std::optional<UrlPassagesEmbeddings> SqlDatabase::GetUrlData(
    history::URLID url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit()) {
    return {};
  }

  history::VisitID visit_id = 0;
  base::Time visit_time;
  std::optional<proto::PassagesValue> passages;
  {
    constexpr char kSqlSelectVisitIdAndPassages[] =
        "SELECT visit_id, visit_time, passages_blob FROM passages WHERE url_id "
        "= ?";
    DCHECK(db_.IsSQLValid(kSqlSelectVisitIdAndPassages));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectVisitIdAndPassages));
    statement.BindInt64(0, url_id);

    if (statement.Step()) {
      visit_id = statement.ColumnInt64(0);
      visit_time = statement.ColumnTime(1);
      passages = PassagesBlobToProto(statement.ColumnBlob(2), *encryptor_);
    }
  }
  if (!passages.has_value() || visit_id == 0) {
    return {};
  }

  UrlPassagesEmbeddings url_data(url_id, visit_id, visit_time);
  url_data.url_passages.passages = std::move(passages.value());
  bool loaded_missized_embedding = false;
  {
    constexpr char kSqlSelectEmbeddings[] =
        "SELECT embeddings_blob FROM embeddings "
        "WHERE visit_id = ?";
    DCHECK(db_.IsSQLValid(kSqlSelectEmbeddings));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlSelectEmbeddings));
    statement.BindInt64(0, visit_id);

    if (statement.Step()) {
      base::span<const uint8_t> blob = statement.ColumnBlob(0);

      proto::EmbeddingsValue value;
      if (!value.ParseFromArray(blob.data(), blob.size())) {
        return url_data;
      }
      for (const proto::EmbeddingVector& vector : value.vectors()) {
        url_data.url_embeddings.embeddings.emplace_back(
            std::vector(vector.floats().cbegin(), vector.floats().cend()),
            vector.passage_word_count());
        if (url_data.url_embeddings.embeddings.back().Dimensions() !=
            GetEmbeddingDimensions()) {
          url_data.url_embeddings.embeddings.clear();
          loaded_missized_embedding = true;
          break;
        }
      }
    }
  }
  base::UmaHistogramBoolean("History.Embeddings.LoadedMissizedEmbedding",
                            loaded_missized_embedding);
  return url_data;
}

std::vector<UrlPassages> SqlDatabase::GetUrlPassagesWithoutEmbeddings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit()) {
    return {};
  }

  constexpr char kSqlSelectPassagesWithoutEmbeddings[] =
      "SELECT url_id, visit_id, visit_time, passages_blob "
      "FROM passages WHERE url_id NOT IN (SELECT url_id FROM embeddings);";
  DCHECK(db_.IsSQLValid(kSqlSelectPassagesWithoutEmbeddings));
  sql::Statement statement(db_.GetCachedStatement(
      SQL_FROM_HERE, kSqlSelectPassagesWithoutEmbeddings));

  std::vector<UrlPassages> all_url_passages;
  while (statement.Step()) {
    std::optional<proto::PassagesValue> passages_value =
        PassagesBlobToProto(statement.ColumnBlob(3), *encryptor_);
    if (passages_value.has_value()) {
      UrlPassages& url_passages = all_url_passages.emplace_back(
          statement.ColumnInt64(0), statement.ColumnInt64(1),
          statement.ColumnTime(2));
      url_passages.passages = std::move(passages_value.value());
    }
  }
  return all_url_passages;
}

size_t SqlDatabase::GetEmbeddingDimensions() const {
  return embedder_metadata_->output_size;
}

bool SqlDatabase::AddUrlData(UrlPassagesEmbeddings url_data) {
  return InsertOrReplacePassages(url_data.url_passages) &&
         InsertOrReplaceEmbeddings(url_data.url_embeddings);
}

constexpr char kSqlSelectPassagesAndEmbeddings[] =
    "SELECT passages.url_id, passages.visit_id, passages.visit_time, "
    "passages.passages_blob, embeddings.embeddings_blob "
    "FROM passages "
    "INNER JOIN embeddings ON passages.url_id = embeddings.url_id";
constexpr char kSqlSelectPassagesAndEmbeddingsWithinTimeRange[] =
    "SELECT passages.url_id, passages.visit_id, passages.visit_time, "
    "passages.passages_blob, embeddings.embeddings_blob "
    "FROM passages "
    "INNER JOIN embeddings ON passages.url_id = embeddings.url_id "
    "WHERE passages.visit_time >= ?";

std::unique_ptr<VectorDatabase::UrlDataIterator>
SqlDatabase::MakeUrlDataIterator(std::optional<base::Time> time_range_start) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!LazyInit()) {
    return nullptr;
  }

  DCHECK(db_.IsSQLValid(kSqlSelectPassagesAndEmbeddings));
  DCHECK(db_.IsSQLValid(kSqlSelectPassagesAndEmbeddingsWithinTimeRange));

  struct RowDataIterator : public UrlDataIterator {
    explicit RowDataIterator(base::WeakPtr<SqlDatabase> sql_database,
                             std::optional<base::Time> time_range_start)
        : sql_database(sql_database), data(0, 0, base::Time()) {
      CHECK(!sql_database->iteration_statement_);
      if (time_range_start.has_value()) {
        sql_database->iteration_statement_ = std::make_unique<sql::Statement>(
            sql_database->db_.GetCachedStatement(
                SQL_FROM_HERE, kSqlSelectPassagesAndEmbeddingsWithinTimeRange));
        sql_database->iteration_statement_->BindTime(0,
                                                     time_range_start.value());
      } else {
        sql_database->iteration_statement_ = std::make_unique<sql::Statement>(
            sql_database->db_.GetCachedStatement(
                SQL_FROM_HERE, kSqlSelectPassagesAndEmbeddings));
      }
    }
    ~RowDataIterator() override {
      if (sql_database) {
        sql_database->iteration_statement_.reset();
      }
      base::UmaHistogramCounts1000(
          "History.Embeddings.DatabaseIterationSkippedPassages",
          skipped_passages);
      base::UmaHistogramCounts1000(
          "History.Embeddings.DatabaseIterationSkippedEmbeddings",
          skipped_embeddings);
      base::UmaHistogramCounts1000(
          "History.Embeddings.DatabaseIterationSkippedMismatches",
          skipped_mismatches);
      base::UmaHistogramCounts1000(
          "History.Embeddings.DatabaseIterationSkippedMissizedEmbeddings",
          skipped_missized);
      base::UmaHistogramCounts10000(
          "History.Embeddings.DatabaseIterationYielded", yielded);
    }

    const UrlPassagesEmbeddings* Next() override {
      if (!sql_database) {
        return nullptr;
      }
      sql::Statement* statement = sql_database->iteration_statement_.get();
      CHECK(statement);
      // Don't expect perfect data; step until we find valid data.
      while (statement->Step()) {
        data = UrlPassagesEmbeddings(/*url_id=*/statement->ColumnInt64(0),
                                     /*visit_id=*/statement->ColumnInt64(1),
                                     /*visit_time=*/statement->ColumnTime(2));
        // Passages
        std::optional<proto::PassagesValue> passages_value =
            PassagesBlobToProto(statement->ColumnBlob(3),
                                *sql_database->encryptor_);
        if (!passages_value.has_value()) {
          skipped_passages++;
          continue;
        }
        data.url_passages.passages = std::move(passages_value.value());

        // Embeddings
        base::span<const uint8_t> blob = statement->ColumnBlob(4);
        proto::EmbeddingsValue value;
        if (!value.ParseFromArray(blob.data(), blob.size())) {
          skipped_embeddings++;
          continue;
        }
        for (const proto::EmbeddingVector& vector : value.vectors()) {
          data.url_embeddings.embeddings.emplace_back(
              std::vector(vector.floats().cbegin(), vector.floats().cend()),
              vector.passage_word_count());
        }
        const size_t expected_dimensions =
            sql_database->GetEmbeddingDimensions();
        if (std::ranges::any_of(data.url_embeddings.embeddings,
                                [=](const Embedding& embedding) {
                                  return embedding.Dimensions() !=
                                         expected_dimensions;
                                })) {
          skipped_missized++;
          continue;
        }

        // Confirm embeddings and passages are 1:1.
        if (data.url_embeddings.embeddings.empty() ||
            data.url_embeddings.embeddings.size() !=
                static_cast<size_t>(
                    data.url_passages.passages.passages_size())) {
          skipped_mismatches++;
          continue;
        }

        yielded++;
        return &data;
      }
      return nullptr;
    }

    base::WeakPtr<SqlDatabase> sql_database;
    UrlPassagesEmbeddings data;
    // Keep stats on any data loading failures, and report histogram in dtor.
    int skipped_passages = 0;
    int skipped_embeddings = 0;
    int skipped_mismatches = 0;
    int skipped_missized = 0;
    int yielded = 0;
  };

  return std::make_unique<RowDataIterator>(weak_ptr_factory_.GetWeakPtr(),
                                           time_range_start);
}

bool SqlDatabase::DeleteDataForUrlId(history::URLID url_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool close = false;
  if (!LazyInit()) {
    // Database isn't fully initialized. Attempt to force open it just for
    // deletion, and then close so it can be initialized normally later.
    if (!LazyInit(true)) {
      return false;
    }
    close = true;
  }

  bool delete_passages_success = false;
  {
    constexpr char kSqlDeleteFromPassagesByUrl[] =
        "DELETE FROM passages WHERE url_id=?";
    DCHECK(db_.IsSQLValid(kSqlDeleteFromPassagesByUrl));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromPassagesByUrl));
    statement.BindInt64(0, url_id);
    delete_passages_success = statement.Run();
  }
  bool delete_embeddings_success = false;
  {
    constexpr char kSqlDeleteFromEmbeddingsByUrl[] =
        "DELETE FROM embeddings WHERE url_id=?";
    DCHECK(db_.IsSQLValid(kSqlDeleteFromEmbeddingsByUrl));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromEmbeddingsByUrl));
    statement.BindInt64(0, url_id);
    delete_embeddings_success = statement.Run();
  }

  if (close) {
    Close();
  }

  return delete_passages_success && delete_embeddings_success;
}

bool SqlDatabase::DeleteDataForVisitId(history::VisitID visit_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool close = false;
  if (!LazyInit()) {
    // Database isn't fully initialized. Attempt to force open it just for
    // deletion, and then close so it can be initialized normally later.
    if (!LazyInit(true)) {
      return false;
    }
    close = true;
  }

  bool delete_passages_success = false;
  {
    constexpr char kSqlDeleteFromPassagesByVisit[] =
        "DELETE FROM passages WHERE visit_id=?";
    DCHECK(db_.IsSQLValid(kSqlDeleteFromPassagesByVisit));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromPassagesByVisit));
    statement.BindInt64(0, visit_id);
    delete_passages_success = statement.Run();
  }
  bool delete_embeddings_success = false;
  {
    constexpr char kSqlDeleteFromEmbeddingsByVisit[] =
        "DELETE FROM embeddings WHERE visit_id=?";
    DCHECK(db_.IsSQLValid(kSqlDeleteFromEmbeddingsByVisit));
    sql::Statement statement(
        db_.GetCachedStatement(SQL_FROM_HERE, kSqlDeleteFromEmbeddingsByVisit));
    statement.BindInt64(0, visit_id);
    delete_embeddings_success = statement.Run();
  }

  if (close) {
    Close();
  }

  return delete_passages_success && delete_embeddings_success;
}

bool SqlDatabase::DeleteAllData(bool delete_passages, bool delete_embeddings) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  bool close = false;
  if (!LazyInit()) {
    // Database isn't fully initialized. Attempt to force open it just for
    // deletion, and then close so it can be initialized normally later.
    if (!LazyInit(true)) {
      return false;
    }
    close = true;
  }

  bool delete_passages_success =
      !delete_passages || db_.Execute("DELETE FROM passages;");
  bool delete_embeddings_success =
      !delete_embeddings || db_.Execute("DELETE FROM embeddings;");

  if (close) {
    Close();
  }

  return delete_passages_success && delete_embeddings_success;
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

void SqlDatabase::DeleteExpiredData(base::Time expiration_time) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  constexpr char kSqlDeleteExpiredPassages[] =
      "DELETE FROM passages WHERE visit_time < ?;";
  constexpr char kSqlDeleteExpiredEmbeddings[] =
      "DELETE FROM embeddings WHERE visit_time < ?;";
  DCHECK(db_.IsSQLValid(kSqlDeleteExpiredPassages));
  DCHECK(db_.IsSQLValid(kSqlDeleteExpiredEmbeddings));

  sql::Statement expire_passages(
      db_.GetUniqueStatement(kSqlDeleteExpiredPassages));
  expire_passages.BindTime(0, expiration_time);
  expire_passages.Run();

  sql::Statement expire_embeddings(
      db_.GetUniqueStatement(kSqlDeleteExpiredEmbeddings));
  expire_embeddings.BindTime(0, expiration_time);
  expire_embeddings.Run();
}

}  // namespace history_embeddings
