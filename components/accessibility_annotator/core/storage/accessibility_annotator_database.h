// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
#define COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_

#include <memory>

#include "base/files/file_path.h"

namespace sql {
class Database;
class Statement;
}  // namespace sql

namespace accessibility_annotator {

// The database manager for the AccessibilityAnnotator.
// TODO(crbug.com/484049558): Add comment representing overall table structures.
class AccessibilityAnnotatorDatabase {
 public:
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
  // The error callback for the database.
  void OnDatabaseError(int extended_error, sql::Statement* stmt);

  std::unique_ptr<sql::Database> db_;
};

}  // namespace accessibility_annotator

#endif  // COMPONENTS_ACCESSIBILITY_ANNOTATOR_CORE_STORAGE_ACCESSIBILITY_ANNOTATOR_DATABASE_H_
