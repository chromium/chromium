// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/secure_proxy_checker.h"
#include "components/data_reduction_proxy/core/browser/warmup_url_fetcher.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_retry_info.h"
#include "services/network/public/cpp/network_connection_tracker.h"

namespace net {
class ProxyServer;
}  // namespace net

namespace data_reduction_proxy {

class DataReductionProxyConfigValues;
class DataReductionProxyConfigurator;
class NetworkPropertiesManager;
class SecureProxyChecker;
struct DataReductionProxyTypeInfo;

// Values of the UMA DataReductionProxy.ProbeURL histogram.
// This enum must remain synchronized with
// DataReductionProxyProbeURLFetchResult in metrics/histograms/histograms.xml.
enum SecureProxyCheckFetchResult {
  // The secure proxy check failed because the Internet was disconnected.
  INTERNET_DISCONNECTED = 0,

  // The secure proxy check failed for any other reason, and as a result, the
  // proxy was disabled.
  FAILED_PROXY_DISABLED,

  // The secure proxy check failed, but the proxy was already restricted.
  FAILED_PROXY_ALREADY_DISABLED,

  // The secure proxy check succeeded, and as a result the proxy was restricted.
  SUCCEEDED_PROXY_ENABLED,

  // The secure proxy check succeeded, but the proxy was already restricted.
  SUCCEEDED_PROXY_ALREADY_ENABLED,

  // The secure proxy has been disabled on a network change until the check
  // succeeds.
  PROXY_DISABLED_BEFORE_CHECK,

  // This must always be last.
  SECURE_PROXY_CHECK_FETCH_RESULT_COUNT
};

// Central point for holding the Data Reduction Proxy configuration.
// This object lives on the IO thread and all of its methods are expected to be
// called from there.
class DataReductionProxyConfig
    : public network::NetworkConnectionTracker::NetworkConnectionObserver {
 public:
  // The caller must ensure that all parameters remain alive for the lifetime
  // of the |DataReductionProxyConfig| instance, with the exception of
  // |config_values| which is owned by |this|. |config_values| contains the Data
  // Reduction Proxy configuration values. |configurator| is the target for a
  // configuration update.
  DataReductionProxyConfig(
      network::NetworkConnectionTracker* network_connection_tracker,
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      DataReductionProxyConfigurator* configurator);
  ~DataReductionProxyConfig() override;

  // Performs initialization on the IO thread.
  // |url_loader_factory| is the network::URLLoaderFactory instance used for
  // making URL requests. The requests disable the use of proxies.
  void Initialize(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      WarmupURLFetcher::CreateCustomProxyConfigCallback
          create_custom_proxy_config_callback,
      NetworkPropertiesManager* manager,
      const std::string& user_agent);

  // Sets the proxy configs, enabling or disabling the proxy according to
  // the value of |enabled|. If |restricted| is true, only enable the fallback
  // proxy. |at_startup| is true when this method is called from
  // InitDataReductionProxySettings.
  void SetProxyConfig(bool enabled, bool at_startup);

  // If the specified |proxy_server| matches a Data Reduction Proxy, returns the
  // DataReductionProxyTypeInfo showing where that proxy is in the list of
  // configured proxies, otherwise returns an empty optional value.
  base::Optional<DataReductionProxyTypeInfo> FindConfiguredDataReductionProxy(
      const net::ProxyServer& proxy_server) const;

  // Gets a list of all the configured proxies. These are the same proxies that
  // will be used if FindConfiguredDataReductionProxy() is called.
  net::ProxyList GetAllConfiguredProxies() const;

  // Returns true if the proxy is on the retry map and the retry delay is not
  // expired. If proxy is bypassed, retry_delay (if not NULL) returns the delay
  // of proxy_server. If proxy is not bypassed, retry_delay is not assigned.
  bool IsProxyBypassed(const net::ProxyRetryInfoMap& retry_map,
                       const net::ProxyServer& proxy_server,
                       base::TimeDelta* retry_delay) const;

  // Check whether the |proxy_rules| contain any of the data reduction proxies.
  virtual bool ContainsDataReductionProxy(
      const net::ProxyConfig::ProxyRules& proxy_rules) const;

  // Returns true if the data saver has been enabled by the user, and the data
  // saver proxy is reachable.
  bool enabled_by_user_and_reachable() const;

  // Gets the ProxyConfig that would be used ignoring the holdback experiment.
  // This should only be used for logging purposes.
  net::ProxyConfig ProxyConfigIgnoringHoldback() const;

  std::vector<DataReductionProxyServer> GetProxiesForHttp() const;

  // Called when a new client config has been fetched.
  void OnNewClientConfigFetched();

  void SetNetworkPropertiesManagerForTesting(NetworkPropertiesManager* manager);

  // Returns the network properties manager which manages whether a given data
  // saver proxy is currently allowed or not.
  const NetworkPropertiesManager& GetNetworkPropertiesManager() const;

  // Returns the details of the proxy to which the warmup URL probe is
  // in-flight. Returns base::nullopt if no warmup probe is in-flight.
  // Virtualized for testing.
  virtual base::Optional<
      std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
  GetInFlightWarmupProxyDetails() const;

#if defined(OS_CHROMEOS)
  // Enables getting the network id asynchronously when
  // GatherEstimatesForNextConnectionType(). This should always be called in
  // production, because getting the network id involves a blocking call to
  // recv() in AddressTrackerLinux, and the IO thread should never be blocked.
  // TODO(https://crbug.com/821607): Remove after the bug is resolved.
  void EnableGetNetworkIdAsynchronously();
#endif  // defined(OS_CHROMEOS)
  // Called when there is a change in the HTTP RTT estimate.
  void OnRTTOrThroughputEstimatesComputed(base::TimeDelta http_rtt);

  // Returns the current HTTP RTT estimate.
  base::Optional<base::TimeDelta> GetHttpRttEstimate() const;

  // Updates the Data Reduction Proxy configurator with the current config.
  void UpdateConfigForTesting(bool enabled,
                              bool secure_proxies_allowed,
                              bool insecure_proxies_allowed);

 protected:
  virtual base::TimeTicks GetTicksNow() const;

  // Returns the ID of the current network by calling the platform APIs.
  virtual std::string GetCurrentNetworkID() const;

  // Callback that is executed when the warmup URL fetch is complete.
  // |proxy_server| is the proxy server over which the warmup URL was fetched.
  // |success_response| is true if the fetching of the URL was successful or
  // not.
  void HandleWarmupFetcherResponse(
      const net::ProxyServer& proxy_server,
      WarmupURLFetcher::FetchResult success_response);

  // Returns the details of the proxy to which the next warmup URL probe should
  // be sent to.
  base::Optional<DataReductionProxyServer> GetProxyConnectionToProbe() const;

  // Returns true if a warmup URL probe is in-flight. Virtualized for testing.
  virtual bool IsFetchInFlight() const;

  // Returns the number of previous attempt counts for the proxy that is going
  // to be probed. Virtualized for testing.
  virtual size_t GetWarmupURLFetchAttemptCounts() const;

 private:
  friend class MockDataReductionProxyConfig;
  friend class TestDataReductionProxyConfig;
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           TestSetProxyConfigsHoldback);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, AreProxiesBypassed);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           AreProxiesBypassedRetryDelay);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest, WarmupURL);
  FRIEND_TEST_ALL_PREFIXES(DataReductionProxyConfigTest,
                           ShouldAcceptServerPreview);

  // Values of the estimated network quality at the beginning of the most
  // recent query of the Network quality estimate provider.
  enum NetworkQualityAtLastQuery {
    NETWORK_QUALITY_AT_LAST_QUERY_UNKNOWN,
    NETWORK_QUALITY_AT_LAST_QUERY_SLOW,
    NETWORK_QUALITY_AT_LAST_QUERY_NOT_SLOW
  };

  // Provides a mechanism for an external object to force |this| to refresh
  // the Data Reduction Proxy configuration from |config_values_| and apply to
  // |configurator_|. Used by the Data Reduction Proxy config service client.
  void ReloadConfig();

  // network::NetworkConnectionTracker::NetworkConnectionObserver:
  void OnConnectionChanged(network::mojom::ConnectionType type) override;

  // Invoked to continue network changed handling after the network id is
  // retrieved. If |get_network_id_asynchronously_| is set, the network id is
  // fetched on the worker thread. Otherwise, OnNetworkChanged calls this
  // directly. This is a workaround for https://crbug.com/821607 where
  // net::GetWifiSSID() call gets stuck.
  void ContinueNetworkChanged(const std::string& network_id);

  // Requests the secure proxy check URL. Upon completion, returns the results
  // to the caller via the |fetcher_callback|. Virtualized for unit testing.
  virtual void SecureProxyCheck(SecureProxyCheckerCallback fetcher_callback);

  // Parses the secure proxy check responses and appropriately configures the
  // Data Reduction Proxy rules.
  void HandleSecureProxyCheckResponse(const std::string& response,
                                      int net_status,
                                      int http_response_code);

  // Adds the default proxy bypass rules for the Data Reduction Proxy.
  // Virtualized for testing.
  virtual void AddDefaultProxyBypassRules();

  // Checks if all configured data reduction proxies are in the retry map.
  // Returns true if the request is bypassed by all configured data reduction
  // proxies that apply to the request scheme. If all possible data reduction
  // proxies are bypassed, returns the minimum retry delay of the bypassed data
  // reduction proxies in min_retry_delay (if not NULL). If there are no
  // bypassed data reduction proxies for the request scheme, returns false and
  // does not assign min_retry_delay.
  bool AreProxiesBypassed(const net::ProxyRetryInfoMap& retry_map,
                          const net::ProxyConfig::ProxyRules& proxy_rules,
                          bool is_https,
                          base::TimeDelta* min_retry_delay) const;


  // Checks if the current network has captive portal, and handles the result.
  // If the captive portal probe was blocked on the current network, disables
  // the use of secure proxies.
  void HandleCaptivePortal();

  // Returns true if the current network has captive portal. Virtualized for
  // testing.
  virtual bool GetIsCaptivePortal() const;

  // Fetches the warmup URL.
  void FetchWarmupProbeURL();

  // URL fetcher used for performing the secure proxy check. May be null.
  std::unique_ptr<SecureProxyChecker> secure_proxy_checker_;

  // URL fetcher used for fetching the warmup URL. May be null.
  std::unique_ptr<WarmupURLFetcher> warmup_url_fetcher_;

  bool unreachable_;
  bool enabled_by_user_;

  // Contains the configuration data being used.
  std::unique_ptr<DataReductionProxyConfigValues> config_values_;

#if defined(OS_CHROMEOS)
  // Whether the network id should be obtained on a worker thread.
  bool get_network_id_asynchronously_ = false;
#endif

  // Watches for network connection changes.
  network::NetworkConnectionTracker* network_connection_tracker_;

  // The caller must ensure that the |configurator_| outlives this instance.
  DataReductionProxyConfigurator* configurator_;

  // Enforce usage on the IO thread.
  base::ThreadChecker thread_checker_;

  // The current connection type.
  network::mojom::ConnectionType connection_type_;

  // Stores the properties of the proxy which is currently being probed. The
  // values are valid only if a probe (or warmup URL) fetch is currently
  // in-flight.
  bool warmup_url_fetch_in_flight_secure_proxy_;
  bool warmup_url_fetch_in_flight_core_proxy_;

  // Should be accessed only on the IO thread. Guaranteed to be non-null during
  // the lifetime of |this| if accessed on the IO thread.
  NetworkPropertiesManager* network_properties_manager_;

  // Current HTTP RTT estimate.
  base::Optional<base::TimeDelta> http_rtt_;

  base::WeakPtrFactory<DataReductionProxyConfig> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfig);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_H_
