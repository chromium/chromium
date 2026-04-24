// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/content_annotations_table.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "components/database_utils/url_converter.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace accessibility_annotator {

namespace {
// Table creation should be pegged to a specific version number, enforcing
// linear migration-only updates.

constexpr char kContentAnnotationsTableVersion1CreationSql[] =
    R"SQL(
  CREATE TABLE content_annotations (
    visit_id INTEGER PRIMARY KEY NOT NULL,
    url TEXT NOT NULL,
    navigation_timestamp INTEGER NOT NULL,
    proto_data BLOB NOT NULL,
    tab_id INTEGER NOT NULL,
    page_title TEXT NOT NULL,
    classifier_results TEXT NOT NULL
  )
  )SQL";

std::optional<ContentAnnotationsData> ToContentAnnotationsData(
    sql::Statement& statement,
    const os_crypt_async::Encryptor* encryptor) {
  ContentAnnotationsData data;
  data.url = GURL(statement.ColumnStringView(1));
  data.navigation_timestamp = statement.ColumnTime(2);

  // Retrieve, decrypt and parse the proto
  std::string serialized_proto = statement.ColumnBlobAsString(3);
  std::string decrypted_proto;
  if (!encryptor->DecryptString(serialized_proto, &decrypted_proto) ||
      !data.content_annotation.ParseFromString(decrypted_proto)) {
    return std::nullopt;
  }

  // Retrieve the tab ID, if it's valid.
  int tab_id = statement.ColumnInt(4);
  if (tab_id != -1) {
    data.tab_id = tab_id;
  }

  data.page_title = statement.ColumnString(5);

  // Parse the JSON string into a dictionary of classifier results.
  std::optional<base::DictValue> dict = base::JSONReader::ReadDict(
      statement.ColumnStringView(6), base::JSON_PARSE_RFC);
  if (!dict) {
    return std::nullopt;
  }
  data.classifier_results = std::move(*dict);

  return data;
}
}  // namespace

ContentAnnotationsTable::ContentAnnotationsTable() = default;
ContentAnnotationsTable::~ContentAnnotationsTable() = default;

bool ContentAnnotationsTable::Init(sql::Database* db,
                                   const os_crypt_async::Encryptor* encryptor) {
  if (!db || !encryptor) {
    return false;
  }
  db_ = db;
  encryptor_ = encryptor;
  return true;
}

bool ContentAnnotationsTable::MigrateFromCleanStateToVersion1() {
  if (!db_) {
    return false;
  }

  if (!db_->Execute(kContentAnnotationsTableVersion1CreationSql)) {
    return false;
  }
  return true;
}

bool ContentAnnotationsTable::AddContentAnnotation(
    history::VisitID visit_id,
    const ContentAnnotationsData& data) {
  if (!db_ || !encryptor_) {
    return false;
  }

  // Serialize and encrypt the proto.
  std::string serialized, encrypted_proto;
  if (!data.content_annotation.SerializeToString(&serialized) ||
      !encryptor_->EncryptString(serialized, &encrypted_proto)) {
    return false;
  }

  // Serialize the classifier results to a JSON string.
  std::string classifier_results_json;
  if (!base::JSONWriter::Write(data.classifier_results,
                               &classifier_results_json)) {
    return false;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "INSERT OR REPLACE INTO content_annotations "
      "(visit_id, url, navigation_timestamp, proto_data, "
      "tab_id, page_title, classifier_results) "
      "VALUES(?, ?, ?, ?, ?, ?, ?)"));

  statement.BindInt64(0, visit_id);
  statement.BindString(1, database_utils::GurlToDatabaseUrl(data.url));
  statement.BindTime(2, data.navigation_timestamp);
  statement.BindBlob(3, encrypted_proto);
  statement.BindInt(4, data.tab_id.value_or(-1));
  statement.BindString(5, data.page_title);
  statement.BindString(6, classifier_results_json);

  return statement.Run();
}

std::optional<ContentAnnotationsData>
ContentAnnotationsTable::GetContentAnnotation(history::VisitID visit_id) {
  if (!db_ || !encryptor_) {
    return std::nullopt;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "visit_id, url, navigation_timestamp, proto_data, tab_id, "
      "page_title, classifier_results "
      "FROM content_annotations "
      "WHERE visit_id = ?"));
  statement.BindInt64(0, visit_id);

  if (!statement.Step()) {
    return std::nullopt;
  }

  return ToContentAnnotationsData(statement, encryptor_);
}

std::vector<std::pair<history::VisitID, ContentAnnotationsData>>
ContentAnnotationsTable::GetAllContentAnnotations() {
  std::vector<std::pair<history::VisitID, ContentAnnotationsData>> results;
  if (!db_ || !encryptor_) {
    return results;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE,
      "SELECT "
      "visit_id, url, navigation_timestamp, proto_data, "
      "tab_id, page_title, classifier_results "
      "FROM content_annotations "
      "ORDER BY visit_id"));

  while (statement.Step()) {
    history::VisitID visit_id = statement.ColumnInt64(0);
    std::optional<ContentAnnotationsData> data =
        ToContentAnnotationsData(statement, encryptor_);
    if (!data.has_value()) {
      // TODO(crbug.com/503879910): Emit a metric here for failed parsing.
      continue;
    }
    results.emplace_back(visit_id, std::move(data.value()));
  }

  return results;
}

bool ContentAnnotationsTable::DeleteContentAnnotations(
    base::span<const history::VisitID> visit_ids) {
  if (!db_ || !encryptor_) {
    return false;
  }

  if (visit_ids.empty()) {
    return true;
  }

  sql::Transaction transaction(db_);
  if (!transaction.Begin()) {
    return false;
  }

  sql::Statement statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM content_annotations WHERE visit_id = ?"));

  for (history::VisitID visit_id : visit_ids) {
    statement.Reset(true);
    statement.BindInt64(0, visit_id);
    if (!statement.Run()) {
      return false;
    }
  }

  return transaction.Commit();
}

bool ContentAnnotationsTable::ClearAllContentAnnotations() {
  if (!db_ || !encryptor_) {
    return false;
  }

  return db_->Execute("DELETE FROM content_annotations");
}

}  // namespace accessibility_annotator
