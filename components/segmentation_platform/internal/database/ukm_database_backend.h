// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "components/segmentation_platform/internal/database/uma_metrics_table.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "sql/database.h"
#include "sql/transaction.h"
#include "url/gurl.h"

namespace segmentation_platform {

// Database backend class that handles various tables in the SQL database. This
// class must be created, operated, and destroyed on a task runner that is
// allowed to do blocking operations. It's intended to be used as a
// `base::SequenceBound` object.
class UkmDatabaseBackend {
 public:
  UkmDatabaseBackend(const base::FilePath& database_path, bool in_memory);
  ~UkmDatabaseBackend();

  UkmDatabaseBackend(const UkmDatabaseBackend&) = delete;
  UkmDatabaseBackend& operator=(const UkmDatabaseBackend&) = delete;

  // Initialize the database. Returns true on success.
  bool InitDatabase();

  void StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry);
  void UpdateUrlForUkmSource(ukm::SourceId source_id,
                             const GURL& url,
                             bool is_validated,
                             const std::string& profile_id);
  void OnUrlValidated(const GURL& url, const std::string& profile_id);
  void RemoveUrls(const std::vector<GURL>& urls, bool all_urls);
  void AddUmaMetric(const std::string& profile_id, const UmaMetricEntry& row);

  // Run read-only queries and return the result. Returns std::nullopt on
  // failure.
  std::optional<processing::IndexedTensors> RunReadOnlyQueries(
      UkmDatabase::QueryList queries);

  void CleanupOldEntries(base::Time ukm_time_limit, base::Time uma_time_limit);
  void CleanupItems(const std::string& profile_id,
                    std::vector<CleanupItem> cleanup_items);
  void CommitTransactionForTesting();

  sql::Database& db() { return db_; }

  UkmUrlTable& url_table_for_testing() { return url_table_; }

  bool has_transaction_for_testing() const { return !!current_transaction_; }

  void RollbackTransactionForTesting();

 private:
  // Helper to delete all URLs from database.
  void DeleteAllUrls();

  // Tracks changes in the current transaction and commits when over a limit.
  void TrackChangesInTransaction(int change_count);

  // Commit current transaction and begin a new one.
  void RestartTransaction();

  const base::FilePath database_path_;
  const bool in_memory_;
  sql::Database db_;
  int change_count_ = 0;
  std::unique_ptr<sql::Transaction> current_transaction_;
  bool inhibit_transaction_;

  UkmMetricsTable metrics_table_;
  UkmUrlTable url_table_;
  UmaMetricsTable uma_metrics_table_;
  enum class Status { CREATED, INIT_FAILED, INIT_SUCCESS };
  Status status_ = Status::CREATED;

  // Map from source ID to URL. When URL updates are sent before metrics, this
  // map is used to set URL ID to the metrics rows. This is an in-memory cache
  // which lives during the current session. UKM does not reuse source ID across
  // sessions.
  // TODO(ssid): This map should be cleaned and not grow forever.
  base::flat_map<ukm::SourceId, UrlId> source_to_url_;

  // List of URL IDs that are needed by UKM metrics, but are not yet validated.
  // Used to verify that URLs are needed in the UKM database when a
  // OnUrlValidated() is called. The map is kept around only for the current
  // session, so we might miss validation calls that happen across sessions.
  // But, usually history updates within the session.
  base::flat_set<UrlId> urls_not_validated_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_
