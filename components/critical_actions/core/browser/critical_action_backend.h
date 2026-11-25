// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_
#define COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/files/file_path.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

namespace critical_actions {

class CriticalActionDatabase;

// Internal implementation of Critical Action History which does the work on the
// database. This runs on a background sequenced task runner (to avoid blocking
// the main thread during database operations) and is NOT thread-safe, so it
// must only be called on that background sequence.
//
// Most functions here correspond to the public asynchronous APIs exposed by the
// service layer.
//
// The underlying database is opened lazily, on the first operation that needs
// it, so that constructing the backend (which happens during profile startup)
// does no disk IO:
// - `AddCriticalAction()` opens the database, creating it on disk if needed.
// - All other operations only read or modify existing records, so they open
//   the database only if it already exists on disk. Otherwise there is nothing
//   to read or delete, and they return early without creating an empty file.
class CriticalActionBackend {
 public:
  explicit CriticalActionBackend(const base::FilePath& db_path);
  CriticalActionBackend(const CriticalActionBackend&) = delete;
  CriticalActionBackend& operator=(const CriticalActionBackend&) = delete;
  ~CriticalActionBackend();

  // Inserts a new critical action record.
  void AddCriticalAction(const CriticalActionEntry& entry);

  // Associates conversation_id with all critical actions matching
  // critical_action_ids.
  void SetCriticalActionsConversationId(
      const std::vector<std::string>& critical_action_ids,
      std::string_view conversation_id);

  // Retrieves a critical action record by its client UUID.
  std::optional<CriticalActionEntry> GetCriticalAction(
      std::string_view critical_action_id);

  // Retrieves critical action records matching the given `options`.
  std::vector<CriticalActionEntry> GetCriticalActions(
      const CriticalActionQueryOptions& options);

  // Deletes a single critical action record.
  void DeleteCriticalAction(std::string_view critical_action_id);

  // Deletes all critical action records within the given time range,
  // inclusive of start_time and exclusive of end_time.
  void DeleteCriticalActionsInTimeRange(base::Time start_time,
                                        base::Time end_time);

  // Deletes all critical action records associated with the given visit IDs.
  void DeleteCriticalActionsByVisitIds(const std::vector<int64_t>& visit_ids);

  // Overrides the per-attempt busy lock wait timeout used when opening the
  // database in tests.
  void SetBusyTimeoutForTesting(base::TimeDelta busy_timeout);

 private:
  // Whether an operation may create the database file if it does not exist.
  enum class CreateIfMissing { kNo, kYes };

  void TryInit(bool advance_backoff);

  // Returns true if `db_` is ready to use. On the first call that is allowed
  // to open the database (see `CreateIfMissing`), opens it; afterwards, retries
  // opening it while a busy lock retry is pending.
  bool EnsureInitialized(CreateIfMissing create_if_missing);

  const base::FilePath db_path_;
  base::TimeDelta busy_timeout_ GUARDED_BY_CONTEXT(sequence_checker_);
  std::unique_ptr<CriticalActionDatabase> db_
      GUARDED_BY_CONTEXT(sequence_checker_);
  // Whether opening the database has been attempted. Once set, opening is only
  // retried according to the busy lock backoff in `TryInit()`, so a permanent
  // failure is not retried on every operation.
  bool init_attempted_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  // Whether an operation that may not create the database found no database
  // file. Only this backend creates the file, so it cannot appear until a
  // write opens (and creates) the database. Caching this avoids checking the
  // filesystem on every read and History deletion (e.g. routine expiry).
  bool db_file_missing_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  int init_retry_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  bool early_retry_used_ GUARDED_BY_CONTEXT(sequence_checker_) = false;
  base::TimeTicks last_busy_failure_ticks_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::OneShotTimer init_retry_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace critical_actions

#endif  // COMPONENTS_CRITICAL_ACTIONS_CORE_BROWSER_CRITICAL_ACTION_BACKEND_H_
