// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/history_database.h"

#include <stdint.h>

#include <algorithm>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/history_types.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/statement.h"
#include "sql/transaction.h"

#if BUILDFLAG(IS_APPLE)
#include "base/apple/backup_util.h"
#endif

namespace history {

namespace {

// Current version number. We write databases at the "current" version number,
// but any previous version that can read the "compatible" one can make do with
// our database without *too* many bad effects.
const int kCurrentVersionNumber = 69;
const int kCompatibleVersionNumber = 16;

const char kEarlyExpirationThresholdKey[] = "early_expiration_threshold";
const char kMayContainForeignVisits[] = "may_contain_foreign_visits";
const char kDeleteForeignVisitsUntilId[] = "delete_foreign_visits_until_id";
const char kKnownToSyncVisitsExist[] = "known_to_sync_visits_exist";

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
                       download_interrupt_reason_crash),
      db_({// Note that we don't set exclusive locking here. That's done by
           // BeginExclusiveMode below which is called later (we have to be in
           // shared mode to start out for the in-memory backend to read the
           // data).
           // TODO(crbug.com/40159106) Remove this dependency on normal locking
           // mode.
           .exclusive_locking = false,
           // Set the database page size to something a little larger to give us
           // better performance (we're typically seek rather than bandwidth
           // limited). Must be a power of 2 and a max of 65536.
           .page_size = 4096,
           // Set the cache size. The page size, plus a little extra, times this
           // value, tells us how much memory the cache will use maximum.
           // 1000 * 4kB = 4MB
           .cache_size = 1000}),
      history_metadata_db_(&db_, &meta_table_) {}

HistoryDatabase::~HistoryDatabase() = default;

sql::InitStatus HistoryDatabase::Init(const base::FilePath& history_name) {
  db_.set_histogram_tag("History");

  if (!db_.Open(history_name))
    return LogInitFailure(InitStep::OPEN);

  // Wrap the rest of init in a transaction. This will prevent the database from
  // getting corrupted if we crash in the middle of initialization or migration.
  sql::Transaction committer(&db_);
  if (!committer.Begin())
    return LogInitFailure(InitStep::TRANSACTION_BEGIN);

#if BUILDFLAG(IS_APPLE)
  // Exclude the history file from backups.
  base::apple::SetBackupExclusion(history_name);
#endif

  // Prime the cache.
  db_.Preload();

  // Create the tables and indices. If you add something here, also add it to
  // `RecreateAllTablesButURL()`.
  if (!meta_table_.Init(&db_, GetCurrentVersion(), kCompatibleVersionNumber))
    return LogInitFailure(InitStep::META_TABLE_INIT);
  if (!CreateURLTable(false) || !InitVisitTable() ||
      !InitKeywordSearchTermsTable() || !InitDownloadTable() ||
      !InitSegmentTables() || !InitVisitAnnotationsTables() ||
      !CreateVisitedLinkTable() || !history_metadata_db_.Init()) {
    return LogInitFailure(InitStep::CREATE_TABLES);
  }
  CreateMainURLIndex();

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

  sql::Statement visited_link_count(
      db_.GetUniqueStatement("SELECT count(*) FROM visited_links"));
  if (!visited_link_count.Step()) {
    return;
  }
  UMA_HISTOGRAM_COUNTS_1M("History.VisitedLinkTableCount",
                          visited_link_count.ColumnInt(0));

  UMA_HISTOGRAM_TIMES("History.DatabaseBasicMetricsTime",
                      base::TimeTicks::Now() - start_time);

  // Compute metrics about foreign visits (i.e. visits coming from other
  // devices) in the DB.
  start_time = base::TimeTicks::Now();

  sql::Statement foreign_visits_sql(db_.GetUniqueStatement(
      "SELECT from_visit, opener_visit, originator_cache_guid, "
      "originator_visit_id, originator_from_visit, originator_opener_visit "
      "FROM visits WHERE originator_cache_guid IS NOT NULL AND "
      "originator_cache_guid != ''"));

  size_t total_foreign_visits = 0;
  size_t legacy_foreign_visits = 0;
  size_t unmapped_foreign_visits = 0;
  size_t mappable_from_visits = 0;
  size_t mappable_opener_visits = 0;
  while (foreign_visits_sql.Step()) {
    ++total_foreign_visits;

    VisitID from_visit = foreign_visits_sql.ColumnInt64(0);
    VisitID opener_visit = foreign_visits_sql.ColumnInt64(1);
    std::string originator_cache_guid = foreign_visits_sql.ColumnString(2);
    VisitID originator_visit = foreign_visits_sql.ColumnInt64(3);
    VisitID originator_from_visit = foreign_visits_sql.ColumnInt64(4);
    VisitID originator_opener_visit = foreign_visits_sql.ColumnInt64(5);

    // Foreign visits that don't have an originator_visit_id must have come
    // from a "legacy" client, i.e. one that's using the Sessions integration
    // to sync history.
    if (originator_visit == 0) {
      ++legacy_foreign_visits;
    }

    bool missing_from_visit = (from_visit == 0 && originator_from_visit != 0);
    bool missing_opener_visit =
        (opener_visit == 0 && originator_opener_visit != 0);
    if (missing_from_visit || missing_opener_visit) {
      // Found a visit that's missing the local from/opener_visit values.
      ++unmapped_foreign_visits;
      // Check if a matching referrer/opener visits actually exist in the DB.
      sql::Statement matching_visit(db_.GetCachedStatement(
          SQL_FROM_HERE,
          "SELECT id FROM visits WHERE originator_cache_guid=? AND "
          "originator_visit_id=?"));
      if (missing_from_visit) {
        matching_visit.BindString(0, originator_cache_guid);
        matching_visit.BindInt64(1, originator_from_visit);
        if (matching_visit.Step()) {
          ++mappable_from_visits;
        }
        matching_visit.Reset(/*clear_bound_vars=*/true);
      }
      if (missing_opener_visit) {
        matching_visit.BindString(0, originator_cache_guid);
        matching_visit.BindInt64(1, originator_opener_visit);
        if (matching_visit.Step()) {
          ++mappable_opener_visits;
        }
      }
    }
  }
  // Only record these metrics if there are any foreign visits in the DB.
  if (total_foreign_visits > 0) {
    base::UmaHistogramCounts1M("History.ForeignVisitsTotal",
                               total_foreign_visits);
    base::UmaHistogramCounts1M("History.ForeignVisitsLegacy",
                               legacy_foreign_visits);
    base::UmaHistogramCounts1M("History.ForeignVisitsNotRemapped",
                               unmapped_foreign_visits);
    base::UmaHistogramCounts1M("History.ForeignVisitsRemappableFrom",
                               mappable_from_visits);
    base::UmaHistogramCounts1M("History.ForeignVisitsRemappableOpener",
                               mappable_opener_visits);
  }

  base::UmaHistogramTimes("History.DatabaseForeignVisitMetricsTime",
                          base::TimeTicks::Now() - start_time);

  // Compute the advanced metrics even less often, pending timing data showing
  // that's not necessary.
  if (base::RandInt(1, 3) == 3) {
    start_time = base::TimeTicks::Now();

    // Collect all URLs visited within the last month.
    base::Time one_month_ago = base::Time::Now() - base::Days(30);
    sql::Statement url_sql(db_.GetUniqueStatement(
        "SELECT url, last_visit_time FROM urls WHERE last_visit_time > ?"));
    url_sql.BindTime(0, one_month_ago);

    // Count URLs (which will always be unique) and unique hosts within the last
    // week and last month.
    int week_url_count = 0;
    int month_url_count = 0;
    std::set<std::string> week_hosts;
    std::set<std::string> month_hosts;
    base::Time one_week_ago = base::Time::Now() - base::Days(7);
    while (url_sql.Step()) {
      GURL url(url_sql.ColumnString(0));
      base::Time visit_time = url_sql.ColumnTime(1);
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
  // Collect all URLs visited within the last month.
  base::Time one_month_ago = base::Time::Now() - base::Days(30);

  sql::Statement url_sql(
      db_.GetUniqueStatement("SELECT url FROM urls "
                             "WHERE last_visit_time > ? "
                             "AND hidden = 0 "
                             "AND visit_count > 0"));
  url_sql.BindTime(0, one_month_ago);

  std::set<std::string> hosts;
  while (url_sql.Step()) {
    GURL url(url_sql.ColumnString(0));
    hosts.insert(url.host());
  }

  return hosts.size();
}

DomainsVisitedResult HistoryDatabase::GetUniqueDomainsVisited(
    base::Time begin_time,
    base::Time end_time) {
  sql::Statement url_sql(db_.GetUniqueStatement(
      "SELECT urls.url, visits.originator_cache_guid, "
      "IFNULL(visit_source.source, ?) "  // SOURCE_BROWSED
      "FROM urls "
      "INNER JOIN visits ON urls.id = visits.url "
      "LEFT JOIN visit_source ON visits.id = visit_source.id "
      "WHERE (transition & ?) != 0 "            // CHAIN_END
      "AND (transition & ?) NOT IN (?, ?, ?) "  // No *_SUBFRAME or
                                                // KEYWORD_GENERATED
      "AND hidden = 0 AND visit_time >= ? AND visit_time < ? "
      "ORDER BY visit_time DESC, visits.id DESC"));

  url_sql.BindInt64(0, VisitSource::SOURCE_BROWSED);
  url_sql.BindInt64(1, ui::PAGE_TRANSITION_CHAIN_END);
  url_sql.BindInt64(2, ui::PAGE_TRANSITION_CORE_MASK);
  url_sql.BindInt64(3, ui::PAGE_TRANSITION_AUTO_SUBFRAME);
  url_sql.BindInt64(4, ui::PAGE_TRANSITION_MANUAL_SUBFRAME);
  url_sql.BindInt64(5, ui::PAGE_TRANSITION_KEYWORD_GENERATED);

  url_sql.BindTime(6, begin_time);
  url_sql.BindTime(7, end_time);

  DomainsVisitedResult result;

  std::set<std::string> all_visited_domains_set;
  std::set<std::string> locally_visited_domains_set;

  while (url_sql.Step()) {
    GURL url(url_sql.ColumnString(0));
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        url, net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);

    // IP addresses, empty URLs, and URLs with empty or unregistered TLDs are
    // all excluded.
    if (domain.empty()) {
      continue;
    }

    if (!all_visited_domains_set.contains(domain)) {
      all_visited_domains_set.insert(domain);
      result.all_visited_domains.push_back(domain);
    }

    bool is_local = url_sql.ColumnString(1).empty() &&
                    url_sql.ColumnInt(2) == VisitSource::SOURCE_BROWSED;

    if (is_local && !locally_visited_domains_set.contains(domain)) {
      locally_visited_domains_set.insert(domain);
      result.locally_visited_domains.push_back(domain);
    }
  }

  return result;
}

std::pair<int, int> HistoryDatabase::CountUniqueDomainsVisited(
    base::Time begin_time,
    base::Time end_time) {
  DomainsVisitedResult result = GetUniqueDomainsVisited(begin_time, end_time);
  return {result.locally_visited_domains.size(),
          result.all_visited_domains.size()};
}

void HistoryDatabase::BeginExclusiveMode() {
  // We need to use a PRAGMA statement here as the DB has already been created.
  std::ignore = db_.Execute("PRAGMA locking_mode=EXCLUSIVE");
}

// static
int HistoryDatabase::GetCurrentVersion() {
  return kCurrentVersionNumber;
}

std::unique_ptr<sql::Transaction> HistoryDatabase::CreateTransaction() {
  return std::make_unique<sql::Transaction>(&db_);
}

bool HistoryDatabase::RecreateAllTablesButURL() {
  if (!DropVisitTable())
    return false;
  if (!InitVisitTable())
    return false;

  if (!DropVisitedLinkTable()) {
    return false;
  }
  if (!CreateVisitedLinkTable()) {
    return false;
  }

  if (!DropKeywordSearchTermsTable())
    return false;
  if (!InitKeywordSearchTermsTable())
    return false;

  if (!DropSegmentTables())
    return false;
  if (!InitSegmentTables())
    return false;

  if (!DropVisitAnnotationsTables())
    return false;
  if (!InitVisitAnnotationsTables())
    return false;

  return true;
}

void HistoryDatabase::Vacuum() {
  DCHECK_EQ(0, db_.transaction_nesting()) <<
      "Can not have a transaction when vacuuming.";
  std::ignore = db_.Execute("VACUUM");
}

void HistoryDatabase::TrimMemory() {
  db_.TrimMemory();
}

bool HistoryDatabase::Raze() {
  return db_.Raze();
}

std::string HistoryDatabase::GetDiagnosticInfo(
    int extended_error,
    sql::Statement* statement,
    sql::DatabaseDiagnostics* diagnostics) {
  return db_.GetDiagnosticInfo(extended_error, statement, diagnostics);
}

bool HistoryDatabase::SetSegmentID(VisitID visit_id, SegmentID segment_id) {
  sql::Statement s(db_.GetCachedStatement(SQL_FROM_HERE,
      "UPDATE visits SET segment_id = ? WHERE id = ?"));
  s.BindInt64(0, segment_id);
  s.BindInt64(1, visit_id);
  DCHECK_EQ(db_.GetLastChangeCount(), 1)
      << "segment_id: " << segment_id << ", visit_id: " << visit_id;

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

bool HistoryDatabase::MayContainForeignVisits() {
  int may_contain_foreign_visits = false;
  meta_table_.GetValue(kMayContainForeignVisits, &may_contain_foreign_visits);
  return may_contain_foreign_visits != 0;
}

void HistoryDatabase::SetMayContainForeignVisits(
    bool may_contain_foreign_visits) {
  meta_table_.SetValue(kMayContainForeignVisits,
                       may_contain_foreign_visits ? 1 : 0);
}

VisitID HistoryDatabase::GetDeleteForeignVisitsUntilId() {
  VisitID visit_id = kInvalidVisitID;
  meta_table_.GetValue(kDeleteForeignVisitsUntilId, &visit_id);
  return visit_id;
}

void HistoryDatabase::SetDeleteForeignVisitsUntilId(VisitID visit_id) {
  meta_table_.SetValue(kDeleteForeignVisitsUntilId, visit_id);
}

bool HistoryDatabase::KnownToSyncVisitsExist() {
  int result = false;
  meta_table_.GetValue(kKnownToSyncVisitsExist, &result);
  return result != 0;
}

void HistoryDatabase::SetKnownToSyncVisitsExist(bool exist) {
  meta_table_.SetValue(kKnownToSyncVisitsExist, exist ? 1 : 0);
}

HistorySyncMetadataDatabase* HistoryDatabase::GetHistoryMetadataDB() {
  return &history_metadata_db_;
}

sql::Database& HistoryDatabase::GetDBForTesting() {
  return db_;
}

sql::Database& HistoryDatabase::GetDB() {
  return db_;
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
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
    std::ignore = meta_table_.SetCompatibleVersionNumber(
        std::min(cur_version, kCompatibleVersionNumber));
  }

  if (cur_version == 16) {
#if !BUILDFLAG(IS_WIN)
    // In this version we bring the time format on Mac & Linux in sync with the
    // Windows version so that profiles can be moved between computers.
    MigrateTimeEpoch();
#endif
    // On all platforms we bump the version number, so on Windows this
    // migration is a NOP. We keep the compatible version at 16 since things
    // will basically still work, just history will be in the future if an
    // old version reads it.
    ++cur_version;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 17) {
    // Version 17 was for thumbnails to top sites migration. We ended up
    // disabling it though, so 17->18 does nothing.
    ++cur_version;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 18) {
    // This is the version prior to adding url_source column. We need to
    // migrate the database.
    cur_version = 19;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 19) {
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
    // This was the thumbnail migration.  Obsolete.
  }

  if (cur_version == 20) {
    // This is the version prior to adding the visit_duration field in visits
    // database. We need to migrate the database.
    if (!MigrateVisitsWithoutDuration())
      return LogMigrationFailure(20);
    ++cur_version;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 21) {
    // The android_urls table's data schemal was changed in version 21.
#if BUILDFLAG(IS_ANDROID)
    if (!MigrateToVersion22())
      return LogMigrationFailure(21);
#endif
    ++cur_version;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 22) {
    if (!MigrateDownloadsState())
      return LogMigrationFailure(22);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 23) {
    if (!MigrateDownloadsReasonPathsAndDangerType())
      return LogMigrationFailure(23);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 24) {
    if (!MigratePresentationIndex())
      return LogMigrationFailure(24);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 25) {
    if (!MigrateReferrer())
      return LogMigrationFailure(25);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 26) {
    if (!MigrateDownloadedByExtension())
      return LogMigrationFailure(26);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 27) {
    if (!MigrateDownloadValidators())
      return LogMigrationFailure(27);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 28) {
    if (!MigrateMimeType())
      return LogMigrationFailure(28);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 29) {
    if (!MigrateHashHttpMethodAndGenerateGuids())
      return LogMigrationFailure(29);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 30) {
    if (!MigrateDownloadTabUrl())
      return LogMigrationFailure(30);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 31) {
    if (!MigrateDownloadSiteInstanceUrl())
      return LogMigrationFailure(31);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 32) {
    // New download slices table is introduced, no migration needed.
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 33) {
    if (!MigrateDownloadLastAccessTime())
      return LogMigrationFailure(33);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 34) {
    // This originally contained an autoincrement migration which was abandoned
    // and added back in version 36. (see https://crbug.com/736136)
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 35) {
    if (!MigrateDownloadTransient())
      return LogMigrationFailure(35);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
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
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 37) {
    if (!MigrateVisitSegmentNames())
      return LogMigrationFailure(37);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 38) {
    if (!MigrateDownloadSliceFinished())
      return LogMigrationFailure(38);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 39) {
    if (!MigrateVisitsWithoutIncrementedOmniboxTypedScore())
      return LogMigrationFailure(39);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 40) {
    // The migration to version 40 concerned Sync metadata for TypedURLs, which
    // doesn't exist anymore in current versions (68+). So nothing to do here.
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 41) {
    if (!MigrateKeywordsSearchTermsLowerTermColumn())
      return LogMigrationFailure(41);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 42) {
    if (!MigrateVisitsWithoutPubliclyRoutableColumn())
      return LogMigrationFailure(42);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 43) {
    if (!CanMigrateFlocAllowed() || !MigrateFlocAllowedToAnnotationsTable())
      return LogMigrationFailure(43);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 44) {
    MigrateReplaceClusterVisitsTable();
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 45) {
    // New download reroute infos table is introduced, no migration needed.
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 46) {
    if (!MigrateContentAnnotationsWithoutEntitiesColumn())
      return LogMigrationFailure(46);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 47) {
    if (!MigrateContentAnnotationsAddRelatedSearchesColumn())
      return LogMigrationFailure(47);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 48) {
    if (!MigrateVisitsWithoutOpenerVisitColumnAndDropPubliclyRoutableColumn())
      return LogMigrationFailure(48);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 49) {
    if (!MigrateContentAnnotationsAddVisibilityScore())
      return LogMigrationFailure(49);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 50) {
    if (!MigrateContextAnnotationsAddTotalForegroundDuration())
      return LogMigrationFailure(50);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 51) {
    if (!MigrateEmbedderDownloadData())
      return LogMigrationFailure(51);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 52) {
    if (!MigrateContentAnnotationsAddSearchMetadata())
      return LogMigrationFailure(52);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 53) {
    if (!MigrateContentAnnotationsAddAlternativeTitle())
      return LogMigrationFailure(53);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 54) {
    if (!MigrateVisitsAutoincrementIdAndAddOriginatorColumns())
      return LogMigrationFailure(54);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 55) {
    if (!MigrateVisitsAddOriginatorFromVisitAndOpenerVisitColumns())
      return LogMigrationFailure(55);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 56) {
    if (!MigrateClustersAddColumns())
      return LogMigrationFailure(56);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 57) {
    if (!MigrateAnnotationsAddColumnsForSync())
      return LogMigrationFailure(57);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 58) {
    if (!MigrateVisitsAddIsKnownToSyncColumn())
      return LogMigrationFailure(58);
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 59) {
    if (!MigrateClustersAddTriggerabilityCalculated()) {
      return LogMigrationFailure(59);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 60) {
    if (!MigrateClustersAutoincrementIdAndAddOriginatorColumns()) {
      return LogMigrationFailure(60);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 61) {
    if (!MigrateContentAnnotationsAddHasUrlKeyedImage()) {
      return LogMigrationFailure(61);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 62) {
    if (!MigrateVisitsAddConsiderForNewTabPageMostVisitedColumn()) {
      return LogMigrationFailure(62);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 63) {
    if (!MigrateDownloadByWebApp()) {
      return LogMigrationFailure(63);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 64) {
    if (!MigrateClustersAndVisitsAddInteractionState()) {
      return LogMigrationFailure(64);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 65) {
    if (!MigrateVisitsAddExternalReferrerUrlColumn()) {
      return LogMigrationFailure(65);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 66) {
    if (!MigrateVisitsAddVisitedLinkIdColumn()) {
      return LogMigrationFailure(66);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 67) {
    if (!MigrateRemoveTypedUrlMetadata()) {
      return LogMigrationFailure(67);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
  }

  if (cur_version == 68) {
    if (!MigrateVisitsAddAppId()) {
      return LogMigrationFailure(68);
    }
    cur_version++;
    // TODO(crbug.com/40891923): Handle failure instead of ignoring it.
    std::ignore = meta_table_.SetVersionNumber(cur_version);
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

#if !BUILDFLAG(IS_WIN)
void HistoryDatabase::MigrateTimeEpoch() {
  // Update all the times in the URLs and visits table in the main database.
  std::ignore = db_.Execute(
      "UPDATE urls "
      "SET last_visit_time = last_visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM urls WHERE last_visit_time > 0);");
  std::ignore = db_.Execute(
      "UPDATE visits "
      "SET visit_time = visit_time + 11644473600000000 "
      "WHERE id IN (SELECT id FROM visits WHERE visit_time > 0);");
  std::ignore = db_.Execute(
      "UPDATE segment_usage "
      "SET time_slot = time_slot + 11644473600000000 "
      "WHERE id IN (SELECT id FROM segment_usage WHERE time_slot > 0);");
}
#endif

bool HistoryDatabase::MigrateRemoveTypedUrlMetadata() {
  if (!meta_table_.DeleteKey("typed_url_model_type_state")) {
    return false;
  }
  if (!db_.Execute("DROP TABLE IF EXISTS typed_url_sync_metadata;")) {
    return false;
  }
  return true;
}

}  // namespace history
