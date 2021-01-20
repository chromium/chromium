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
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_util.h"
#include "components/data_reduction_proxy/core/browser/db_data_owner.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
class TimeDelta;
}  // namespace base

namespace net {
class HttpRequestHeaders;
}  // namespace net

namespace data_reduction_proxy {

class DataReductionProxyCompressionStats;
class DataReductionProxyConfigServiceClient;
class DataReductionProxyRequestOptions;
class DataReductionProxySettings;

// Contains and initializes all Data Reduction Proxy objects that have a
// lifetime based on the UI thread.
class DataReductionProxyService
    : public data_use_measurement::DataUseMeasurement::ServicesDataUseObserver,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver,
      public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver,
      public network::NetworkConnectionTracker::NetworkConnectionObserver {
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
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DataStore> store,
      network::NetworkQualityTracker* network_quality_tracker,
      network::NetworkConnectionTracker* network_connection_tracker,
      data_use_measurement::DataUseMeasurement* data_use_measurement,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::TimeDelta& commit_delay,
      Client client,
      const std::string& channel,
      const std::string& user_agent);

  ~DataReductionProxyService() override;

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
      DataReductionProxyRequestType request_type,
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

  // Bridge methods to safely call to the UI thread objects.
  // Virtual for testing.
  virtual void SetProxyPrefs(bool enabled, bool at_startup);

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

  // Returns the current network quality estimates.
  net::EffectiveConnectionType GetEffectiveConnectionType() const;
  base::Optional<base::TimeDelta> GetHttpRttEstimate() const;

  network::mojom::ConnectionType GetConnectionType() const;

  // Sends the given |headers| to |DataReductionProxySettings|.
  void UpdateProxyRequestHeaders(const net::HttpRequestHeaders& headers);

  // Sends the given |prefetch_proxies| to |DataReductionProxySettings|.
  void UpdatePrefetchProxyHosts(const std::vector<GURL>& prefetch_proxies);

  // Adds a config client that can be used to update Data Reduction Proxy
  // settings.
  void AddCustomProxyConfigClient(
      mojo::Remote<network::mojom::CustomProxyConfigClient> config_client);

  // Returns the percentage of data savings estimate provided by save-data for
  // an origin.
  double GetSaveDataSavingsPercentEstimate(const std::string& origin) const;

  // Accessor methods.
  DataReductionProxyCompressionStats* compression_stats() const {
    return compression_stats_.get();
  }

  std::unique_ptr<network::PendingSharedURLLoaderFactory>
  pending_url_loader_factory() const {
    return url_loader_factory_->Clone();
  }


  DataReductionProxyConfigServiceClient* config_client() const {
    return config_client_.get();
  }

  DataReductionProxyRequestOptions* request_options() const {
    return request_options_.get();
  }

  // The production channel of this build.
  std::string channel() const { return channel_; }

  // The Client type of this build.
  Client client() const { return client_; }

  base::WeakPtr<DataReductionProxyService> GetWeakPtr();

  base::SequencedTaskRunner* GetDBTaskRunnerForTesting() const {
    return db_task_runner_.get();
  }

  void SetDependenciesForTesting(
      std::unique_ptr<DataReductionProxyRequestOptions> request_options,
      std::unique_ptr<DataReductionProxyConfigServiceClient> config_client);

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           MultipleAuthFailures);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigServiceClientTest,
                           ValidatePersistedClientConfig);

  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override;

  // Loads the Data Reduction Proxy configuration from |prefs_| and applies it.
  void ReadPersistedClientConfig();

  void OnServicesDataUse(int32_t service_hash_code,
                         int64_t recv_bytes,
                         int64_t sent_bytes) override;

  // NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Called when the list of proxies changes.
  void OnProxyConfigUpdated();

  // Stores a serialized Data Reduction Proxy configuration in preferences
  // storage.
  void StoreSerializedConfig(const std::string& serialized_config);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

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
  network::NetworkQualityTracker* network_quality_tracker_;
  network::NetworkConnectionTracker* network_connection_tracker_;

  // Must be accessed on UI thread. Guaranteed to be non-null during the
  // lifetime of |this|.
  data_use_measurement::DataUseMeasurement* data_use_measurement_;

  // Current network quality estimates.
  net::EffectiveConnectionType effective_connection_type_;
  base::Optional<base::TimeDelta> http_rtt_;

  network::mojom::ConnectionType connection_type_ =
      network::mojom::ConnectionType::CONNECTION_UNKNOWN;

  // The type of Data Reduction Proxy client.
  const Client client_;


  // Constructs credentials suitable for authenticating the client.
  std::unique_ptr<DataReductionProxyRequestOptions> request_options_;

  // Requests new Data Reduction Proxy configurations from a remote service.
  // May be null.
  std::unique_ptr<DataReductionProxyConfigServiceClient> config_client_;

  // The production channel of this build.
  const std::string channel_;

  // Dictionary of save-data savings estimates by origin.
  const base::Optional<base::Value> save_data_savings_estimate_dict_;

  // The set of clients that will get updates about changes to the proxy config.
  mojo::RemoteSet<network::mojom::CustomProxyConfigClient>
      proxy_config_clients_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
