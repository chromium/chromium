// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_DATABASE_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_DATABASE_H_

#include <memory>

#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "components/history/core/browser/history_types.h"

namespace base {
class FilePath;
}  // namespace base

namespace sql {
class Database;
class MetaTable;
class Statement;
}  // namespace sql

namespace history {

class TopSitesDatabase {
 public:
  // Rank used to indicate that a URL is not stored in the database.
  static const int kRankOfNonExistingURL;

  TopSitesDatabase();

  TopSitesDatabase(const TopSitesDatabase&) = delete;
  TopSitesDatabase& operator=(const TopSitesDatabase&) = delete;

  ~TopSitesDatabase();

  // Must be called after creation but before any other methods are called.
  // Returns true on success. If false, no other functions should be called.
  bool Init(const base::FilePath& db_name);

  // Updates the database according to the changes recorded in `delta`.
  void ApplyDelta(const TopSitesDelta& delta);

  // Returns a list of all URLs currently in the table.
  MostVisitedURLList GetSites();

  sql::Database* db_for_testing();

  int GetURLRankForTesting(const MostVisitedURL& url);

  bool RemoveURLNoTransactionForTesting(const MostVisitedURL& url);

 private:
  // Sets a top site for the URL. `new_rank` is the position of the URL in the
  // list of top sites, zero-based.
  // If the URL is not in the table, adds it. If it is, updates its rank and
  // shifts the ranks of other URLs if necessary. Should be called within an
  // open transaction.
  void SetSiteNoTransaction(const MostVisitedURL& url, int new_rank)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Adds a new URL to the database.
  void AddSite(const MostVisitedURL& url, int new_rank)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Updates title and redirects of a URL that's already in the database.
  // Returns true if the database query succeeds.
  bool UpdateSite(const MostVisitedURL& url)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Returns `url`'s current rank or kRankOfNonExistingURL if not present.
  int GetURLRank(const MostVisitedURL& url)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Sets the rank for a given URL. The URL must be in the database. Should be
  // called within an open transaction.
  void UpdateSiteRankNoTransaction(const MostVisitedURL& url, int new_rank)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Removes the record for this URL. Returns false iff there is a failure in
  // running the statement. Should be called within an open transaction.
  bool RemoveURLNoTransaction(const MostVisitedURL& url)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Helper function to implement internals of Init().  This allows
  // Init() to retry in case of failure, since some failures will
  // invoke recovery code.
  bool InitImpl(const base::FilePath& db_name)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  [[nodiscard]] bool UpgradeToVersion5(sql::MetaTable&)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DatabaseErrorCallback(const base::FilePath& db_path,
                             int extended_error,
                             sql::Statement* stmt);

  SEQUENCE_CHECKER(sequence_checker_);

  // If recovery is attempted during one of the preliminary open attempts, the
  // database should be checked for broken constraints. See comment in the
  // DatabaseErrorCallback for more details.
  bool needs_fixing_up_ = false;

  std::unique_ptr<sql::Database> db_ GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_DATABASE_H_
