// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/metrics/public/mojom/ukm_interface.mojom.h"
#include "url/gurl.h"

namespace segmentation_platform {

class UkmDatabaseBackend;

// UKM database is a single instance for each browser process. This class will
// be used for the storage and query of UKM data, for all the segmentation
// platform service(s).
class UkmDatabase {
 public:
  explicit UkmDatabase(const base::FilePath& database_path);
  virtual ~UkmDatabase();

  UkmDatabase(UkmDatabase&) = delete;
  UkmDatabase& operator=(UkmDatabase&) = delete;

  // Initialize the database.
  virtual void InitDatabase();

  // Called once when an UKM event is added. All metrics from the event will be
  // added to the metrics table.
  virtual void StoreUkmEntry(ukm::mojom::UkmEntryPtr ukm_entry);

  // Called when URL for a source ID is updated. Only validated URLs will be
  // written to the URL table. The URL can be validated either by setting
  // |is_validated| to true when calling this method or by calling
  // OnUrlValidated() at a later point in time. Note that this method needs to
  // be called even when |is_validated| is false so that the database is able to
  // index metrics with the same |source_id| with the URL.
  virtual void UpdateUrlForUkmSource(ukm::SourceId source_id,
                                     const GURL& url,
                                     bool is_validated);

  // Called to validate an URL, see also UpdateUrlForUkmSource(). Safe to call
  // with unneeded URLs, since the database will only persist the URLs already
  // pending for known |source_id|s. Note that this call will not automatically
  // validate future URLs given by UpdateUrlForUkmSource(). They need to have
  // |is_validated| set to be persisted.
  virtual void OnUrlValidated(const GURL& url);

  // Removes all the URLs from URL table and all the associated metrics in
  // metrics table, on best effort. Any new metrics added with the URL will
  // still be stored in metrics table (without URLs).
  virtual void RemoveUrls(const std::vector<GURL>& urls);

  // Struct responsible for storing a sql query and its bind values.
  struct CustomSqlQuery {
    CustomSqlQuery();
    CustomSqlQuery(CustomSqlQuery&&);
    CustomSqlQuery(const base::StringPiece& query,
                   const std::vector<ProcessedValue>& bind_values);
    ~CustomSqlQuery();

    bool operator==(const CustomSqlQuery& rhs) const {
      return query == rhs.query && bind_values == rhs.bind_values;
    }
    CustomSqlQuery& operator=(CustomSqlQuery&&) = default;

    std::string query;
    std::vector<ProcessedValue> bind_values;
  };

  // TODO(haileywang): move all the using associated with feature processing to
  // one place.
  using FeatureIndex = int;
  using Tensor = std::vector<ProcessedValue>;
  using IndexedTensors = base::flat_map<FeatureIndex, Tensor>;
  using QueryList = base::flat_map<FeatureIndex, CustomSqlQuery>;
  using QueryCallback = base::OnceCallback<void(IndexedTensors)>;

  // Called to query data from the ukm database. The result is returned in the
  // |callback| as a mapping of indexed vectors of ProcessedValue.
  virtual void RunReadonlyQueries(const QueryList& queries,
                                  QueryCallback callback);

  // Removes metrics older than the given time from the database. URLs are
  // removed when there are no references to the metrics.
  virtual void DeleteEntriesOlderThan(base::Time time);

 private:
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  scoped_refptr<base::SequencedTaskRunner> backend_task_runner_;
  std::unique_ptr<UkmDatabaseBackend> backend_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_DATABASE_UKM_DATABASE_H_
