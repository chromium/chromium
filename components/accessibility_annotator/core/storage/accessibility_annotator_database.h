// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_

#include <memory>
#include <optional>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/accessibility_annotator/core/storage/accessibility_annotator_backend.h"
#include "components/accessibility_annotator/core/storage/content_annotations_table.h"
#include "components/history/core/browser/history_types.h"
#include "components/os_crypt/async/browser/os_crypt_async.h"

namespace sql {
class Database;
}  // namespace sql

namespace accessibility_annotator {

// TODO(crbug.com/501429617): Remove this alias once ContentAnnotationsData is
// refactored out of the backend.
using ContentAnnotationsData =
    AccessibilityAnnotatorBackend::ContentAnnotationsData;

// The database manager for the AccessibilityAnnotator.
class AccessibilityAnnotatorDatabase {
 public:
  // Current version number. This value is to be incremented when the database
  // schema used by this class evolves in a non-backwards compatible way. When
  // this number changes, all existing databases will be migrated to the new
  // schema via the `MigrateOldVersionsAsNeeded` function.
  static constexpr int kCurrentVersionNumber = 1;

  AccessibilityAnnotatorDatabase();

  AccessibilityAnnotatorDatabase(const AccessibilityAnnotatorDatabase&) =
      delete;
  AccessibilityAnnotatorDatabase& operator=(
      const AccessibilityAnnotatorDatabase&) = delete;

  ~AccessibilityAnnotatorDatabase();

  // Initializes the database connection and all tables. Must be called
  // before any other methods. Returns true on success.
  bool Init(const base::FilePath& db_path, os_crypt_async::Encryptor encryptor);

  // Adds, deletes, or retrieves data in the content_annotations
  // table. See the identically named functions in `ContentAnnotationsTable`.
  bool AddContentAnnotation(history::VisitID visit_id,
                            const ContentAnnotationsData& data);
  std::optional<ContentAnnotationsData> GetContentAnnotation(
      history::VisitID visit_id);
  std::vector<std::pair<history::VisitID, ContentAnnotationsData>>
  GetAllContentAnnotations();
  bool DeleteContentAnnotation(history::VisitID visit_id);
  bool ClearAllContentAnnotations();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // Creates the tables if they don't exist. Returns true on success.
  bool CreateTablesIfNecessary();

  // Migrates the database from the detected version to the current version.
  // Returns true on success, false otherwise.
  bool MigrateOldVersionsAsNeeded(int detected_user_version);

  std::unique_ptr<sql::Database> db_;

  std::optional<os_crypt_async::Encryptor> encryptor_;

  ContentAnnotationsTable content_annotations_table_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
