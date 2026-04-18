// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_CONTENT_ANNOTATIONS_TABLE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_CONTENT_ANNOTATIONS_TABLE_H_

#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/history/core/browser/history_types.h"

namespace os_crypt_async {
class Encryptor;
}  // namespace os_crypt_async

namespace sql {
class Database;
}  // namespace sql

namespace accessibility_annotator {

// TODO(crbug.com/501429617): Remove this alias once ContentAnnotationsData is
// refactored out of the backend.
using ContentAnnotationsData =
    AccessibilityAnnotatorBackend::ContentAnnotationsData;

// This class manages the table storing content annotations for visits to URLs
// It expects the following schema:
//
// -----------------------------------------------------------------------------
// content_annotations
//
//   visit_id                           INTEGER PRIMARY KEY NOT NULL Generated
//                                      by Chrome History, uniquely
//                                      identifies a visit to a URL.
//   url                                TEXT NOT NULL The URL of the page.
//   navigation_timestamp               INTEGER NOT NULL The timestamp of the
//                                      page visit, stored as microseconds since
//                                      the Windows epoch.
//   proto_data                         BLOB NOT NULL containing the serialized
//                                      and encrypted ContentAnnotation proto.
//   tab_id                             INTEGER NOT NULL Uniquely identifies a
//                                      tab within its session.
//   page_title                         TEXT NOT NULL The page title.
//   classifier_results                 TEXT NOT NULL JSON-serialized dictionary
//                                      of classifier results (e.g., an object
//                                      of key-value pairs where the classifier
//                                      name maps to its resulting category).
// -----------------------------------------------------------------------------
class ContentAnnotationsTable {
 public:
  ContentAnnotationsTable();
  ContentAnnotationsTable(const ContentAnnotationsTable&) = delete;
  ContentAnnotationsTable& operator=(const ContentAnnotationsTable&) = delete;
  ~ContentAnnotationsTable();

  // Initializes the table with the given SQLite database. Returns true on
  // success. Must be called before any other methods.
  bool Init(sql::Database* db, const os_crypt_async::Encryptor* encryptor);

  // Creates the tables if they do not exist. Returns true on success. Must be
  // called after `Init()`.
  bool CreateTablesIfNecessary();

  // Inserts or replaces `data` in content_annotations table. Returns true on
  // success.
  bool AddContentAnnotation(history::VisitID visit_id,
                            const ContentAnnotationsData& data);

  // Retrieves a record from content_annotations table by visit_id. Returns
  // std::nullopt if not found.
  std::optional<ContentAnnotationsData> GetContentAnnotation(
      history::VisitID visit_id);

  // Retrieves all records from content_annotations table.
  std::vector<std::pair<history::VisitID, ContentAnnotationsData>>
  GetAllContentAnnotations();

  // Deletes a record from content_annotations table by visit_id. Returns true
  // on success.
  bool DeleteContentAnnotation(history::VisitID visit_id);

  // Clears all records from content_annotations table. Returns true on success.
  bool ClearAllContentAnnotations();

 private:
  // Owned by the `AccessibilityAnnotatorDatabase`.  Outlives `this`.
  raw_ptr<sql::Database> db_ = nullptr;
  raw_ptr<const os_crypt_async::Encryptor> encryptor_ = nullptr;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_CONTENT_ANNOTATIONS_TABLE_H_
