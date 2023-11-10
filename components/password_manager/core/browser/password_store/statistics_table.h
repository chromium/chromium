// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STATISTICS_TABLE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STATISTICS_TABLE_H_

#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"

namespace sql {
class Database;
}

namespace password_manager {

// Represents the 'stats' table in the Login Database.
class StatisticsTable {
 public:
  StatisticsTable();

  StatisticsTable(const StatisticsTable&) = delete;
  StatisticsTable& operator=(const StatisticsTable&) = delete;

  ~StatisticsTable();

  // Initializes |db_|.
  void Init(sql::Database* db);

  // Creates the statistics table if it doesn't exist.
  bool CreateTableIfNecessary();

  // Migrates this table to |version|. The current version should be less than
  // |version|. Returns false if there was migration work to do and it failed,
  // true otherwise.
  bool MigrateToVersion(int version);

  // Adds or replaces the statistics about |stats.origin_domain| and
  // |stats.username_value|.
  bool AddRow(const InteractionsStats& stats);

  // Removes the statistics for |domain|. Returns true if the SQL completed
  // successfully.
  bool RemoveRow(const GURL& domain);

  // Returns the statistics for |domain| if it exists.
  std::vector<InteractionsStats> GetRows(const GURL& domain);

  // Removes the statistics between the dates. If |origin_filter| is not null,
  // only statistics for matching origins are removed. Returns true if the SQL
  // completed successfully.
  bool RemoveStatsByOriginAndTime(
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter,
      base::Time delete_begin,
      base::Time delete_end);

  // Returns the number of rows (origin/username pairs) in the table.
  int GetNumAccounts();

  // Returns all statistics from the database.
  std::vector<InteractionsStats> GetAllRowsForTest();

 private:
  raw_ptr<sql::Database> db_ = nullptr;
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_PASSWORD_STORE_STATISTICS_TABLE_H_
