// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/stale_entry_finalizer_task.h"

#include <array>

#include "base/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/offline_pages/core/offline_clock.h"
#include "components/offline_pages/core/offline_store_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_dispatcher.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace offline_pages {

using Result = StaleEntryFinalizerTask::Result;

namespace {

// Maximum amount of time into the future an item can has its freshness time set
// to after which it will be finalized (or deleted if in the zombie state).
constexpr base::TimeDelta kFutureItemTimeLimit = base::TimeDelta::FromDays(1);

// Expiration time delay for items entering the zombie state, after which they
// are permanently deleted.
constexpr base::TimeDelta kZombieItemLifetime = base::TimeDelta::FromDays(7);

// If this time changes, we need to update the desciption in histograms.xml
// for OfflinePages.Prefetching.StuckItemState.
const int kStuckTimeLimitInDays = 7;

const base::TimeDelta FreshnessPeriodForState(PrefetchItemState state) {
  switch (state) {
    // Bucket 1.
    case PrefetchItemState::NEW_REQUEST:
      return base::TimeDelta::FromDays(1);
    // Bucket 2.
    case PrefetchItemState::AWAITING_GCM:
    case PrefetchItemState::RECEIVED_GCM:
    case PrefetchItemState::RECEIVED_BUNDLE:
      return base::TimeDelta::FromDays(1);
    // Bucket 3.
    case PrefetchItemState::DOWNLOADING:
    case PrefetchItemState::IMPORTING:
      return kPrefetchDownloadLifetime;
    // The following states do not expire based on per bucket freshness so they
    // are not expected to be passed into this function.
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
    case PrefetchItemState::SENT_GET_OPERATION:
    case PrefetchItemState::DOWNLOADED:
    case PrefetchItemState::FINISHED:
    case PrefetchItemState::ZOMBIE:
      NOTREACHED();
  }
  return base::TimeDelta::FromDays(1);
}

PrefetchItemErrorCode ErrorCodeForState(PrefetchItemState state) {
  switch (state) {
    // Valid values.
    case PrefetchItemState::NEW_REQUEST:
      return PrefetchItemErrorCode::STALE_AT_NEW_REQUEST;
    case PrefetchItemState::AWAITING_GCM:
      return PrefetchItemErrorCode::STALE_AT_AWAITING_GCM;
    case PrefetchItemState::RECEIVED_GCM:
      return PrefetchItemErrorCode::STALE_AT_RECEIVED_GCM;
    case PrefetchItemState::RECEIVED_BUNDLE:
      return PrefetchItemErrorCode::STALE_AT_RECEIVED_BUNDLE;
    case PrefetchItemState::DOWNLOADING:
      return PrefetchItemErrorCode::STALE_AT_DOWNLOADING;
    case PrefetchItemState::IMPORTING:
      return PrefetchItemErrorCode::STALE_AT_IMPORTING;
    // The following states do not expire based on per bucket freshness so they
    // are not expected to be passed into this function.
    case PrefetchItemState::SENT_GENERATE_PAGE_BUNDLE:
    case PrefetchItemState::SENT_GET_OPERATION:
    case PrefetchItemState::DOWNLOADED:
    case PrefetchItemState::FINISHED:
    case PrefetchItemState::ZOMBIE:
      NOTREACHED();
  }
  return PrefetchItemErrorCode::STALE_AT_UNKNOWN;
}

bool FinalizeStaleItems(PrefetchItemState state,
                        base::Time now,
                        sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE state = ? AND freshness_time < ?";
  const int64_t earliest_fresh_db_time =
      store_utils::ToDatabaseTime(now - FreshnessPeriodForState(state));
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(1, static_cast<int>(ErrorCodeForState(state)));
  statement.BindInt(2, static_cast<int>(state));
  statement.BindInt64(3, earliest_fresh_db_time);

  return statement.Run();
}

bool MoreWorkInQueue(sql::Database* db) {
  static const char kSql[] =
      "SELECT COUNT(*) FROM prefetch_items"
      " WHERE state NOT IN (?, ?)";
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::ZOMBIE));
  statement.BindInt(1, static_cast<int>(PrefetchItemState::AWAITING_GCM));

  // In event of failure, assume more work exists.
  if (!statement.Step())
    return true;

  return statement.ColumnInt(0) > 0;
}

// If the user shifted the clock backwards too far, our items will stay around
// for a very long time.  Don't allow that so we don't waste resources with
// potentially outdated content.
bool FinalizeFutureItems(PrefetchItemState state,
                         base::Time now,
                         sql::Database* db) {
  static const char kSql[] =
      "UPDATE prefetch_items SET state = ?, error_code = ?"
      " WHERE state = ? AND freshness_time > ?";
  const int64_t future_fresh_db_time_limit =
      store_utils::ToDatabaseTime(now + kFutureItemTimeLimit);
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::FINISHED));
  statement.BindInt(
      1, static_cast<int>(
             PrefetchItemErrorCode::MAXIMUM_CLOCK_BACKWARD_SKEW_EXCEEDED));
  statement.BindInt(2, static_cast<int>(state));
  statement.BindInt64(3, future_fresh_db_time_limit);

  return statement.Run();
}

bool DeleteExpiredAndFutureZombies(base::Time now, sql::Database* db) {
  static const char kSql[] =
      "DELETE FROM prefetch_items"
      " WHERE state = ? "
      " AND (freshness_time < ? OR freshness_time > ?)";
  const int64_t earliest_zombie_db_time =
      store_utils::ToDatabaseTime(now - kZombieItemLifetime);
  const int64_t future_zombie_db_time =
      store_utils::ToDatabaseTime(now + kFutureItemTimeLimit);
  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  statement.BindInt(0, static_cast<int>(PrefetchItemState::ZOMBIE));
  statement.BindInt64(1, earliest_zombie_db_time);
  statement.BindInt64(2, future_zombie_db_time);
  return statement.Run();
}

// If there is a bug in our code, an item might be stuck in the queue waiting
// on an event that didn't happen.  If so, finalize that item and report it.
void ReportAndFinalizeStuckItems(base::Time now, sql::Database* db) {
  const int64_t earliest_valid_creation_time = store_utils::ToDatabaseTime(
      now - base::TimeDelta::FromDays(kStuckTimeLimitInDays));
  // Report.
  {
    static constexpr char kSql[] =
        "SELECT state FROM prefetch_items"
        " WHERE creation_time < ?"
        " AND state NOT IN (?, ?)";  // (ZOMBIE, FINISHED);
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(0, earliest_valid_creation_time);
    statement.BindInt64(1, static_cast<int>(PrefetchItemState::FINISHED));
    statement.BindInt64(2, static_cast<int>(PrefetchItemState::ZOMBIE));

    while (statement.Step()) {
      int state_int = statement.ColumnInt(0);
      if (ToPrefetchItemState(state_int)) {  // Only report valid enum values.
        base::UmaHistogramSparse("OfflinePages.Prefetching.StuckItemState",
                                 state_int);
      }
    }
  }
  // Finalize.
  {
    static constexpr char kSql[] =
        "UPDATE prefetch_items SET state = ?, error_code = ?"
        " WHERE creation_time < ?"
        " AND state NOT IN (?, ?)";  // (ZOMBIE, FINISHED)
    sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
    statement.BindInt64(0, static_cast<int>(PrefetchItemState::FINISHED));
    statement.BindInt64(1, static_cast<int>(PrefetchItemErrorCode::STUCK));
    statement.BindInt64(2, earliest_valid_creation_time);
    statement.BindInt64(3, static_cast<int>(PrefetchItemState::FINISHED));
    statement.BindInt64(4, static_cast<int>(PrefetchItemState::ZOMBIE));

    statement.Run();
  }
}

Result FinalizeStaleEntriesSync(sql::Database* db) {
  sql::Transaction transaction(db);
  if (!transaction.Begin())
    return Result::NO_MORE_WORK;

  // Only the following states are supposed to expire based on per bucket
  // freshness.
  static constexpr std::array<PrefetchItemState, 6> expirable_states = {{
      // Bucket 1.
      PrefetchItemState::NEW_REQUEST,
      // Bucket 2.
      PrefetchItemState::AWAITING_GCM, PrefetchItemState::RECEIVED_GCM,
      PrefetchItemState::RECEIVED_BUNDLE,
      // Bucket 3.
      PrefetchItemState::DOWNLOADING, PrefetchItemState::IMPORTING,
  }};
  base::Time now = OfflineTimeNow();
  for (PrefetchItemState state : expirable_states) {
    if (!FinalizeStaleItems(state, now, db))
      return Result::NO_MORE_WORK;

    if (!FinalizeFutureItems(state, now, db))
      return Result::NO_MORE_WORK;
  }

  if (!DeleteExpiredAndFutureZombies(now, db))
    return Result::NO_MORE_WORK;

  // Items could also be stuck in a non-expirable state due to a bug, report
  // them. This should always be the last step, coming after the regular
  // freshness maintenance steps above are done.
  ReportAndFinalizeStuckItems(now, db);

  Result result = Result::MORE_WORK_NEEDED;
  if (!MoreWorkInQueue(db))
    result = Result::NO_MORE_WORK;

  // If all FinalizeStaleItems calls succeeded the transaction is committed.
  return transaction.Commit() ? result : Result::NO_MORE_WORK;
}

}  // namespace

StaleEntryFinalizerTask::StaleEntryFinalizerTask(
    PrefetchDispatcher* prefetch_dispatcher,
    PrefetchStore* prefetch_store)
    : prefetch_dispatcher_(prefetch_dispatcher),
      prefetch_store_(prefetch_store) {
  DCHECK(prefetch_dispatcher_);
  DCHECK(prefetch_store_);
}

StaleEntryFinalizerTask::~StaleEntryFinalizerTask() {}

void StaleEntryFinalizerTask::Run() {
  prefetch_store_->Execute(base::BindOnce(&FinalizeStaleEntriesSync),
                           base::BindOnce(&StaleEntryFinalizerTask::OnFinished,
                                          weak_ptr_factory_.GetWeakPtr()),
                           Result::NO_MORE_WORK);
}

void StaleEntryFinalizerTask::OnFinished(Result result) {
  final_status_ = result;
  if (final_status_ == Result::MORE_WORK_NEEDED)
    prefetch_dispatcher_->EnsureTaskScheduled();
  DVLOG(1) << "Finalization task "
           << (result == Result::NO_MORE_WORK ? "not " : "")
           << "scheduling background processing.";
  TaskComplete();
}

}  // namespace offline_pages
