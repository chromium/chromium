// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/core/page_content_store.h"

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/task/thread_pool.h"
#include "components/database_utils/url_converter.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "sql/init_status.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace optimization_guide {

PageContentStore::PageContentStore(const base::FilePath& db_path)
    : db_path_(db_path), db_("PageContentStore") {
  db_initialized_ = InitializeDb();
}

PageContentStore::~PageContentStore() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool PageContentStore::InitializeDb() {
  CHECK(!db_initialized_);

  db_.set_error_callback(base::BindRepeating([](int extended_error,
                                                sql::Statement* stmt) {
    // TODO(ssid): Add error handling.
    VLOG(1) << "PageContentStore database operation failed: " << extended_error
            << ", " << stmt->GetSQLStatement();
  }));

  if (!db_.Open(db_path_)) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_.DoesTableExist("page_metadata")) {
    static const char kCreateMetadataTableSql[] =
        "CREATE TABLE page_metadata ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "tab_id INTEGER UNIQUE,"
        "url TEXT,"
        "content_id INTEGER,"
        "visit_timestamp INTEGER,"
        "extraction_timestamp INTEGER)";
    if (!db_.Execute(kCreateMetadataTableSql)) {
      return false;
    }
  }

  if (!db_.DoesTableExist("page_content")) {
    static const char kCreateContentTableSql[] =
        "CREATE TABLE page_content ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "value BLOB)";
    if (!db_.Execute(kCreateContentTableSql)) {
      return false;
    }
  }

  static const char kCreateIndexTabIdSql[] =
      "CREATE INDEX IF NOT EXISTS page_metadata_tab_id_index ON "
      "page_metadata(tab_id)";
  if (!db_.Execute(kCreateIndexTabIdSql)) {
    return false;
  }

  static const char kCreateIndexVisitTimestampSql[] =
      "CREATE INDEX IF NOT EXISTS page_metadata_visit_timestamp_index ON "
      "page_metadata(visit_timestamp)";
  if (!db_.Execute(kCreateIndexVisitTimestampSql)) {
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  return true;
}

void PageContentStore::InitWithEncryptor(os_crypt_async::Encryptor encryptor) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  encryptor_ = std::move(encryptor);
}

bool PageContentStore::AddPageContent(const GURL& url,
                                      const proto::PageContext& page_context,
                                      base::Time visit_timestamp,
                                      base::Time extraction_timestamp,
                                      std::optional<int64_t> tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_ || !encryptor_.has_value()) {
    return false;
  }

  // Delete existing contents, else the insert call would fail since tab_id is
  // marked unique
  if (tab_id.has_value()) {
    DeletePageContentForTab(tab_id.value());
  }

  std::string serialized_page_context;
  if (!page_context.SerializeToString(&serialized_page_context)) {
    return false;
  }
  std::string encrypted_page_context;
  if (!encryptor_->EncryptString(serialized_page_context,
                                 &encrypted_page_context)) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static const char kInsertContentSql[] =
      "INSERT INTO page_content (value) VALUES (?)";
  sql::Statement content_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertContentSql));
  content_statement.BindBlob(0, encrypted_page_context);
  if (!content_statement.Run()) {
    return false;
  }
  const int64_t content_id = db_.GetLastInsertRowId();

  static const char kInsertMetadataSql[] =
      "INSERT INTO page_metadata (url, content_id, visit_timestamp, "
      "extraction_timestamp, tab_id) "
      "VALUES (?, ?, ?, ?, ?)";
  sql::Statement metadata_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kInsertMetadataSql));
  metadata_statement.BindString(0, database_utils::GurlToDatabaseUrl(url));
  metadata_statement.BindInt64(1, content_id);
  metadata_statement.BindTime(2, visit_timestamp);
  metadata_statement.BindTime(3, extraction_timestamp);
  if (tab_id.has_value()) {
    metadata_statement.BindInt64(4, tab_id.value());
  } else {
    metadata_statement.BindNull(4);
  }

  if (!metadata_statement.Run()) {
    return false;
  }
  return transaction.Commit();
}

std::optional<proto::PageContext> PageContentStore::GetPageContent(
    const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_ || !encryptor_.has_value()) {
    return std::nullopt;
  }

  static const char kSelectSql[] =
      "SELECT pc.value FROM page_content pc "
      "JOIN page_metadata pm ON pc.id = pm.content_id "
      "WHERE pm.url = ? "
      "ORDER BY pm.visit_timestamp DESC "
      "LIMIT 1";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindString(0, database_utils::GurlToDatabaseUrl(url));

  return GetPageContentFromStatement(&statement);
}

std::optional<proto::PageContext> PageContentStore::GetPageContentForTab(
    int64_t tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_ || !encryptor_.has_value()) {
    return std::nullopt;
  }

  static const char kSelectSql[] =
      "SELECT pc.value FROM page_content pc "
      "JOIN page_metadata pm ON pc.id = pm.content_id "
      "WHERE pm.tab_id = ?";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));
  statement.BindInt64(0, tab_id);

  return GetPageContentFromStatement(&statement);
}

std::optional<proto::PageContext> PageContentStore::GetPageContentFromStatement(
    sql::Statement* statement) {
  if (!statement->Step()) {
    return std::nullopt;
  }

  std::string encrypted_page_context = statement->ColumnBlobAsString(0);
  std::string serialized_page_context;
  if (!encryptor_->DecryptString(encrypted_page_context,
                                 &serialized_page_context)) {
    return std::nullopt;
  }
  proto::PageContext page_context;
  if (!page_context.ParseFromString(serialized_page_context)) {
    return std::nullopt;
  }
  return page_context;
}

bool PageContentStore::DeletePageContentOlderThan(base::Time timestamp) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static const char kDeleteContentSql[] =
      "DELETE FROM page_content WHERE id IN ("
      "SELECT content_id FROM page_metadata WHERE visit_timestamp < ?)";
  sql::Statement delete_content_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteContentSql));
  delete_content_statement.BindTime(0, timestamp);
  if (!delete_content_statement.Run()) {
    return false;
  }

  static const char kDeleteMetadataSql[] =
      "DELETE FROM page_metadata WHERE visit_timestamp < ?";
  sql::Statement delete_metadata_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteMetadataSql));
  delete_metadata_statement.BindTime(0, timestamp);
  if (!delete_metadata_statement.Run()) {
    return false;
  }

  return transaction.Commit();
}

bool PageContentStore::DeletePageContentForTab(int64_t tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  static const char kDeleteContentSql[] =
      "DELETE FROM page_content WHERE id IN "
      "(SELECT content_id FROM page_metadata WHERE tab_id = ?)";
  sql::Statement delete_content_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteContentSql));
  delete_content_statement.BindInt64(0, tab_id);
  if (!delete_content_statement.Run()) {
    return false;
  }

  static const char kDeleteMetadataSql[] =
      "DELETE FROM page_metadata WHERE tab_id = ?";
  sql::Statement delete_metadata_statement(
      db_.GetCachedStatement(SQL_FROM_HERE, kDeleteMetadataSql));
  delete_metadata_statement.BindInt64(0, tab_id);
  if (!delete_metadata_statement.Run()) {
    return false;
  }
  return transaction.Commit();
}

bool PageContentStore::DeleteAllEntries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_) {
    return false;
  }

  sql::Transaction transaction(&db_);
  if (!transaction.Begin()) {
    return false;
  }

  if (!db_.Execute("DELETE FROM page_content")) {
    return false;
  }
  if (!db_.Execute("DELETE FROM page_metadata")) {
    return false;
  }

  return transaction.Commit();
}

std::vector<int64_t> PageContentStore::GetAllTabIds() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_initialized_) {
    return {};
  }

  static const char kSelectSql[] =
      "SELECT tab_id FROM page_metadata WHERE tab_id IS NOT NULL";
  sql::Statement statement(db_.GetCachedStatement(SQL_FROM_HERE, kSelectSql));

  std::vector<int64_t> tab_ids;
  while (statement.Step()) {
    tab_ids.push_back(statement.ColumnInt64(0));
  }
  return tab_ids;
}

}  // namespace optimization_guide
