// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/storage/accessibility_annotator_database.h"

#include <memory>
#include <utility>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "sql/database.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace accessibility_annotator {

AccessibilityAnnotatorDatabase::AccessibilityAnnotatorDatabase() = default;

AccessibilityAnnotatorDatabase::~AccessibilityAnnotatorDatabase() = default;

bool AccessibilityAnnotatorDatabase::Init(const base::FilePath& db_path) {
  // Use a write-ahead log rather than a rollback journal to reduce the average
  // cost of writes, especially beneficial if writes are frequent.
  // TODO(crbug.com/484049558): Finalize the decision on WAL mode based on
  // expected usage patterns.
  // Open the database file with exclusive read/write access on Windows.
  // This prevents other processes from opening them concurrently.
  // This may cause an increase in kBusy failures on open.
  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions()
#if BUILDFLAG(IS_WIN)
          .set_exclusive_database_file_lock(true)
#endif  // IS_WIN
          .set_wal_mode(true),
      sql::Database::Tag("AccessibilityAnnotator"));

  // TODO(crbug.com/489690454): Update this with metrics for various failure
  // cases in this function.

  // Capture any errors that occur during database open.
  int captured_error = 0;
  db_->set_error_callback(
      base::BindRepeating([](int* out_error, int error,
                             sql::Statement* stmt) { *out_error = error; },
                          &captured_error));

  bool open_success = db_->Open(db_path);
  db_->reset_error_callback();
  if (!open_success) {
    // If the error is kNotADatabase, attempt to delete the file and re-open
    // the database.
    if (captured_error ==
        std::to_underlying(sql::SqliteResultCode::kNotADatabase)) {
      if (!db_->CloseAndDelete()) {
        return false;
      }
      if (!db_->Open(db_path)) {
        return false;
      }
      // Fallthrough to the default initialization path if open succeeds.
    } else {
      return false;
    }
  }

  // Check the user-version (https://sqlite.org/pragma.html#pragma_user_version)
  // to see if there has been a schema change since the last time this database
  // was modified.
  int detected_user_version;
  if (sql::Statement get_user_version_stm(
          db_->GetUniqueStatement("PRAGMA user_version"));
      get_user_version_stm.is_valid() && get_user_version_stm.Step()) {
    detected_user_version = get_user_version_stm.ColumnInt(0);
  } else {
    return false;
  }

  if (detected_user_version == kCurrentVersionNumber) {
    return true;
  }

  // If the database is newer than the current version, leave it unmodified.
  // This allows the data to be used again once the browser is updated.
  if (detected_user_version > kCurrentVersionNumber) {
    return false;
  }

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin()) {
    return false;
  }

  if (!CreateTablesIfNecessary()) {
    return false;
  }

  // Perform any necessary migrations here. The updated user-version will be set
  // in the migration function.
  if (!MigrateOldVersionsAsNeeded(detected_user_version)) {
    return false;
  }

  if (!transaction.Commit()) {
    return false;
  }

  return true;
}

bool AccessibilityAnnotatorDatabase::CreateTablesIfNecessary() {
  // TODO(crbug.com/483214801): Replace this table once some actual tables are
  // finalized.
  static constexpr char kCreateServerEntitiesSql[] =
      "CREATE TABLE IF NOT EXISTS entities ("
      "id TEXT PRIMARY KEY NOT NULL)";
  return db_->Execute(kCreateServerEntitiesSql);
}

bool AccessibilityAnnotatorDatabase::MigrateOldVersionsAsNeeded(
    int detected_user_version) {
  // Perform any necessary migrations from `detected_user_version` to
  // `kCurrentVersionNumber` here.

  // Set the user-version to the current version.
  return db_->Execute(base::StrCat(
      {"PRAGMA user_version=", base::NumberToString(kCurrentVersionNumber)}));
}

}  // namespace accessibility_annotator
