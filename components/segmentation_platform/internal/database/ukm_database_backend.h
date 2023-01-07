// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "components/segmentation_platform/internal/database/ukm_database.h"
#include "components/segmentation_platform/internal/database/ukm_metrics_table.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/database/ukm_url_table.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "sql/database.h"
#include "url/gurl.h"

namespace segmentation_platform {

// Database backend class that handles various tables in the SQL database. Runs
// in database task runner.
class UkmDatabaseBackend : public UkmDatabase {
 public:
  UkmDatabaseBackend(
      const base::FilePath& database_path,
      scoped_refptr<base::SequencedTaskRunner> callback_task_runner);
  ~UkmDatabaseBackend() override;

  UkmDatabaseBackend(const UkmDatabaseBackend&) = delete;
  UkmDatabaseBackend& operator=(const UkmDatabaseBackend&) = delete;

  // UkmDatabase implementation. All callbacks will be posted to
  // |callback_task_runner|.
  void InitDatabase(SuccessCallback callback) override;
  void StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) override;
  void UpdateUrlForUkmSource(ukm::SourceId source_id,
                             const GURL& url,
                             bool is_validated) override;
  void OnUrlValidated(const GURL& url) override;
  void RemoveUrls(const std::vector<GURL>& urls, bool all_urls) override;
  void RunReadonlyQueries(QueryList&& queries, QueryCallback callback) override;
  void DeleteEntriesOlderThan(base::Time time) override;

  sql::Database& db() { return db_; }

  UkmUrlTable& url_table_for_testing() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return url_table_;
  }

  base::WeakPtr<UkmDatabaseBackend> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  // Helper to delete all URLs from database.
  void DeleteAllUrls();

  const base::FilePath database_path_;
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_
      GUARDED_BY_CONTEXT(sequence_checker_);
  sql::Database db_ GUARDED_BY_CONTEXT(sequence_checker_);
  UkmMetricsTable metrics_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  UkmUrlTable url_table_ GUARDED_BY_CONTEXT(sequence_checker_);
  enum class Status { CREATED, INIT_FAILED, INIT_SUCCESS };
  Status status_ = Status::CREATED;

  // Map from source ID to URL. When URL updates are sent before metrics, this
  // map is used to set URL ID to the metrics rows. This is an in-memory cache
  // which lives during the current session. UKM does not reuse source ID across
  // sessions.
  // TODO(ssid): This map should be cleaned and not grow forever.
  base::flat_map<ukm::SourceId, UrlId> source_to_url_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // List of URL IDs that are needed by UKM metrics, but are not yet validated.
  // Used to verify that URLs are needed in the UKM database when a
  // OnUrlValidated() is called. The map is kept around only for the current
  // session, so we might miss validation calls that happen across sessions.
  // But, usually history updates within the session.
  base::flat_set<UrlId> urls_not_validated_
      GUARDED_BY_CONTEXT(sequence_checker_);
  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<UkmDatabaseBackend> weak_factory_{this};
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_BACKEND_H_
