// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_

#include <memory>

#include "base/files/file_path.h"

namespace sql {
class Database;
}  // namespace sql

namespace accessibility_annotator {

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
  bool Init(const base::FilePath& db_path);

 private:
  // Creates the tables if they don't exist. Returns true on success.
  bool CreateTablesIfNecessary();

  // Migrates the database from the detected version to the current version.
  // Returns true on success, false otherwise.
  bool MigrateOldVersionsAsNeeded(int detected_user_version);

  std::unique_ptr<sql::Database> db_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
