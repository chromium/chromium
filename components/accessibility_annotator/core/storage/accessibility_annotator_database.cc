// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace accessibility_annotator {

AccessibilityAnnotatorDatabase::AccessibilityAnnotatorDatabase() = default;

AccessibilityAnnotatorDatabase::~AccessibilityAnnotatorDatabase() = default;

bool AccessibilityAnnotatorDatabase::Init(const base::FilePath& db_path) {
  db_ = std::make_unique<sql::Database>(
      sql::Database::Tag("AccessibilityAnnotator"));

  db_->set_error_callback(
      base::BindRepeating(&AccessibilityAnnotatorDatabase::OnDatabaseError,
                          base::Unretained(this)));

  if (!db_->Open(db_path)) {
    return false;
  }

  // TODO(crbug.com/484049558): Handle database versioning.
  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  static constexpr char kCreateServerEntitiesSql[] =
      "CREATE TABLE IF NOT EXISTS entities ("
      "id TEXT PRIMARY KEY NOT NULL)";
  if (!db_->Execute(kCreateServerEntitiesSql)) {
    return false;
  }

  return transaction.Commit();
}

void AccessibilityAnnotatorDatabase::OnDatabaseError(int extended_error,
                                                     sql::Statement* stmt) {
  // Attempt to recover a corrupt database, if it is eligible to be recovered.
  if (sql::Recovery::RecoverIfPossible(
          db_.get(), extended_error, sql::Recovery::Strategy::kRecoverOrRaze)) {
    // Recovery was attempted. The database handle has been poisoned and the
    // error callback has been reset.

    // Signal the test-expectation framework that the error was handled.
    std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
    return;
  }

  DVLOG(1) << db_->GetErrorMessage();
}

}  // namespace accessibility_annotator
