// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/db_data_owner.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "net/nqe/effective_connection_type.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
class TimeDelta;
}  // namespace base

namespace data_reduction_proxy {

class DataReductionProxyCompressionStats;
class DataReductionProxySettings;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the UI thread.
class DataReductionProxyService
    : public data_use_measurement::DataUseMeasurement::ServicesDataUseObserver {
 public:
  // The caller must ensure that |settings|, |prefs|, |request_context|, and
  // |io_task_runner| remain alive for the lifetime of the
  // |DataReductionProxyService| instance. |prefs| may be null. This instance
  // will take ownership of |compression_stats|.
  // TODO(jeremyim): DataReductionProxyService should own
  // DataReductionProxySettings and not vice versa.
  DataReductionProxyService(
      DataReductionProxySettings* settings,
      PrefService* prefs,
      std::unique_ptr<DataStore> store,
      data_use_measurement::DataUseMeasurement* data_use_measurement,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::TimeDelta& commit_delay);

  DataReductionProxyService(const DataReductionProxyService&) = delete;
  DataReductionProxyService& operator=(const DataReductionProxyService&) =
      delete;

  virtual ~DataReductionProxyService();

  void Shutdown();

  // Records data usage per host.
  // Virtual for testing.
  virtual void UpdateDataUseForHost(int64_t network_bytes,
                                    int64_t original_bytes,
                                    const std::string& host);

  // Records daily data savings statistics in |compression_stats_|.
  // Virtual for testing.
  virtual void UpdateContentLengths(
      int64_t data_used,
      int64_t original_size,
      bool data_reduction_proxy_enabled,
      const std::string& mime_type,
      bool is_user_traffic,
      data_use_measurement::DataUseUserData::DataUseContentType content_type,
      int32_t service_hash_code);

  // Records whether the Data Reduction Proxy is unreachable or not.
  void SetUnreachable(bool unreachable);

  // Stores an int64_t value in |prefs_|.
  void SetInt64Pref(const std::string& pref_path, int64_t value);

  // Stores a string value in |prefs_|.
  void SetStringPref(const std::string& pref_path, const std::string& value);

  void LoadHistoricalDataUsage(
      HistoricalDataUsageCallback load_data_usage_callback);
  void LoadCurrentDataUsageBucket(
      LoadCurrentDataUsageCallback load_current_data_usage_callback);
  void StoreCurrentDataUsageBucket(std::unique_ptr<DataUsageBucket> current);
  void DeleteHistoricalDataUsage();
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  void SetSettingsForTesting(DataReductionProxySettings* settings) {
    settings_ = settings;
  }

  // Returns the percentage of data savings estimate provided by save-data for
  // an origin.
  double GetSaveDataSavingsPercentEstimate(const std::string& origin) const;

  // Accessor methods.
  DataReductionProxyCompressionStats* compression_stats() const {
    return compression_stats_.get();
  }

  base::WeakPtr<DataReductionProxyService> GetWeakPtr();

  base::SequencedTaskRunner* GetDBTaskRunnerForTesting() const {
    return db_task_runner_.get();
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           MultipleAuthFailures);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           ValidatePersistedClientConfig);

  void OnServicesDataUse(int32_t service_hash_code,
                         int64_t recv_bytes,
                         int64_t sent_bytes) override;

  // Tracks compression statistics to be displayed to the user.
  std::unique_ptr<DataReductionProxyCompressionStats> compression_stats_;

  DataReductionProxySettings* settings_;

  // A prefs service for storing data.
  PrefService* prefs_;

  std::unique_ptr<DBDataOwner> db_data_owner_;

  // Used to post tasks to |db_data_owner_|.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // Must be accessed on UI thread. Guaranteed to be non-null during the
  // lifetime of |this|.
  data_use_measurement::DataUseMeasurement* data_use_measurement_;

  // Dictionary of save-data savings estimates by origin.
  const absl::optional<base::Value> save_data_savings_estimate_dict_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_{this};
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
