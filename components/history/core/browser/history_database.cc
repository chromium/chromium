// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/url_utils.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include "base/mac/mac_util.h"
#endif

namespace history {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 42;
const int kCompatibleVersionNumber = 16;
const char kEarlyExpirationThresholdKey[] = "early_expiration_threshold";

// Logs a migration failure to UMA and logging. The return value will be
// what to return from ::Init (to simplify the call sites). Migration failures
// are almost always fatal since the database can be in an inconsistent state.
sql::InitStatus LogMigrationFailure(int from_version) {
  base::UmaHistogramSparse("History.MigrateFailureFromVersion", from_version);
  LOG(ERROR) << "History failed to migrate from version " << from_version
             << ". History will be disabled.";
  return sql::INIT_FAILURE;
}

// Reasons for initialization to fail. These are logged to UMA. It corresponds
// to the HistoryInitStep enum in enums.xml.
//
// DO NOT CHANGE THE VALUES. Leave holes if anything is removed and add only
// to the end.
enum class InitStep {
  OPEN = 0,
  TRANSACTION_BEGIN = 1,
  META_TABLE_INIT = 2,
  CREATE_TABLES = 3,
  VERSION = 4,
  COMMIT = 5,
};

sql::InitStatus LogInitFailure(InitStep what) {
  base::UmaHistogramSparse("History.InitializationFailureStep",
                           static_cast<int>(what));
  return sql::INIT_FAILURE;
}

}  // namespace

HistoryDatabase::HistoryDatabase(
    DownloadInterruptReason download_interrupt_reason_none,
    DownloadInterruptReason download_interrupt_reason_crash)
    : DownloadDatabase(download_interrupt_reason_none,
                       download_interrupt_reason_crash) {
}

HistoryDatabase::~HistoryDatabase() {
}

sql::InitStatus HistoryDatabase::Init(const base::FilePath& history_name) {
  db_.set_histogram_tag("History");

  // Set the database page size to something a little larger to give us
  // better performance (we're typically seek rather than bandwidth limited).
  // This only has an effect before any tables have been created, otherwise
  // this is a NOP. Must be a power of 2 and a max of 8192.
  db_.set_page_size(4096);

  // Set the cache size. The page size, plus a little extra, times this
  // value, tells us how much memory the cache will use maximum.
  // 1000 * 4kB = 4MB
  // TODO(brettw) scale this value to the amount of available memory.
  db_.set_cache_size(1000);

  // Note that we don't set exclusive locking here. That's done by
  // BeginExclusiveMode below which is called later (we have to be in shared
  // mode to start out for the in-memory backend to read the data).

  if (!db_.Open(history_name))
    return LogInitFailure(InitStep::OPEN);

  // Wrap the rest of init in a tranaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return LogInitFailure(InitStep::TRANSACTION_BEGIN);

#if defined(OS_MACOSX) && !defined(OS_IOS)
  // Exclude the history file from backups.
  base::mac::SetFileBackupExclusion(history_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices.
  // NOTE: If you add something here, also add it to
  //       RecreateAllButStarAndURLTables.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return LogInitFailure(InitStep::META_TABLE_INIT);
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable() || !InitDownloadTable() ||
      !InitSegmentTables() || !InitSyncTable())
    return LogInitFailure(InitStep::CREATE_TABLES);
  CreateMainURLIndex();

  // TODO(benjhayden) Remove at some point.
  meta_table_.DeleteKey("next_download_id");

  // Version check.
  sql::InitStatus version_status = EnsureCurrentVersion();
  if (version_status != sql::INIT_OK) {
    LogInitFailure(InitStep::VERSION);
    return version_status;
  }

  if (!committer.Commit())
    return LogInitFailure(InitStep::COMMIT);
  return sql::INIT_OK;
}

void HistoryDatabase::ComputeDatabaseMetrics(
    const base::FilePath& history_name) {
  base::TimeTicks start_time = base::TimeTicks::Now();
  int64_t file_size = 0;
  if (!base::GetFileSize(history_name, &file_size))
    return;
  int file_mb = static_cast<int>(file_size / (1024 * 1024));
  UMA_HISTOGRAM_MEMORY_MB("History.DatabaseFileMB", file_mb);

  sql::Statement url_count(db_.GetUniqueStatement("SELECT count(*) FROM urls"));
  if (!url_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS_1M("History.URLTableCount", url_count.ColumnInt(0));

  sql::Statement visit_count(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits"));
  if (!visit_count.Step())
    return;
  UMA_HISTOGRAM_COUNTS_1M("History.VisitTableCount", visit_count.ColumnInt(0));

  base::Time one_week_ago = base::Time::Now() - base::TimeDelta::FromDays(7);
  sql::Statement weekly_visit_sql(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits WHERE visit_time > ?"));
  weekly_visit_sql.BindInt64(0, one_week_ago.ToInternalValue());
  int weekly_visit_count = 0;
  if (weekly_visit_sql.Step())
    weekly_visit_count = weekly_visit_sql.ColumnInt(0);
  UMA_HISTOGRAM_COUNTS_1M("History.WeeklyVisitCount", weekly_visit_count);

  base::Time one_month_ago = base::Time::Now() - base::TimeDelta::FromDays(30);
  sql::Statement monthly_visit_sql(db_.GetUniqueStatement(
      "SELECT count(*) FROM visits WHERE visit_time > ? AND visit_time <= ?"));
  monthly_visit_sql.BindInt64(0, one_month_ago.ToInternalValue());
  monthly_visit_sql.BindInt64(1, one_week_ago.ToInternalValue());
  int older_visit_count = 0;
  if (monthly_visit_sql.Step())
    older_visit_count = monthly_visit_sql.ColumnInt(0);
  UMA_HISTOGRAM_COUNTS_1M("History.MonthlyVisitCount",
                          older_visit_count + weekly_visit_count);

  UMA_HISTOGRAM_TIMES("History.DatabaseBasicMetricsTime",
                      base::TimeTicks::Now() - start_time);

  // Compute the advanced metrics even less often, pending timing data showing
  // that's not necessary.
  if (base::RandInt(1, 3) == 3) {
    start_time = base::TimeTicks::Now();

    // Collect all URLs visited within the last month.
    sql::Statement url_sql(db_.GetUniqueStatement(
        "SELECT url, last_visit_time FROM urls WHERE last_visit_time > ?"));
    url_sql.BindInt64(0, one_month_ago.ToInternalValue());

    // Count URLs (which will always be unique) and unique hosts within the last
    // week and last month.
    int week_url_count = 0;
    int month_url_count = 0;
    std::set<std::string> week_hosts;
    std::set<std::string> month_hosts;
    while (url_sql.Step()) {
      GURL url(url_sql.ColumnString(0));
      base::Time visit_time =
          base::Time::FromInternalValue(url_sql.ColumnInt64(1));
      ++month_url_count;
      month_hosts.insert(url.host());
      if (visit_time > one_week_ago) {
        ++week_url_count;
        week_hosts.insert(url.host());
      }
    }
    UMA_HISTOGRAM_COUNTS_1M("History.WeeklyURLCount", week_url_count);
    UMA_HISTOGRAM_COUNTS_10000("History.WeeklyHostCount",
                               static_cast<int>(week_hosts.size()));
    UMA_HISTOGRAM_COUNTS_1M("History.MonthlyURLCount", month_url_count);
    UMA_HISTOGRAM_COUNTS_10000("History.MonthlyHostCount",
                               static_cast<int>(month_hosts.size()));
    UMA_HISTOGRAM_TIMES("History.DatabaseAdvancedMetricsTime",
                        base::TimeTicks::Now() - start_time);
  }
}

int HistoryDatabase::CountUniqueHostsVisitedLastMonth() {
  base::TimeTicks start_time = base::TimeTicks::Now();
  // Collect all URLs visited within the last month.
  base::Time one_month_ago = base::Time::Now() - base::TimeDelta::FromDays(30);

  sql::Statement url_sql(
      db_.GetUniqueStatement("SELECT url FROM urls "
                             "WHERE last_visit_time > ? "
                             "AND hidden = 0 "
                             "AND visit_count > 0"));
  url_sql.BindInt64(0, one_month_ago.ToInternalValue());

  std::set<std::string> hosts;
  while (url_sql.Step()) {
    GURL url(url_sql.ColumnString(0));
    hosts.insert(url.host());
  }

  UMA_HISTOGRAM_TIMES("History.DatabaseMonthlyHostCountTime",
                      base::TimeTicks::Now() - start_time);
  return hosts.size();
}

void HistoryDatabase::BeginExclusiveMode() {
  // We can't use set_exclusive_locking() since that only has an effect before
  // the DB is opened.
  ignore_result(db_.Execute("PRAGMA locking_mode=EXCLUSIVE"));
}

// static
int HistoryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

void HistoryDatabase::BeginTransaction() {
  db_.BeginTransaction();
}

void HistoryDatabase::CommitTransaction() {
  db_.CommitTransaction();
}

void HistoryDatabase::RollbackTransaction() {
  // If Init() returns with a failure status, the Transaction created there will
  // be destructed and rolled back. HistoryBackend might try to kill the
  // database after that, at which point it will try to roll back a non-existing
  // transaction. This will crash on a DCHECK. So transaction_nesting() is
  // checked first.
  if (db_.transaction_nesting())
    db_.RollbackTransaction();
}

bool HistoryDatabase::RecreateAllTablesButURL() {
  if (!DropVisitTable())
    return false;
  if (!InitVisitTable())
    return false;

  if (!DropKeywordSearchTermsTable())
    return false;
  if (!InitKeywordSearchTermsTable())
    return false;

  if (!DropSegmentTables())
    return false;
  if (!InitSegmentTables())
    return false;

  return true;
}

void HistoryDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting()) <<
      "Can not have a transaction when vacuuming.";
  ignore_result(db_.Execute("VACUUM"));
}

void HistoryDatabase::TrimMemory() {
  db_.TrimMemory();
}

bool HistoryDatabase::Raze() {
  return db_.Raze();
}

std::string HistoryDatabase::GetDiagnosticInfo(int extended_error,
                                               sql::Statement* statement) {
  return db_.GetDiagnosticInfo(extended_error, statement);
}

bool HistoryDatabase::SetSegmentID(VisitID visit_id, SegmentID segment_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET segment_id = ? WHERE id = ?"));
  s.BindInt64(0, segment_id);
  s.BindInt64(1, visit_id);
  DCHECK(db_.GetLastChangeCount() == 1);

  return s.Run();
}

SegmentID HistoryDatabase::GetSegmentID(VisitID visit_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "SELECT segment_id FROM visits WHERE id = ?"));
  s.BindInt64(0, visit_id);

  if (!s.Step() || s.GetColumnType(0) == sql::ColumnType::kNull)
    return 0;
  return s.ColumnInt64(0);
}

base::Time HistoryDatabase::GetEarlyExpirationThreshold() {
  if (!cached_early_expiration_threshold_.is_null())
    return cached_early_expiration_threshold_;

  int64_t threshold;
  if (!meta_table_.GetValue(kEarlyExpirationThresholdKey, &threshold)) {
    // Set to a very early non-zero time, so it's before all history, but not
    // zero to avoid re-retrieval.
    threshold = 1L;
  }

  cached_early_expiration_threshold_ = base::Time::FromInternalValue(threshold);
  return cached_early_expiration_threshold_;
}

void HistoryDatabase::UpdateEarlyExpirationThreshold(base::Time threshold) {
  meta_table_.SetValue(kEarlyExpirationThresholdKey,
                       threshold.ToInternalValue());
  cached_early_expiration_threshold_ = threshold;
}

sql::Database& HistoryDatabase::GetDB() {
  return db_;
}

sql::MetaTable& HistoryDatabase::GetMetaTable() {
  return meta_table_;
}

// Migration -------------------------------------------------------------------

sql::InitStatus HistoryDatabase::EnsureCurrentVersion() {
  // We can't read databases newer than we were designed for.
  if (meta_table_.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    LOG(WARNING) << "History database is too new.";
    return sql::INIT_TOO_NEW;
  }

  int cur_version = meta_table_.GetVersionNumber();

  // Put migration code here

  if (cur_version == 15) {
    if (!db_.Execute("DROP TABLE starred") || !DropStarredIDFromURLs())
      return LogMigrationFailure(15);
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
    meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 16) {
#if !defined(OS_WIN)
    // In this version we bring the time format on Mac & Linux in sync with the
    // Windows version so that profiles can be moved between computers.
    MigrateTimeEpoch();
#endif
    // On all platforms we bump the version number, so on Windows this
    // migration is a NOP. We keep the compatible version at 16 since things
    // will basically still work, just history will be in the future if an
    // old version reads it.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 17) {
    // Version 17 was for thumbnails to top sites migration. We ended up
    // disabling it though, so 17->18 does nothing.
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 18) {
    // This is the version prior to adding url_source column. We need to
    // migrate the database.
    cur_version = 19;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 19) {
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
    // This was the thumbnail migration.  Obsolete.
  }

  if (cur_version == 20) {
    // This is the version prior to adding the visit_duration field in visits
    // database. We need to migrate the database.
    if (!MigrateVisitsWithoutDuration())
      return LogMigrationFailure(20);
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 21) {
    // The android_urls table's data schemal was changed in version 21.
#if defined(OS_ANDROID)
    if (!MigrateToVersion22())
      return LogMigrationFailure(21);
#endif
    ++cur_version;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 22) {
    if (!MigrateDownloadsState())
      return LogMigrationFailure(22);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 23) {
    if (!MigrateDownloadsReasonPathsAndDangerType())
      return LogMigrationFailure(23);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 24) {
    if (!MigratePresentationIndex())
      return LogMigrationFailure(24);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 25) {
    if (!MigrateReferrer())
      return LogMigrationFailure(25);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 26) {
    if (!MigrateDownloadedByExtension())
      return LogMigrationFailure(26);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 27) {
    if (!MigrateDownloadValidators())
      return LogMigrationFailure(27);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 28) {
    if (!MigrateMimeType())
      return LogMigrationFailure(28);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 29) {
    if (!MigrateHashHttpMethodAndGenerateGuids())
      return LogMigrationFailure(29);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 30) {
    if (!MigrateDownloadTabUrl())
      return LogMigrationFailure(30);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 31) {
    if (!MigrateDownloadSiteInstanceUrl())
      return LogMigrationFailure(31);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 32) {
    // New download slices table is introduced, no migration needed.
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 33) {
    if (!MigrateDownloadLastAccessTime())
      return LogMigrationFailure(33);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 34) {
    // This originally contained an autoincrement migration which was abandoned
    // and added back in version 36. (see https://crbug.com/736136)
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 35) {
    if (!MigrateDownloadTransient())
      return LogMigrationFailure(35);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 36) {
    // Version 34 added AUTOINCREMENT but was reverted. Since some users will
    // have been migrated and others not, explicitly check for the AUTOINCREMENT
    // annotation rather than the version number.
    if (!URLTableContainsAutoincrement()) {
      if (!RecreateURLTableWithAllContents())
        return LogMigrationFailure(36);
    }

    DCHECK(URLTableContainsAutoincrement());
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 37) {
    if (!MigrateVisitSegmentNames())
      return LogMigrationFailure(37);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 38) {
    if (!MigrateDownloadSliceFinished())
      return LogMigrationFailure(38);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 39) {
    if (!MigrateVisitsWithoutIncrementedOmniboxTypedScore())
      return LogMigrationFailure(39);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 40) {
    std::vector<URLID> visited_url_rowids_sorted;
    if (!GetAllVisitedURLRowidsForMigrationToVersion40(
            &visited_url_rowids_sorted) ||
        !CleanTypedURLOrphanedMetadataForMigrationToVersion40(
            visited_url_rowids_sorted)) {
      return LogMigrationFailure(40);
    }
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 41) {
    if (!MigrateKeywordsSearchTermsLowerTermColumn())
      return LogMigrationFailure(41);
    cur_version++;
    meta_table_.SetVersionNumber(cur_version);
  }

  // =========================       ^^ new migration code goes here ^^
  // ADDING NEW MIGRATION CODE
  // =========================
  //
  // Add new migration code above here. It's important to use as little space
  // as possible during migration. Many phones are very near their storage
  // limit, so anything that recreates or duplicates large history tables can
  // easily push them over that limit.
  //
  // When failures happen during initialization, history is not loaded. This
  // causes all components related to the history database file to fail
  // completely, including autocomplete and downloads. Devices near their
  // storage limit are likely to fail doing some update later, but those
  // operations will then just be skipped which is not nearly as disruptive.
  // See https://crbug.com/734194.

  // When the version is too old, we just try to continue anyway, there should
  // not be a released product that makes a database too old for us to handle.
  LOG_IF(WARNING, cur_version < GetCurrentVersion()) <<
         "History database version " << cur_version << " is too old to handle.";

  return sql::INIT_OK;
}

#if !defined(OS_WIN)
void HistoryDatabase::MigrateTimeEpoch() {
  // Update all the times in the URLs and visits table in the main database.
  ignore_result(db_.Execute(
      "UPDATE urls "
      "SET last_visit_time = last_visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM urls WHERE last_visit_time > 0);"));
  ignore_result(db_.Execute(
      "UPDATE visits "
      "SET visit_time = visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM visits WHERE visit_time > 0);"));
  ignore_result(db_.Execute(
      "UPDATE segment_usage "
      "SET time_slot = time_slot + 11644473600000000 "
      "WHERE id IN (SELECT id FROM segment_usage WHERE time_slot > 0);"));
}
#endif

}  // namespace history
