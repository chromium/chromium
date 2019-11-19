// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "components/data_reduction_proxy/proto/network_properties.pb.h"
#include "components/prefs/pref_service.h"

namespace base {
class Clock;
class Value;
}

namespace data_reduction_proxy {

// Stores the properties of a single network. Lives on the IO thread.
class NetworkPropertiesManager {
 public:
  NetworkPropertiesManager(base::Clock* clock, PrefService* pref_service);

  virtual ~NetworkPropertiesManager();

  // Called when the user clears the browsing history.
  void DeleteHistory();

  void OnChangeInNetworkID(const std::string& network_id);

  // Returns true if usage of secure proxies are allowed on the current network.
  // Returns the status of core secure proxies if |is_core_proxy| is true.
  bool IsSecureProxyAllowed(bool is_core_proxy) const;

  // Returns true if usage of insecure proxies are allowed on the current
  // network.  Returns the status of core non-secure proxies if |is_core_proxy|
  // is true.
  bool IsInsecureProxyAllowed(bool is_core_proxy) const;

  // Returns true if usage of secure proxies has been disallowed by the carrier
  // on the current network.
  bool IsSecureProxyDisallowedByCarrier() const;

  // Sets the status of whether the usage of secure proxies is disallowed by the
  // carrier on the current network.
  void SetIsSecureProxyDisallowedByCarrier(bool disallowed_by_carrier);

  // Returns true if the current network has a captive portal.
  bool IsCaptivePortal() const;

  // Sets the status of whether the current network has a captive portal or not.
  // If the current network has captive portal, usage of secure proxies is
  // disallowed.
  void SetIsCaptivePortal(bool is_captive_portal);

  // Returns true if the warmup URL probe has failed
  // on secure (or insecure), core (or non-core) data saver proxies on the
  // current network.
  bool HasWarmupURLProbeFailed(bool secure_proxy, bool is_core_proxy) const;

  // Sets the status of whether the fetching of warmup URL failed on the current
  // network. Sets the status for secure (or insecure), core (or non-core) data
  // saver proxies.
  void SetHasWarmupURLProbeFailed(bool secure_proxy,
                                  bool is_core_proxy,
                                  bool warmup_url_probe_failed);

  // Returns true if the warmup URL probe can be fetched from the proxy with the
  // specified properties.
  bool ShouldFetchWarmupProbeURL(bool secure_proxy, bool is_core_proxy) const;

  // Called when the warmup URL probe to a proxy with the specified properties
  // has been initiated.
  void OnWarmupFetchInitiated(bool secure_proxy, bool is_core_proxy);

  // Returns the count of fetch attempts that have been made to the proxy with
  // the specified properties.
  size_t GetWarmupURLFetchAttemptCounts(bool secure_proxy,
                                        bool is_core_proxy) const;

  // Resets the metrics related to the fetching of the warmup probe URL.
  void ResetWarmupURLFetchMetrics();

 private:
  // Map from network IDs to network properties.
  typedef std::map<std::string, NetworkProperties> NetworkPropertiesContainer;

  // PrefManager writes or updates the network properties prefs. Created on
  // UI thread, and should be used on the UI thread.
  class PrefManager;

  // Called when there is a change in the network property of the current
  // network.
  void OnChangeInNetworkProperty();

  static NetworkPropertiesContainer ConvertDictionaryValueToParsedPrefs(
      const base::Value* value);

  // Clock used for querying current time. Guaranteed to be non-null.
  base::Clock* clock_;

  // Network properties of different networks. Should be accessed on the UI
  // thread.
  NetworkPropertiesContainer network_properties_container_;

  // ID of the current network.
  std::string network_id_;

  // State of the proxies on the current network.
  NetworkProperties network_properties_;

  std::unique_ptr<PrefManager> pref_manager_;

  // Set to true if the fetch of the warmup URL was successful since the last
  // connection change. The status is recorded separately for each combination
  // of (is_secure_proxy) and (is_core_proxy).
  bool has_warmup_url_succeded_secure_core_;
  bool has_warmup_url_succeded_secure_non_core_;
  bool has_warmup_url_succeded_insecure_core_;
  bool has_warmup_url_succeded_insecure_non_core_;

  // Count of warmup URL fetch attempts since the last connection change. The
  // count is recorded separately for each combination of (is_secure_proxy) and
  // (is_core_proxy).
  size_t warmup_url_fetch_attempt_counts_secure_core_;
  size_t warmup_url_fetch_attempt_counts_secure_non_core_;
  size_t warmup_url_fetch_attempt_counts_insecure_core_;
  size_t warmup_url_fetch_attempt_counts_insecure_non_core_;

  // Should be dereferenced only on the UI thread.
  base::WeakPtr<PrefManager> pref_manager_weak_ptr_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NetworkPropertiesManager);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_NETWORK_PROPERTIES_MANAGER_H_
