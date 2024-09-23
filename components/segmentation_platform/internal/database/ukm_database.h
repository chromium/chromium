// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace segmentation_platform {

// UKM database is a single instance for each browser process. This class will
// be used for the storage and query of UKM data, for all the segmentation
// platform service(s).
class UkmDatabase {
 public:
  UkmDatabase() = default;
  virtual ~UkmDatabase() = default;

  UkmDatabase(const UkmDatabase&) = delete;
  UkmDatabase& operator=(const UkmDatabase&) = delete;

  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Initialize the database.
  virtual void InitDatabase(SuccessCallback callback) = 0;

  // Called once when an UKM event is added. All metrics from the event will be
  // added to the metrics table.
  virtual void StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry) = 0;

  // Called when URL for a source ID is updated. Only validated URLs will be
  // written to the URL table. The URL can be validated either by setting
  // |is_validated| to true when calling this method or by calling
  // OnUrlValidated() at a later point in time. Note that this method needs to
  // be called even when |is_validated| is false so that the database is able to
  // index metrics with the same |source_id| with the URL.
  virtual void UpdateUrlForUkmSource(ukm::SourceId source_id,
                                     const GURL& url,
                                     bool is_validated,
                                     const std::string& profile_id) = 0;

  // Called to validate an URL, see also UpdateUrlForUkmSource(). Safe to call
  // with unneeded URLs, since the database will only persist the URLs already
  // pending for known |source_id|s. Note that this call will not automatically
  // validate future URLs given by UpdateUrlForUkmSource(). They need to have
  // |is_validated| set to be persisted.
  virtual void OnUrlValidated(const GURL& url,
                              const std::string& profile_id) = 0;

  // Removes all the URLs from URL table and all the associated metrics in
  // metrics table, on best effort. Any new metrics added with the URL will
  // still be stored in metrics table (without URLs). If `all_urls` is true,
  // then clears all the URLs without using `urls` list. It is an optimization
  // to clear all the URLs quickly.
  virtual void RemoveUrls(const std::vector<GURL>& urls, bool all_urls) = 0;

  // Called once when a new UMA metric is to be recorded in the database.
  virtual void AddUmaMetric(const std::string& profile_id,
                            const UmaMetricEntry& row) = 0;

  // Struct responsible for storing a sql query and its bind values.
  struct CustomSqlQuery {
    CustomSqlQuery();
    CustomSqlQuery(CustomSqlQuery&&);
    CustomSqlQuery(std::string_view query,
                   const std::vector<processing::ProcessedValue>& bind_values);
    ~CustomSqlQuery();

    bool operator==(const CustomSqlQuery& rhs) const {
      return query == rhs.query && bind_values == rhs.bind_values;
    }
    CustomSqlQuery& operator=(CustomSqlQuery&&) = default;

    std::string query;
    std::vector<processing::ProcessedValue> bind_values;
  };

  using QueryList = base::flat_map<processing::FeatureIndex, CustomSqlQuery>;
  using QueryCallback =
      base::OnceCallback<void(bool success, processing::IndexedTensors)>;

  // Called to query data from the ukm database. The result is returned in the
  // |callback| as a mapping of indexed vectors of processing::ProcessedValue.
  virtual void RunReadOnlyQueries(QueryList&& queries,
                                  QueryCallback callback) = 0;

  // Removes metrics older than or equal to the given `time` from the database.
  // URLs are removed when there are no references to the metrics.
  virtual void DeleteEntriesOlderThan(base::Time time) = 0;

  // Cleans up old items from the database. Only cleans up UMA entries. UKM
  // entries still uses `DeleteEntriesOlderThan()` instead.
  virtual void CleanupItems(const std::string& profile_id,
                            std::vector<CleanupItem> cleanup_items) = 0;

  virtual void CommitTransactionForTesting() = 0;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_
