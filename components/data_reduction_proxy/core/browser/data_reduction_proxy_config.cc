// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/metrics/histogram_base.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/lazy_task_runner.h"
#include "base/task_runner_util.h"
#include "base/time/default_tick_clock.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_bypass_protocol.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/variations/variations_associated_data.h"
#include "net/base/host_port_pair.h"
#include "net/base/load_flags.h"
#include "net/base/network_change_notifier.h"
#include "net/base/network_interfaces.h"
#include "net/base/proxy_server.h"
#include "net/nqe/effective_connection_type.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if defined(OS_ANDROID)
#include "net/android/network_library.h"
#endif  // OS_ANDROID

using base::FieldTrialList;

namespace {

#if defined(OS_CHROMEOS)
// SequencedTaskRunner to get the network id. A SequencedTaskRunner is used
// rather than parallel tasks to avoid having many threads getting the network
// id concurrently.
base::LazySequencedTaskRunner g_get_network_id_task_runner =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::ThreadPool(),
                         base::MayBlock(),
                         base::TaskPriority::BEST_EFFORT,
                         base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN));
#endif

// Values of the UMA DataReductionProxy.NetworkChangeEvents histograms.
// This enum must remain synchronized with the enum of the same
// name in metrics/histograms/histograms.xml.
enum DataReductionProxyNetworkChangeEvent {
  // The client IP address changed.
  DEPRECATED_IP_CHANGED = 0,
  // [Deprecated] Proxy is disabled because a VPN is running.
  DEPRECATED_DISABLED_ON_VPN = 1,
  // There was a network change.
  NETWORK_CHANGED = 2,
  CHANGE_EVENT_COUNT = NETWORK_CHANGED + 1

};

// Key of the UMA DataReductionProxy.ProbeURL histogram.
const char kUMAProxyProbeURL[] = "DataReductionProxy.ProbeURL";

// Key of the UMA DataReductionProxy.ProbeURLNetError histogram.
const char kUMAProxyProbeURLNetError[] = "DataReductionProxy.ProbeURLNetError";

// Record a network change event.
void RecordNetworkChangeEvent(DataReductionProxyNetworkChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.NetworkChangeEvents", event,
                            CHANGE_EVENT_COUNT);
}

//  Records UMA containing the result of requesting the secure proxy check.
void RecordSecureProxyCheckFetchResult(
    data_reduction_proxy::SecureProxyCheckFetchResult result) {
  UMA_HISTOGRAM_ENUMERATION(
      kUMAProxyProbeURL, result,
      data_reduction_proxy::SECURE_PROXY_CHECK_FETCH_RESULT_COUNT);
}

enum class WarmupURLFetchAttemptEvent {
  kFetchInitiated = 0,
  kConnectionTypeNone = 1,
  kProxyNotEnabledByUser = 2,
  kWarmupURLFetchingDisabled = 3,
  kCount
};

void RecordWarmupURLFetchAttemptEvent(
    WarmupURLFetchAttemptEvent warmup_url_fetch_event) {
  DCHECK_GT(WarmupURLFetchAttemptEvent::kCount, warmup_url_fetch_event);
  UMA_HISTOGRAM_ENUMERATION("DataReductionProxy.WarmupURL.FetchAttemptEvent",
                            warmup_url_fetch_event,
                            WarmupURLFetchAttemptEvent::kCount);
}

// Returns the current connection type if known, otherwise returns
// CONNECTION_UNKNOWN.
network::mojom::ConnectionType GetConnectionType(
    network::NetworkConnectionTracker* network_connection_tracker) {
  auto connection_type = network::mojom::ConnectionType::CONNECTION_UNKNOWN;
  network_connection_tracker->GetConnectionType(&connection_type,
                                                base::DoNothing());
  return connection_type;
}

std::string DoGetCurrentNetworkID(
    network::NetworkConnectionTracker* network_connection_tracker) {
  // It is possible that the connection type changed between when
  // GetConnectionType() was called and when the API to determine the
  // network name was called. Check if that happened and retry until the
  // connection type stabilizes. This is an imperfect solution but should
  // capture majority of cases, and should not significantly affect estimates
  // (that are approximate to begin with).

  while (true) {
    auto connection_type = GetConnectionType(network_connection_tracker);
    std::string ssid_mccmnc;

    switch (connection_type) {
      case network::mojom::ConnectionType::CONNECTION_UNKNOWN:
      case network::mojom::ConnectionType::CONNECTION_NONE:
      case network::mojom::ConnectionType::CONNECTION_BLUETOOTH:
      case network::mojom::ConnectionType::CONNECTION_ETHERNET:
        break;
      case network::mojom::ConnectionType::CONNECTION_WIFI:
// Get WiFi SSID only on Android since calling it on non-Android
// platforms may result in hung IO loop. See https://crbg.com/896296.
#if defined(OS_ANDROID)
        ssid_mccmnc = net::GetWifiSSID();
#endif
        break;
      case network::mojom::ConnectionType::CONNECTION_2G:
      case network::mojom::ConnectionType::CONNECTION_3G:
      case network::mojom::ConnectionType::CONNECTION_4G:
#if defined(OS_ANDROID)
        ssid_mccmnc = net::android::GetTelephonyNetworkOperator();
#endif
        break;
    }

    if (connection_type == GetConnectionType(network_connection_tracker)) {
      if (connection_type >= network::mojom::ConnectionType::CONNECTION_2G &&
          connection_type <= network::mojom::ConnectionType::CONNECTION_4G) {
        // No need to differentiate cellular connections by the exact
        // connection type.
        return "cell," + ssid_mccmnc;
      }
      return base::NumberToString(static_cast<int>(connection_type)) + "," +
             ssid_mccmnc;
    }
  }
  NOTREACHED();
}

}  // namespace

namespace data_reduction_proxy {

DataReductionProxyConfig::DataReductionProxyConfig(
    network::NetworkConnectionTracker* network_connection_tracker,
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    DataReductionProxyConfigurator* configurator)
    : unreachable_(false),
      enabled_by_user_(false),
      config_values_(std::move(config_values)),
      network_connection_tracker_(network_connection_tracker),
      configurator_(configurator),
      connection_type_(network::mojom::ConnectionType::CONNECTION_UNKNOWN),
      network_properties_manager_(nullptr) {
  DCHECK(network_connection_tracker_);
  DCHECK(configurator);

  // Constructed on the UI thread, but should be checked on the IO thread.
  thread_checker_.DetachFromThread();
}

DataReductionProxyConfig::~DataReductionProxyConfig() {
  network_connection_tracker_->RemoveNetworkConnectionObserver(this);
}

void DataReductionProxyConfig::Initialize(
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    WarmupURLFetcher::CreateCustomProxyConfigCallback
        create_custom_proxy_config_callback,
    NetworkPropertiesManager* manager,
    const std::string& user_agent) {
  DCHECK(thread_checker_.CalledOnValidThread());
  network_properties_manager_ = manager;
  network_properties_manager_->ResetWarmupURLFetchMetrics();

  if (!params::IsIncludedInHoldbackFieldTrial()) {
    secure_proxy_checker_ =
        std::make_unique<SecureProxyChecker>(url_loader_factory);
    warmup_url_fetcher_ = std::make_unique<WarmupURLFetcher>(
        create_custom_proxy_config_callback,
        base::BindRepeating(
            &DataReductionProxyConfig::HandleWarmupFetcherResponse,
            base::Unretained(this)),
        base::BindRepeating(&DataReductionProxyConfig::GetHttpRttEstimate,
                            base::Unretained(this)),
        user_agent);
  }

  AddDefaultProxyBypassRules();

  network_connection_tracker_->AddNetworkConnectionObserver(this);
  network_connection_tracker_->GetConnectionType(
      &connection_type_,
      base::BindOnce(&DataReductionProxyConfig::OnConnectionChanged,
                     weak_factory_.GetWeakPtr()));
}

void DataReductionProxyConfig::OnNewClientConfigFetched() {
  DCHECK(thread_checker_.CalledOnValidThread());
  ReloadConfig();
  // Call ResetWarmupURLFetchMetrics to reset the counts since the list of
  // proxies may have changed.
  network_properties_manager_->ResetWarmupURLFetchMetrics();
  FetchWarmupProbeURL();
}

void DataReductionProxyConfig::ReloadConfig() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(configurator_);

  if (enabled_by_user_ && !params::IsIncludedInHoldbackFieldTrial() &&
      !config_values_->proxies_for_http().empty()) {
    configurator_->Enable(*network_properties_manager_,
                          config_values_->proxies_for_http());
  } else {
    configurator_->Disable();
  }
}

base::Optional<DataReductionProxyTypeInfo>
DataReductionProxyConfig::FindConfiguredDataReductionProxy(
    const net::ProxyServer& proxy_server) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_values_->FindConfiguredDataReductionProxy(proxy_server);
}

net::ProxyList DataReductionProxyConfig::GetAllConfiguredProxies() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return config_values_->GetAllConfiguredProxies();
}

bool DataReductionProxyConfig::AreProxiesBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyConfig::ProxyRules& proxy_rules,
    bool is_https,
    base::TimeDelta* min_retry_delay) const {
  // Data reduction proxy config is Type::PROXY_LIST_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME)
    return false;

  if (is_https)
    return false;

  const net::ProxyList* proxies =
      proxy_rules.MapUrlSchemeToProxyList(url::kHttpScheme);

  if (!proxies)
    return false;

  base::TimeDelta min_delay = base::TimeDelta::Max();
  bool bypassed = false;

  for (const net::ProxyServer& proxy : proxies->GetAll()) {
    if (!proxy.is_valid() || proxy.is_direct())
      continue;

    base::TimeDelta delay;
    if (FindConfiguredDataReductionProxy(proxy)) {
      if (!IsProxyBypassed(retry_map, proxy, &delay))
        return false;
      if (delay < min_delay)
        min_delay = delay;
      bypassed = true;
    }
  }

  if (min_retry_delay && bypassed)
    *min_retry_delay = min_delay;

  return bypassed;
}

bool DataReductionProxyConfig::IsProxyBypassed(
    const net::ProxyRetryInfoMap& retry_map,
    const net::ProxyServer& proxy_server,
    base::TimeDelta* retry_delay) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return IsProxyBypassedAtTime(retry_map, proxy_server, GetTicksNow(),
                               retry_delay);
}

bool DataReductionProxyConfig::ContainsDataReductionProxy(
    const net::ProxyConfig::ProxyRules& proxy_rules) const {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Data Reduction Proxy configurations are always Type::PROXY_LIST_PER_SCHEME.
  if (proxy_rules.type != net::ProxyConfig::ProxyRules::Type::PROXY_LIST_PER_SCHEME)
    return false;

  const net::ProxyList* http_proxy_list =
      proxy_rules.MapUrlSchemeToProxyList("http");
  if (http_proxy_list && !http_proxy_list->IsEmpty() &&
      // Sufficient to check only the first proxy.
      FindConfiguredDataReductionProxy(http_proxy_list->Get())) {
    return true;
  }

  return false;
}

void DataReductionProxyConfig::SetProxyConfig(bool enabled, bool at_startup) {
  DCHECK(thread_checker_.CalledOnValidThread());
  enabled_by_user_ = enabled;
  network_properties_manager_->OnChangeInNetworkID(GetCurrentNetworkID());

  ReloadConfig();

  if (enabled_by_user_) {
    HandleCaptivePortal();

    // Check if the proxy has been restricted explicitly by the carrier.
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
  network_properties_manager_->ResetWarmupURLFetchMetrics();
  FetchWarmupProbeURL();
}

void DataReductionProxyConfig::HandleCaptivePortal() {
  DCHECK(thread_checker_.CalledOnValidThread());

  bool is_captive_portal = GetIsCaptivePortal();
  if (is_captive_portal == network_properties_manager_->IsCaptivePortal())
    return;
  network_properties_manager_->SetIsCaptivePortal(is_captive_portal);
  ReloadConfig();
}

bool DataReductionProxyConfig::GetIsCaptivePortal() const {
  DCHECK(thread_checker_.CalledOnValidThread());

#if defined(OS_ANDROID)
  return net::android::GetIsCaptivePortal();
#endif  // OS_ANDROID
  return false;
}

void DataReductionProxyConfig::UpdateConfigForTesting(
    bool enabled,
    bool secure_proxies_allowed,
    bool insecure_proxies_allowed) {
  enabled_by_user_ = enabled;
  network_properties_manager_->ResetWarmupURLFetchMetrics();
  network_properties_manager_->SetIsSecureProxyDisallowedByCarrier(
      !secure_proxies_allowed);
  if (!insecure_proxies_allowed !=
          network_properties_manager_->HasWarmupURLProbeFailed(
              false /* secure_proxy */, true /* is_core_proxy */)) {
    network_properties_manager_->SetHasWarmupURLProbeFailed(
        false /* secure_proxy */, true /* is_core_proxy */,
        !insecure_proxies_allowed);
  }
}

void DataReductionProxyConfig::SetNetworkPropertiesManagerForTesting(
    NetworkPropertiesManager* manager) {
  network_properties_manager_ = manager;
}

base::Optional<DataReductionProxyServer>
DataReductionProxyConfig::GetProxyConnectionToProbe() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  const std::vector<DataReductionProxyServer>& proxies =
      DataReductionProxyConfig::GetProxiesForHttp();

  for (const DataReductionProxyServer& proxy_server : proxies) {
    // First find a proxy server that has never been probed before. Proxies that
    // have been probed before successfully do not need to be probed. On the
    // other hand, proxies that have been probed before unsuccessfully are
    // already disabled, and so they need not be probed immediately.
    bool is_secure_proxy = proxy_server.IsSecureProxy();
    bool is_core_proxy = proxy_server.IsCoreProxy();
    if (!network_properties_manager_->HasWarmupURLProbeFailed(is_secure_proxy,
                                                              is_core_proxy) &&
        network_properties_manager_->ShouldFetchWarmupProbeURL(is_secure_proxy,
                                                               is_core_proxy)) {
      return proxy_server;
    }
  }

  for (const DataReductionProxyServer& proxy_server : proxies) {
    // Now find any proxy server that can be probed. This would return proxies
    // that were probed before, the result was unsuccessful, but they have not
    // yet hit the maximum probe retry limit.
    bool is_secure_proxy = proxy_server.IsSecureProxy();
    bool is_core_proxy = proxy_server.IsCoreProxy();
    if (network_properties_manager_->ShouldFetchWarmupProbeURL(is_secure_proxy,
                                                               is_core_proxy)) {
      return proxy_server;
    }
  }

  // No more proxies left to probe.
  return base::nullopt;
}

void DataReductionProxyConfig::HandleWarmupFetcherResponse(
    const net::ProxyServer& proxy_server,
    WarmupURLFetcher::FetchResult success_response) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(IsFetchInFlight());

  base::Optional<DataReductionProxyTypeInfo> proxy_type_info =
      FindConfiguredDataReductionProxy(proxy_server);

  // Check the proxy server used.
  if (!proxy_type_info && proxy_server.is_valid() &&
      !proxy_server.is_direct()) {
    // No need to do anything here since the warmup fetch went through
    // a non-datasaver proxy.
    return;
  }

  bool is_secure_proxy = false;
  bool is_core_proxy = false;

  if (proxy_type_info) {
    DCHECK(proxy_server.is_valid());
    DCHECK(!proxy_server.is_direct());
    is_secure_proxy = proxy_server.is_https() || proxy_server.is_quic();
    is_core_proxy = proxy_type_info->proxy_servers[proxy_type_info->proxy_index]
                        .IsCoreProxy();

    // The proxy server through which the warmup URL was fetched should match
    // the proxy server for which the warmup URL is in-flight.
    DCHECK(GetInFlightWarmupProxyDetails());
    DCHECK_EQ(is_secure_proxy, GetInFlightWarmupProxyDetails()->first);
    DCHECK_EQ(is_core_proxy, GetInFlightWarmupProxyDetails()->second);
  } else {
    DCHECK(!proxy_server.is_valid() || proxy_server.is_direct());
    // When the probe times out or if the warmup URL was fetched via DIRECT
    // proxy, the data reduction proxy information may not be set. Fill-in the
    // missing data using the proxy that was being probed.
    is_secure_proxy = warmup_url_fetch_in_flight_secure_proxy_;
    is_core_proxy = warmup_url_fetch_in_flight_core_proxy_;
  }

  if (is_secure_proxy && is_core_proxy) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
        "SecureProxy.Core",
        success_response == WarmupURLFetcher::FetchResult::kSuccessful);
  } else if (is_secure_proxy && !is_core_proxy) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
        "SecureProxy.NonCore",
        success_response == WarmupURLFetcher::FetchResult::kSuccessful);
  } else if (!is_secure_proxy && is_core_proxy) {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
        "InsecureProxy.Core",
        success_response == WarmupURLFetcher::FetchResult::kSuccessful);
  } else {
    UMA_HISTOGRAM_BOOLEAN(
        "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
        "InsecureProxy.NonCore",
        success_response == WarmupURLFetcher::FetchResult::kSuccessful);
  }

  bool warmup_url_failed_past =
      network_properties_manager_->HasWarmupURLProbeFailed(is_secure_proxy,
                                                           is_core_proxy);

  network_properties_manager_->SetHasWarmupURLProbeFailed(
      is_secure_proxy, is_core_proxy,
      success_response !=
          WarmupURLFetcher::FetchResult::kSuccessful /* warmup failed */);

  if (warmup_url_failed_past !=
      network_properties_manager_->HasWarmupURLProbeFailed(is_secure_proxy,
                                                           is_core_proxy)) {
    ReloadConfig();
  }

  // May probe other proxy types that have not been probed yet, or may retry
  // probe of proxy types that has failed but the maximum probe limit has not
  // been reached yet. This method may have been called by warmup URL fetcher.
  // FetchWarmupProbeURL() may itself call warmup URL fetcher. Posting the call
  // here avoids recursive calls to the warmup URL fetcher.
  FetchWarmupProbeURL();
}

void DataReductionProxyConfig::HandleSecureProxyCheckResponse(
    const std::string& response,
    int net_status,
    int http_response_code) {
  bool success_response =
      base::StartsWith(response, "OK", base::CompareCase::SENSITIVE);

  if (net_status != net::OK) {
    if (net_status == net::ERR_INTERNET_DISCONNECTED) {
      RecordSecureProxyCheckFetchResult(INTERNET_DISCONNECTED);
      return;
    }
    // TODO(bengr): Remove once we understand the reasons secure proxy checks
    // are failing. Secure proxy check errors are either due to fetcher-level
    // errors or modified responses. This only tracks the former.
    base::UmaHistogramSparse(kUMAProxyProbeURLNetError, std::abs(net_status));
  }

  bool secure_proxy_allowed_past =
      !network_properties_manager_->IsSecureProxyDisallowedByCarrier();
  network_properties_manager_->SetIsSecureProxyDisallowedByCarrier(
      !success_response);
  if (!enabled_by_user_)
    return;

  if (!network_properties_manager_->IsSecureProxyDisallowedByCarrier() !=
      secure_proxy_allowed_past)
    ReloadConfig();

  // Record the result.
  if (secure_proxy_allowed_past &&
      !network_properties_manager_->IsSecureProxyDisallowedByCarrier()) {
    RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ALREADY_ENABLED);
  } else if (secure_proxy_allowed_past &&
             network_properties_manager_->IsSecureProxyDisallowedByCarrier()) {
    RecordSecureProxyCheckFetchResult(FAILED_PROXY_DISABLED);
  } else if (!secure_proxy_allowed_past &&
             !network_properties_manager_->IsSecureProxyDisallowedByCarrier()) {
    RecordSecureProxyCheckFetchResult(SUCCEEDED_PROXY_ENABLED);
  } else {
    DCHECK(!secure_proxy_allowed_past &&
           network_properties_manager_->IsSecureProxyDisallowedByCarrier());
    RecordSecureProxyCheckFetchResult(FAILED_PROXY_ALREADY_DISABLED);
  }
}

void DataReductionProxyConfig::OnConnectionChanged(
    network::mojom::ConnectionType type) {
  DCHECK(thread_checker_.CalledOnValidThread());

  connection_type_ = type;
  RecordNetworkChangeEvent(NETWORK_CHANGED);

#if defined(OS_CHROMEOS)
  if (get_network_id_asynchronously_) {
    base::PostTaskAndReplyWithResult(
        g_get_network_id_task_runner.Get().get(), FROM_HERE,
        base::BindOnce(&DoGetCurrentNetworkID,
                       base::Unretained(network_connection_tracker_)),
        base::BindOnce(&DataReductionProxyConfig::ContinueNetworkChanged,
                       weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // defined(OS_CHROMEOS)

  ContinueNetworkChanged(GetCurrentNetworkID());
}

void DataReductionProxyConfig::ContinueNetworkChanged(
    const std::string& network_id) {
  network_properties_manager_->OnChangeInNetworkID(network_id);

  ReloadConfig();

  FetchWarmupProbeURL();

  if (enabled_by_user_) {
    HandleCaptivePortal();
    // It is safe to use base::Unretained here, since it gets executed
    // synchronously on the IO thread, and |this| outlives
    // |secure_proxy_checker_|.
    SecureProxyCheck(
        base::Bind(&DataReductionProxyConfig::HandleSecureProxyCheckResponse,
                   base::Unretained(this)));
  }
}

void DataReductionProxyConfig::AddDefaultProxyBypassRules() {
  DCHECK(configurator_);
  // Under the hood we use an instance of ProxyBypassRules to evaluate these
  // rules. ProxyBypassRules implicitly bypasses localhost, loopback, and
  // link-local addresses, so it is not necessary to explicitly add them here.
  // See ProxyBypassRules::MatchesImplicitRules() for details.
  configurator_->SetBypassRules(
      // Hostnames with no dot in them.
      "<local>,"

      // WebSockets
      "ws://*,"
      "wss://*,"

      // RFC6890 current network (only valid as source address).
      "0.0.0.0/8,"

      // RFC1918 private addresses.
      "10.0.0.0/8,"
      "172.16.0.0/12,"
      "192.168.0.0/16,"

      // RFC3513 unspecified address.
      "::/128,"

      // RFC4193 private addresses.
      "fc00::/7,"

      // IPV6 probe addresses.
      "*-ds.metric.gstatic.com,"
      "*-v4.metric.gstatic.com");
}

void DataReductionProxyConfig::SecureProxyCheck(
    SecureProxyCheckerCallback fetcher_callback) {
  if (params::IsIncludedInHoldbackFieldTrial())
    return;

  secure_proxy_checker_->CheckIfSecureProxyIsAllowed(fetcher_callback);
}

void DataReductionProxyConfig::FetchWarmupProbeURL() {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (params::IsIncludedInHoldbackFieldTrial())
    return;

  if (!enabled_by_user_) {
    RecordWarmupURLFetchAttemptEvent(
        WarmupURLFetchAttemptEvent::kProxyNotEnabledByUser);
    return;
  }

  if (!params::FetchWarmupProbeURLEnabled()) {
    RecordWarmupURLFetchAttemptEvent(
        WarmupURLFetchAttemptEvent::kWarmupURLFetchingDisabled);
    return;
  }

  if (connection_type_ == network::mojom::ConnectionType::CONNECTION_NONE) {
    RecordWarmupURLFetchAttemptEvent(
        WarmupURLFetchAttemptEvent::kConnectionTypeNone);
    return;
  }

  base::Optional<DataReductionProxyServer> warmup_proxy =
      GetProxyConnectionToProbe();

  if (!warmup_proxy)
    return;

  // Refetch the warmup URL when it has failed.
  warmup_url_fetch_in_flight_secure_proxy_ = warmup_proxy->IsSecureProxy();
  warmup_url_fetch_in_flight_core_proxy_ = warmup_proxy->IsCoreProxy();

  size_t previous_attempt_counts = GetWarmupURLFetchAttemptCounts();

  network_properties_manager_->OnWarmupFetchInitiated(
      warmup_url_fetch_in_flight_secure_proxy_,
      warmup_url_fetch_in_flight_core_proxy_);

  RecordWarmupURLFetchAttemptEvent(WarmupURLFetchAttemptEvent::kFetchInitiated);

  warmup_url_fetcher_->FetchWarmupURL(previous_attempt_counts,
                                      warmup_proxy.value());
}

size_t DataReductionProxyConfig::GetWarmupURLFetchAttemptCounts() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return network_properties_manager_->GetWarmupURLFetchAttemptCounts(
      warmup_url_fetch_in_flight_secure_proxy_,
      warmup_url_fetch_in_flight_core_proxy_);
}

void DataReductionProxyConfig::OnRTTOrThroughputEstimatesComputed(
    base::TimeDelta http_rtt) {
  DCHECK(thread_checker_.CalledOnValidThread());
  http_rtt_ = http_rtt;
}

base::Optional<base::TimeDelta> DataReductionProxyConfig::GetHttpRttEstimate()
    const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return http_rtt_;
}

bool DataReductionProxyConfig::enabled_by_user_and_reachable() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return enabled_by_user_ && !unreachable_;
}

base::TimeTicks DataReductionProxyConfig::GetTicksNow() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return base::TimeTicks::Now();
}

net::ProxyConfig DataReductionProxyConfig::ProxyConfigIgnoringHoldback() const {
  if (!enabled_by_user_ || config_values_->proxies_for_http().empty())
    return net::ProxyConfig::CreateDirect();
  return configurator_->CreateProxyConfig(false /* probe_url_config */,
                                          *network_properties_manager_,
                                          config_values_->proxies_for_http());
}

std::vector<DataReductionProxyServer>
DataReductionProxyConfig::GetProxiesForHttp() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!enabled_by_user_)
    return std::vector<DataReductionProxyServer>();

  return config_values_->proxies_for_http();
}

std::string DataReductionProxyConfig::GetCurrentNetworkID() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return DoGetCurrentNetworkID(network_connection_tracker_);
}

const NetworkPropertiesManager&
DataReductionProxyConfig::GetNetworkPropertiesManager() const {
  DCHECK(thread_checker_.CalledOnValidThread());
  return *network_properties_manager_;
}

bool DataReductionProxyConfig::IsFetchInFlight() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!warmup_url_fetcher_)
    return false;
  return warmup_url_fetcher_->IsFetchInFlight();
}

base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
DataReductionProxyConfig::GetInFlightWarmupProxyDetails() const {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (!IsFetchInFlight())
    return base::nullopt;

  return std::make_pair(warmup_url_fetch_in_flight_secure_proxy_,
                        warmup_url_fetch_in_flight_core_proxy_);
}

#if defined(OS_CHROMEOS)
void DataReductionProxyConfig::EnableGetNetworkIdAsynchronously() {
  get_network_id_asynchronously_ = true;
}
#endif  // defined(OS_CHROMEOS)

}  // namespace data_reduction_proxy
