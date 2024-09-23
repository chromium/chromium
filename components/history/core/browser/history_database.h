// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DATABASE_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/history/core/browser/download_database.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/sync/history_sync_metadata_database.h"
#include "components/history/core/browser/url_database.h"
#include "components/history/core/browser/visit_annotations_database.h"
#include "components/history/core/browser/visit_database.h"
#include "components/history/core/browser/visited_link_database.h"
#include "components/history/core/browser/visitsegment_database.h"
#include "sql/database.h"
#include "sql/init_status.h"
#include "sql/meta_table.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/history/core/browser/android/android_urls_database.h"
#endif

namespace base {
class FilePath;
}

namespace sql {
class Transaction;
}

class InMemoryURLIndexTest;

namespace history {

// Encapsulates the SQL connection for the history database. This class holds
// the database connection and has methods the history system (including full
// text search) uses for writing and retrieving information.
//
// We try to keep most logic out of the history database; this should be seen
// as the storage interface. Logic for manipulating this storage layer should
// be in HistoryBackend.cc.
class HistoryDatabase : public DownloadDatabase,
#if BUILDFLAG(IS_ANDROID)
                        public AndroidURLsDatabase,
#endif
                        public URLDatabase,
                        public VisitDatabase,
                        public VisitAnnotationsDatabase,
                        public VisitedLinkDatabase,
                        public VisitSegmentDatabase {
 public:
  // Must call Init() to complete construction. Although it can be created on
  // any thread, it must be destructed on the history thread for proper
  // database cleanup.
  HistoryDatabase(DownloadInterruptReason download_interrupt_reason_none,
                  DownloadInterruptReason download_interrupt_reason_crash);

  HistoryDatabase(const HistoryDatabase&) = delete;
  HistoryDatabase& operator=(const HistoryDatabase&) = delete;

  ~HistoryDatabase() override;

  // Call before Init() to set the error callback to be used for the
  // underlying database connection.
  void set_error_callback(const sql::Database::ErrorCallback& error_callback) {
    db_.set_error_callback(error_callback);
  }
  void reset_error_callback() { db_.reset_error_callback(); }

  // Must call this function to complete initialization. Will return
  // sql::INIT_OK on success. Otherwise, no other function should be called. You
  // may want to call BeginExclusiveMode after this when you are ready.
  sql::InitStatus Init(const base::FilePath& history_name);

  // Computes and records various metrics for the database. Should only be
  // called once and only upon successful Init.
  void ComputeDatabaseMetrics(const base::FilePath& filename);

  // Counts the number of unique Hosts visited in the last month.
  int CountUniqueHostsVisitedLastMonth();

  // Gets unique domains (eTLD+1) visited within the time range
  // [`begin_time`, `end_time`) for local and synced visits sorted in
  // reverse-chronological order.
  DomainsVisitedResult GetUniqueDomainsVisited(base::Time begin_time,
                                               base::Time end_time);

  // Counts the number of unique domains (eTLD+1) visited within
  // [`begin_time`, `end_time`).
  // The return value is a pair of (local, all), where "local" only counts
  // domains that were visited on this device, whereas "all" also counts
  // foreign/synced visits.
  std::pair<int, int> CountUniqueDomainsVisited(base::Time begin_time,
                                                base::Time end_time);

  // Call to set the mode on the database to exclusive. The default locking mode
  // is "normal" but we want to run in exclusive mode for slightly better
  // performance since we know nobody else is using the database. This is
  // separate from Init() since the in-memory database attaches to slurp the
  // data out, and this can't happen in exclusive mode.
  void BeginExclusiveMode();

  // Returns the current version that we will generate history databases with.
  static int GetCurrentVersion();

  // Creates a new inactive transaction for the history database. Caller is
  // responsible for calling `sql::Transaction::Begin()` and checking the return
  // value. Only call this after `Init()`.
  //
  // There should only ever be one instance of these alive, as transaction
  // nesting doesn't exist. The caller is responsible for ensuring this, and
  // therefore, ONLY the owner of this instance (`HistoryBackend`) should call
  // this, NOT any `HistoryDBTask`, which has a non-owning pointer to this.
  std::unique_ptr<sql::Transaction> CreateTransaction();

  // We DO NOT support transaction nesting. It's considered a "misfeature", and
  // so the return value of this should always be 0 or 1 during runtime.
  int transaction_nesting() const { return db_.transaction_nesting(); }

  // Drops all tables except the URL, and download tables, and recreates them
  // from scratch. This is done to rapidly clean up stuff when deleting all
  // history. It is faster and less likely to have problems that deleting all
  // rows in the tables.
  //
  // We don't delete the downloads table, since there may be in progress
  // downloads. We handle the download history clean up separately in:
  // content::DownloadManager::RemoveDownloadsFromHistoryBetween.
  //
  // Returns true on success. On failure, the caller should assume that the
  // database is invalid. There could have been an error recreating a table.
  // This should be treated the same as an init failure, and the database
  // should not be used any more.
  //
  // This will also recreate the supplementary URL indices, since these
  // indices won't be created automatically when using the temporary URL
  // table (what the caller does right before calling this).
  bool RecreateAllTablesButURL();

  // Vacuums the database. This will cause sqlite to defragment and collect
  // unused space in the file. It can be VERY SLOW.
  void Vacuum();

  // Release all non-essential memory associated with this database connection.
  void TrimMemory();

  // Razes the database. Returns true if successful.
  bool Raze();

  // A simple passthrough to `sql::Database::GetDiagnosticInfo()`.
  std::string GetDiagnosticInfo(
      int extended_error,
      sql::Statement* statement,
      sql::DatabaseDiagnostics* diagnostics = nullptr);

  // Visit table functions ----------------------------------------------------

  // Update the segment id of a visit. Return true on success.
  bool SetSegmentID(VisitID visit_id, SegmentID segment_id);

  // Query the segment ID for the provided visit. Return 0 on failure or if the
  // visit id wasn't found.
  SegmentID GetSegmentID(VisitID visit_id);

  // Retrieves/Updates early expiration threshold, which specifies the earliest
  // known point in history that may possibly to contain visits suitable for
  // early expiration (AUTO_SUBFRAMES).
  virtual base::Time GetEarlyExpirationThreshold();
  virtual void UpdateEarlyExpirationThreshold(base::Time threshold);

  // Retrieves/updates the bit that indicates whether the DB may contain any
  // foreign visits, i.e. visits coming from other syncing devices.
  // Note that this only counts visits *not* pending deletion (see below) - as
  // soon as a deletion operation is started, this will get set to false.
  bool MayContainForeignVisits();
  void SetMayContainForeignVisits(bool may_contain_foreign_visits);

  // Retrieves/updates the max-foreign-visit-to-delete threshold. If this is
  // not kInvalidVisitID, then all foreign visits with an ID <= this value
  // should be deleted from the DB.
  VisitID GetDeleteForeignVisitsUntilId();
  void SetDeleteForeignVisitsUntilId(VisitID visit_id);

  // Retrieves/updates the bit that indicates whether the DB may contain any
  // visits known to sync.
  bool KnownToSyncVisitsExist();
  void SetKnownToSyncVisitsExist(bool exist);

  // Sync metadata storage ----------------------------------------------------

  // Returns the sub-database used for storing Sync metadata for History.
  HistorySyncMetadataDatabase* GetHistoryMetadataDB();

  sql::Database& GetDBForTesting();

 private:
#if BUILDFLAG(IS_ANDROID)
  // AndroidProviderBackend uses the `db_`.
  friend class AndroidProviderBackend;
  FRIEND_TEST_ALL_PREFIXES(AndroidURLsMigrationTest, MigrateToVersion22);
#endif
  friend class ::InMemoryURLIndexTest;

  // Overridden from URLDatabase, DownloadDatabase, VisitDatabase, and
  // VisitSegmentDatabase.
  sql::Database& GetDB() override;

  // Migration -----------------------------------------------------------------

  // Makes sure the version is up to date, updating if necessary. If the
  // database is too old to migrate, the user will be notified. Returns
  // sql::INIT_OK iff the DB is up to date and ready for use.
  //
  // This assumes it is called from the init function inside a transaction. It
  // may commit the transaction and start a new one if migration requires it.
  sql::InitStatus EnsureCurrentVersion();

#if !BUILDFLAG(IS_WIN)
  // Converts the time epoch in the database from being 1970-based to being
  // 1601-based which corresponds to the change in Time.internal_value_.
  void MigrateTimeEpoch();
#endif

  bool MigrateRemoveTypedUrlMetadata();

  // ---------------------------------------------------------------------------

  sql::Database db_;
  sql::MetaTable meta_table_;

  // Most of the sub-DBs (URLDatabase etc.) are integrated into HistoryDatabase
  // via inheritance. However, that can lead to "diamond inheritance" issues
  // when multiple of these base classes define the same methods. Therefore the
  // Sync metadata DB is integrated via composition instead.
  HistorySyncMetadataDatabase history_metadata_db_;

  base::Time cached_early_expiration_threshold_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_HISTORY_DATABASE_H_
