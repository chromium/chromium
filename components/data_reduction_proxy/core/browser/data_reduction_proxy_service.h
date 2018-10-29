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
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_metrics.h"
#include "components/data_reduction_proxy/core/browser/db_data_owner.h"
#include "components/data_use_measurement/core/data_use_measurement.h"
#include "components/data_use_measurement/core/data_use_user_data.h"
#include "net/nqe/effective_connection_type.h"
#include "services/network/public/cpp/network_connection_tracker.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

class PrefService;

namespace base {
class SequencedTaskRunner;
class SingleThreadTaskRunner;
class TimeDelta;
}

namespace net {
class HttpRequestHeaders;
class URLRequestContextGetter;
}

namespace data_reduction_proxy {

class DataReductionProxyCompressionStats;
class DataReductionProxyIOData;
class DataReductionProxyPingbackClient;
class DataReductionProxyServiceObserver;
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
      net::URLRequestContextGetter* request_context_getter,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      std::unique_ptr<DataStore> store,
      std::unique_ptr<DataReductionProxyPingbackClient> pingback_client,
      network::NetworkQualityTracker* network_quality_tracker,
      network::NetworkConnectionTracker* network_connection_tracker,
      data_use_measurement::DataUseMeasurement* data_use_measurement,
      const scoped_refptr<base::SequencedTaskRunner>& ui_task_runner,
      const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& db_task_runner,
      const base::TimeDelta& commit_delay);

  ~DataReductionProxyService() override;

  // Sets the DataReductionProxyIOData weak pointer.
  void SetIOData(base::WeakPtr<DataReductionProxyIOData> io_data);

  void Shutdown();

  // Indicates whether |this| has been fully initialized. |SetIOData| is the
  // final step in initialization.
  bool Initialized() const;

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
      const HistoricalDataUsageCallback& load_data_usage_callback);
  void LoadCurrentDataUsageBucket(
      const LoadCurrentDataUsageCallback& load_current_data_usage_callback);
  void StoreCurrentDataUsageBucket(std::unique_ptr<DataUsageBucket> current);
  void DeleteHistoricalDataUsage();
  void DeleteBrowsingHistory(const base::Time& start, const base::Time& end);

  // Methods for adding/removing observers on |this|.
  void AddObserver(DataReductionProxyServiceObserver* observer);
  void RemoveObserver(DataReductionProxyServiceObserver* observer);

  // Sets the reporting fraction in the pingback client.
  void SetPingbackReportingFraction(float pingback_reporting_fraction);

  // Notifies |this| that the user has requested to clear the browser
  // cache. This method is not called if only a subset of site entries are
  // cleared.
  void OnCacheCleared(const base::Time start, const base::Time end);

  // When triggering previews, prevent long term black list rules.
  void SetIgnoreLongTermBlackListRules(bool ignore_long_term_black_list_rules);

  // Returns the current network quality estimates.
  net::EffectiveConnectionType GetEffectiveConnectionType() const;
  base::Optional<base::TimeDelta> GetHttpRttEstimate() const;

  network::mojom::ConnectionType GetConnectionType() const;

  // Sends the given |headers| to |DataReductionProxySettings|.
  void SetProxyRequestHeadersOnUI(const net::HttpRequestHeaders& headers);

  // Sends the given |proxies| to |DataReductionProxySettings|.
  void SetConfiguredProxiesOnUI(const net::ProxyList& proxies);

  // Sets a config client that can be used to update Data Reduction Proxy
  // settings when the network service is enabled.
  void SetCustomProxyConfigClient(
      network::mojom::CustomProxyConfigClientPtrInfo config_client_info);

  // Accessor methods.
  DataReductionProxyCompressionStats* compression_stats() const {
    return compression_stats_.get();
  }

  net::URLRequestContextGetter* url_request_context_getter() const {
    return url_request_context_getter_;
  }

  std::unique_ptr<network::SharedURLLoaderFactoryInfo> url_loader_factory_info()
      const {
    return url_loader_factory_->Clone();
  }

  DataReductionProxyPingbackClient* pingback_client() const {
    return pingback_client_.get();
  }

  base::WeakPtr<DataReductionProxyService> GetWeakPtr();

 private:
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxySettingsTest,
                           TestLoFiSessionStateHistograms);

  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  void OnRTTOrThroughputEstimatesComputed(
      base::TimeDelta http_rtt,
      base::TimeDelta transport_rtt,
      int32_t downstream_throughput_kbps) override;

  // Loads the Data Reduction Proxy configuration from |prefs_| and applies it.
  void ReadPersistedClientConfig();

  void OnServicesDataUse(int64_t recv_bytes, int64_t sent_bytes) override;

  // NetworkConnectionTracker::NetworkConnectionObserver
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  net::URLRequestContextGetter* url_request_context_getter_;
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;

  // Tracks compression statistics to be displayed to the user.
  std::unique_ptr<DataReductionProxyCompressionStats> compression_stats_;

  std::unique_ptr<DataReductionProxyPingbackClient> pingback_client_;

  DataReductionProxySettings* settings_;

  // A prefs service for storing data.
  PrefService* prefs_;

  std::unique_ptr<DBDataOwner> db_data_owner_;

  // Used to post tasks to |io_data_|.
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  // Used to post tasks to |db_data_owner_|.
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // A weak pointer to DataReductionProxyIOData so that UI based objects can
  // make calls to IO based objects.
  base::WeakPtr<DataReductionProxyIOData> io_data_;

  base::ObserverList<DataReductionProxyServiceObserver>::Unchecked
      observer_list_;

  bool initialized_;

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

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<DataReductionProxyService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyService);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_SERVICE_H_
