// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/content_annotations_table.h"

#include "base/notimplemented.h"
#include "sql/database.h"

namespace accessibility_annotator {

namespace {
constexpr char kContentAnnotationsTableCreationSql[] =
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
constexpr char kContentAnnotationsTableName[] = "content_annotations";
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

bool ContentAnnotationsTable::CreateTablesIfNecessary() {
  if (!db_) {
    return false;
  }

  if (!db_->DoesTableExist(kContentAnnotationsTableName)) {
    if (!db_->Execute(kContentAnnotationsTableCreationSql)) {
      return false;
    }
  }
  return true;
}

bool ContentAnnotationsTable::AddContentAnnotation(
    history::VisitID visit_id,
    const ContentAnnotationsData& data) {
  NOTIMPLEMENTED();
  return false;
}

std::optional<ContentAnnotationsData>
ContentAnnotationsTable::GetContentAnnotation(history::VisitID visit_id) {
  NOTIMPLEMENTED();
  return std::nullopt;
}

std::vector<ContentAnnotationsData>
ContentAnnotationsTable::GetAllContentAnnotations() {
  NOTIMPLEMENTED();
  return {};
}

bool ContentAnnotationsTable::DeleteContentAnnotation(
    history::VisitID visit_id) {
  NOTIMPLEMENTED();
  return false;
}

bool ContentAnnotationsTable::ClearAllContentAnnotations() {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace accessibility_annotator
