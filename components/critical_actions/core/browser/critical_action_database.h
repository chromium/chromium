// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_DATABASE_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_DATABASE_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "sql/database.h"
#include "sql/meta_table.h"

namespace critical_actions {

// Encapsulates the SQL connection for the critical action history database.
// This class is not thread-safe and must be called exclusively on the sequenced
// backend thread.
class CriticalActionDatabase {
 public:
  explicit CriticalActionDatabase(const base::FilePath& db_path);
  CriticalActionDatabase(const CriticalActionDatabase&) = delete;
  CriticalActionDatabase& operator=(const CriticalActionDatabase&) = delete;
  ~CriticalActionDatabase();

  // Opens/initializes the database. Main entry point on the backend thread.
  // Performs the table and index initialization inside a transaction.
  // Returns true on success.
  bool Init();

  // Adds a new critical action record to the database.
  // Returns true if successfully added, or false if an entry with the same ID
  // already exists or a database error occurs.
  bool AddCriticalAction(const CriticalActionEntry& entry);

  // Retrieves a critical action record by its ID.
  // Returns the record if found, or std::nullopt if the action ID is not found.
  std::optional<CriticalActionEntry> GetCriticalAction(
      std::string_view critical_action_id);

  // Retrieves critical action records matching the given `options`.
  // Returns a list of matching critical action entries, sorted by timestamp
  // descending.
  std::vector<CriticalActionEntry> GetCriticalActions(
      const CriticalActionQueryOptions& options);

  // Deletes the critical action record matching `critical_action_id`.
  // Returns true if the query executed successfully (even if no matching record
  // was deleted), or false on database error.
  bool DeleteCriticalAction(std::string_view critical_action_id);

  // Deletes all critical action records within the given time range,
  // inclusive of start_time and exclusive of end_time.
  // Returns true if the query executed successfully (even if no matching record
  // was deleted), or false on database error.
  bool DeleteCriticalActionsInTimeRange(base::Time start_time,
                                        base::Time end_time);

  // Deletes all critical action records associated with the given visit IDs.
  // Returns true if the transaction executed successfully (even if no matching
  // record was deleted), or false on database error.
  bool DeleteCriticalActionsByVisitIds(const std::vector<int64_t>& visit_ids);

  // Cleanly closes the database.
  void Close();

  // Exposes underlying database for testing or verification.
  sql::Database& GetDBForTesting() { return db_; }

 private:
  // Creates tables and indices if they do not yet exist.
  // Must be called within an active transaction.
  bool InitSchema();

  // SQLite error callback.
  void DatabaseErrorCallback(int extended_error, sql::Statement* statement);

  const base::FilePath db_path_;
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  sql::MetaTable meta_table_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_DATABASE_H_
