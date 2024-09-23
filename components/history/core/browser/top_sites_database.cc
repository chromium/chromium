// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/top_sites_database.h"

#include <stddef.h>
#include <stdint.h>

#include <tuple>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "components/history/core/browser/features.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/top_sites.h"
#include "sql/database.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"

namespace history {

// Description of database table:
//
// top_sites
//   url              URL of the top site.
//   url_rank         Index of the site, 0-based. The site with the highest rank
//                    will be the next one evicted.
//   title            The title to display under that site.

namespace {

// For this database, schema migrations are deprecated after two
// years.  This means that the oldest non-deprecated version should be
// two years old or greater (thus the migrations to get there are
// older).  Databases containing deprecated versions will be cleared
// at startup.  Since this database is a cache, losing old data is not
// fatal (in fact, very old data may be expired immediately at startup
// anyhow).

// Version 5: TODO apaseltiner@chromium.org on 2022-09-21
// Version 4: 95af34ec/r618360 kristipark@chromium.org on 2018-12-20
// Version 3: b6d6a783/r231648 by beaudoin@chromium.org on 2013-10-29
// Version 2: eb0b24e6/r87284 by satorux@chromium.org on 2011-05-31 (deprecated)
// Version 1: 809cc4d8/r64072 by sky@chromium.org on 2010-10-27 (deprecated)

// NOTE(shess): When changing the version, add a new golden file for
// the new version and a test to verify that Init() works with it.
static const int kVersionNumber = 5;
static const int kDeprecatedVersionNumber = 3;  // and earlier.

// Rank used to indicate that this is a newly added URL.
static const int kRankOfNewURL = -1;

bool InitTables(sql::Database* db) {
  static constexpr char kTopSitesSql[] =
      "CREATE TABLE IF NOT EXISTS top_sites("
      "url TEXT NOT NULL PRIMARY KEY,"
      "url_rank INTEGER NOT NULL,"
      "title TEXT NOT NULL)";
  return db->Execute(kTopSitesSql);
}

// Most corruption comes down to atomic updates between pages being broken
// somehow.  This can result in either missing data, or overlapping data,
// depending on the operation broken.  This table has large rows, which will use
// overflow pages, so it is possible (though unlikely) that a chain could fit
// together and yield a row with errors.
void FixTopSitesTable(sql::Database& db) {
  // Enforce invariant that url_rank>=0 forms a contiguous series.
  // TODO(shess): I have not found an UPDATE+SUBSELECT method of managing this.
  // It can be done with a temporary table and a subselect, but doing it
  // manually is easier to follow.  Another option would be to somehow integrate
  // the renumbering into the table recovery code.
  static constexpr char kByRankSql[] =
      "SELECT url_rank,rowid FROM top_sites "
      "WHERE url_rank<>-1 "
      "ORDER BY url_rank";
  sql::Statement select_statement(db.GetUniqueStatement(kByRankSql));

  static constexpr char kAdjustRankSql[] =
      "UPDATE top_sites SET url_rank=? WHERE rowid=?";
  sql::Statement update_statement(db.GetUniqueStatement(kAdjustRankSql));

  // Update any rows where `next_rank` doesn't match `url_rank`.
  int next_rank = 0;
  while (select_statement.Step()) {
    const int url_rank = select_statement.ColumnInt(0);
    if (url_rank != next_rank) {
      update_statement.Reset(true);
      update_statement.BindInt(0, next_rank);
      update_statement.BindInt64(1, select_statement.ColumnInt64(1));
      update_statement.Run();
    }
    ++next_rank;
  }
}

}  // namespace

void TopSitesDatabase::DatabaseErrorCallback(const base::FilePath& db_path,
                                             int extended_error,
                                             sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(db_);

  if (sql::Recovery::RecoverIfPossible(
          db_.get(), extended_error,
          sql::Recovery::Strategy::kRecoverWithMetaVersionOrRaze)) {
    // Recovery was attempted. The database handle has been poisoned and the
    // error callback has been reset.

    // Since the database was recovered from corruption, it's possible that some
    // data in the newly recovered database is incorrect. When re-opening the
    // database, we should attempt to fix any broken constraints.
    //
    // Unlike below when using sql::Recovery, which runs `FixTopSitesTable()` on
    // the recovered copy of the database before overwriting the original
    // (corrupted) database, here we defer the fix-up logic to after we've
    // re-opened the database and all the other checks in the Init() method
    // (version, schema, etc) pass.
    //
    // Note that recovery is only run when we detect corruption, but undetected
    // corruption can happen at any time. We could consider running
    // `FixTopSitesTable()` every time the database is opened, but in most cases
    // that would be a (possibly expensive) no-op.
    needs_fixing_up_ = true;
  }

  // Signal the test-expectation framework that the error was handled.
  std::ignore = sql::Database::IsExpectedSqliteError(extended_error);
}

// static
const int TopSitesDatabase::kRankOfNonExistingURL = -2;

TopSitesDatabase::TopSitesDatabase() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

TopSitesDatabase::~TopSitesDatabase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool TopSitesDatabase::Init(const base::FilePath& db_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Retry failed InitImpl() in case the recovery system fixed things.
  // TODO(shess): Instrument to figure out if there are any persistent failure
  // cases which do not resolve themselves.
  const size_t kAttempts = 2;

  for (size_t i = 0; i < kAttempts; ++i) {
    if (InitImpl(db_name))
      return true;

    db_.reset();
  }
  return false;
}

bool TopSitesDatabase::UpgradeToVersion5(sql::MetaTable& meta_table) {
  DCHECK(db_);
  DCHECK(db_->HasActiveTransactions());
  DCHECK_EQ(4, meta_table.GetVersionNumber());

  static constexpr char kCreateSql[] =
      "CREATE TABLE new_top_sites("
      "url TEXT NOT NULL PRIMARY KEY,"
      "url_rank INTEGER NOT NULL,"
      "title TEXT NOT NULL)";
  if (!db_->Execute(kCreateSql))
    return false;

  static constexpr char kMigrateSql[] =
      "INSERT INTO new_top_sites(url,url_rank,title)"
      "SELECT url,url_rank,title FROM top_sites "
      "WHERE url IS NOT NULL AND url_rank IS NOT NULL AND title IS NOT NULL";
  if (!db_->Execute(kMigrateSql))
    return false;

  static constexpr char kDropSql[] = "DROP TABLE top_sites";
  if (!db_->Execute(kDropSql))
    return false;

  static constexpr char kRenameSql[] =
      "ALTER TABLE new_top_sites "
      "RENAME TO top_sites";
  if (!db_->Execute(kRenameSql))
    return false;

  return meta_table.SetVersionNumber(5);
}

bool TopSitesDatabase::InitImpl(const base::FilePath& db_name) {
  const bool file_existed = base::PathExists(db_name);

  // Settings copied from FaviconDatabase.
  db_ = std::make_unique<sql::Database>(
      sql::DatabaseOptions{.page_size = 4096, .cache_size = 32});
  db_->set_histogram_tag("TopSites");
  db_->set_error_callback(
      base::BindRepeating(&TopSitesDatabase::DatabaseErrorCallback,
                          base::Unretained(this), db_name));

  if (!db_->Open(db_name))
    return false;

  // An older version had data with no meta table.  Deprecate by razing.
  // TODO(shess): Just have RazeIfDeprecated() handle this case.
  const bool does_meta_exist = sql::MetaTable::DoesTableExist(db_.get());
  if (!does_meta_exist && file_existed) {
    if (!db_->Raze())
      return false;
  }

  // Clear databases which are too old to process.
  DCHECK_LT(kDeprecatedVersionNumber, kVersionNumber);
  if (sql::MetaTable::RazeIfIncompatible(
          db_.get(), /*lowest_supported_version=*/kDeprecatedVersionNumber + 1,
          kVersionNumber) == sql::RazeIfIncompatibleResult::kFailed) {
    return false;
  }

  // Scope initialization in a transaction so we can't be partially
  // initialized.
  sql::Transaction transaction(db_.get());
  // TODO(shess): Failure to open transaction is bad, address it.
  if (!transaction.Begin())
    return false;

  sql::MetaTable meta_table;

  if (!meta_table.Init(db_.get(), kVersionNumber, kVersionNumber))
    return false;

  if (!InitTables(db_.get()))
    return false;

  if (meta_table.GetVersionNumber() == 4) {
    if (!UpgradeToVersion5(meta_table))
      return false;
  }

  // Version check.
  if (meta_table.GetVersionNumber() != kVersionNumber)
    return false;

  // Attempt to fix up the table if recovery was attempted when opening.
  if (needs_fixing_up_) {
    FixTopSitesTable(*db_);
    needs_fixing_up_ = false;
  }

  // Initialization is complete.
  return transaction.Commit();
}

void TopSitesDatabase::ApplyDelta(const TopSitesDelta& delta) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  sql::Transaction transaction(db_.get());
  if (!transaction.Begin())
    return;

  for (const auto& deleted : delta.deleted) {
    if (!RemoveURLNoTransaction(deleted))
      return;
  }

  for (const auto& added : delta.added)
    SetSiteNoTransaction(added.url, added.rank);

  for (const auto& moved : delta.moved)
    UpdateSiteRankNoTransaction(moved.url, moved.rank);

  transaction.Commit();
}

MostVisitedURLList TopSitesDatabase::GetSites() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MostVisitedURLList urls;

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT url,title "
                              "FROM top_sites ORDER BY url_rank"));

  if (!statement.is_valid()) {
    LOG(WARNING) << db_->GetErrorMessage();
    return urls;
  }

  while (statement.Step()) {
    // Results are sorted by url_rank.
    urls.emplace_back(GURL(statement.ColumnString(0)),
                      /*title=*/statement.ColumnString16(1));
  }

  return urls;
}

void TopSitesDatabase::SetSiteNoTransaction(const MostVisitedURL& url,
                                            int new_rank) {
  DCHECK(db_->HasActiveTransactions());

  int rank = GetURLRank(url);
  if (rank == kRankOfNonExistingURL) {
    AddSite(url, new_rank);
  } else {
    UpdateSiteRankNoTransaction(url, new_rank);
    UpdateSite(url);
  }
}

void TopSitesDatabase::AddSite(const MostVisitedURL& url, int new_rank) {
  DCHECK(db_->HasActiveTransactions());

  sql::Statement statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "INSERT OR REPLACE INTO top_sites "
                              "(url,url_rank,title)"
                              "VALUES(?,?,?)"));
  statement.BindString(0, url.url.spec());
  statement.BindInt(1, kRankOfNewURL);
  statement.BindString16(2, url.title);
  if (!statement.Run())
    return;

  // Update the new site's rank.
  UpdateSiteRankNoTransaction(url, new_rank);
}

bool TopSitesDatabase::UpdateSite(const MostVisitedURL& url) {
  sql::Statement statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                   "UPDATE top_sites SET "
                                                   "title=? "
                                                   "WHERE url=?"));
  statement.BindString16(0, url.title);
  statement.BindString(1, url.url.spec());
  return statement.Run();
}

int TopSitesDatabase::GetURLRank(const MostVisitedURL& url) {
  sql::Statement select_statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "SELECT url_rank "
                              "FROM top_sites WHERE url=?"));
  select_statement.BindString(0, url.url.spec());
  if (select_statement.Step())
    return select_statement.ColumnInt(0);

  return kRankOfNonExistingURL;
}

void TopSitesDatabase::UpdateSiteRankNoTransaction(const MostVisitedURL& url,
                                                   int new_rank) {
  DCHECK(db_->HasActiveTransactions());

  int prev_rank = GetURLRank(url);
  if (prev_rank == kRankOfNonExistingURL) {
    LOG(WARNING) << "Updating rank of an unknown URL: " << url.url.spec();
    return;
  }

  // TODO: consider returning early if any of the `Run()` calls below return
  // false.

  // Shift the ranks.
  if (prev_rank == kRankOfNewURL) {
    // Starting from new_rank, shift up.
    // Example: -1 -> 2
    // [-1 -> 2], 0, 1, [2 -> 3], [3 -> 4], [4 -> 5]
    sql::Statement shift_statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "UPDATE top_sites "
                                "SET url_rank=url_rank+1 "
                                "WHERE url_rank>=?"));
    shift_statement.BindInt(0, new_rank);
    shift_statement.Run();
  } else if (prev_rank > new_rank) {
    // From [new_rank, prev_rank), shift up.
    // Example: 3 -> 1
    // 0, [1 -> 2], [2 -> 3], [3 -> 1], 4
    sql::Statement shift_statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "UPDATE top_sites "
                                "SET url_rank=url_rank+1 "
                                "WHERE url_rank>=? AND url_rank<?"));
    shift_statement.BindInt(0, new_rank);
    shift_statement.BindInt(1, prev_rank);
    shift_statement.Run();
  } else if (prev_rank < new_rank) {
    // From (prev_rank, new_rank], shift down.
    // Example: 1 -> 3.
    // 0, [1 -> 3], [2 -> 1], [3 -> 2], 4
    sql::Statement shift_statement(
        db_->GetCachedStatement(SQL_FROM_HERE,
                                "UPDATE top_sites "
                                "SET url_rank=url_rank-1 "
                                "WHERE url_rank>? AND url_rank<=?"));
    shift_statement.BindInt(0, prev_rank);
    shift_statement.BindInt(1, new_rank);
    shift_statement.Run();
  }

  // Set the url's new_rank.
  sql::Statement set_statement(db_->GetCachedStatement(SQL_FROM_HERE,
                                                       "UPDATE top_sites "
                                                       "SET url_rank=? "
                                                       "WHERE url=?"));
  set_statement.BindInt(0, new_rank);
  set_statement.BindString(1, url.url.spec());
  set_statement.Run();
}

bool TopSitesDatabase::RemoveURLNoTransaction(const MostVisitedURL& url) {
  DCHECK(db_->HasActiveTransactions());

  int old_rank = GetURLRank(url);
  if (old_rank == kRankOfNonExistingURL)
    return true;

  // Decrement all following ranks.
  sql::Statement shift_statement(
      db_->GetCachedStatement(SQL_FROM_HERE,
                              "UPDATE top_sites "
                              "SET url_rank=url_rank-1 "
                              "WHERE url_rank>?"));
  shift_statement.BindInt(0, old_rank);

  if (!shift_statement.Run())
    return false;

  sql::Statement delete_statement(db_->GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM top_sites WHERE url=?"));
  delete_statement.BindString(0, url.url.spec());

  return delete_statement.Run();
}

sql::Database* TopSitesDatabase::db_for_testing() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return db_.get();
}

int TopSitesDatabase::GetURLRankForTesting(const MostVisitedURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetURLRank(url);
}

bool TopSitesDatabase::RemoveURLNoTransactionForTesting(
    const MostVisitedURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return RemoveURLNoTransaction(url);
}

}  // namespace history
