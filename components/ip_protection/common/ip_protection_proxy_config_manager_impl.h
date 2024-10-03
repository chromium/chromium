// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MANAGER_IMPL_H_
#define COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MANAGER_IMPL_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/ip_protection/common/ip_protection_config_getter.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_manager.h"
#include "net/base/proxy_chain.h"

namespace ip_protection {

// An implementation of IpProtectionProxyConfigManager that populates itself
// using a passed in IpProtectionConfigGetter pointer from the cache.
class IpProtectionProxyConfigManagerImpl
    : public IpProtectionProxyConfigManager {
 public:
  explicit IpProtectionProxyConfigManagerImpl(
      IpProtectionCore* core,
      IpProtectionConfigGetter& config_getter,
      bool disable_proxy_refreshing_for_testing = false);
  ~IpProtectionProxyConfigManagerImpl() override;

  // IpProtectionProxyConfigManager implementation.
  bool IsProxyListAvailable() override;
  const std::vector<net::ProxyChain>& ProxyList() override;
  const std::string& CurrentGeo() override;
  void RefreshProxyListForGeoChange() override;
  void RequestRefreshProxyList() override;

  // Set a callback to occur when the proxy list has been refreshed.
  void SetOnProxyListRefreshedForTesting(
      base::OnceClosure on_proxy_list_refreshed) {
    on_proxy_list_refreshed_for_testing_ = std::move(on_proxy_list_refreshed);
  }

  // Trigger a proxy list refresh.
  void EnableAndTriggerProxyListRefreshingForTesting() {
    EnableProxyListRefreshingForTesting();
    RefreshProxyList();
  }

  // Enable proxy refreshing.
  // This does not trigger an immediate proxy list refresh.
  void EnableProxyListRefreshingForTesting() {
    disable_proxy_refreshing_for_testing_ = false;
  }

  void SetProxyListForTesting(
      std::optional<std::vector<net::ProxyChain>> proxy_list,
      std::optional<GeoHint> geo_hint) {
    current_geo_id_ = GetGeoIdFromGeoHint(geo_hint);
    proxy_list_ = *proxy_list;
    have_fetched_proxy_list_ = true;
  }

  void EnableProxyListFetchIntervalFuzzingForTesting(bool enable);

 private:
  void RefreshProxyList();
  void ScheduleRefreshProxyList(base::TimeDelta delay);
  void OnGotProxyList(base::TimeTicks refresh_start_time_for_metrics,
                      std::optional<std::vector<net::ProxyChain>> proxy_list,
                      std::optional<GeoHint> geo_hint);
  bool IsProxyListOlderThanMinAge() const;
  base::TimeDelta FuzzProxyListFetchInterval(base::TimeDelta delay);

  // Latest fetched proxy list.
  std::vector<net::ProxyChain> proxy_list_;

  // Current geo of the proxy list.
  std::string current_geo_id_ = "";

  // True if an invocation of `config_getter_.GetProxyList()` is
  // outstanding.
  bool fetching_proxy_list_ = false;

  // True if the proxy list has been fetched at least once.
  bool have_fetched_proxy_list_ = false;

  // Pointer to the `IpProtectionCore` that holds the proxy list and
  // tokens. Required to observe geo changes from refreshed proxy lists.
  // The lifetime of the `IpProtectionCore` object WILL ALWAYS outlive
  // this class b/c `ip_protection_core_` owns this (at least outside of
  // testing).
  const raw_ptr<IpProtectionCore> ip_protection_core_;

  // Source of proxy list, when needed.
  raw_ref<IpProtectionConfigGetter> config_getter_;

  // The last time this instance began refreshing the proxy list.
  base::Time last_proxy_list_refresh_;

  // The min age of the proxy list before an additional refresh is allowed.
  const base::TimeDelta proxy_list_min_age_;

  // The regular time interval where the proxy list is refreshed.
  const base::TimeDelta proxy_list_refresh_interval_;

  // If false, proxy list refresh interval is not fuzzed.
  bool enable_proxy_list_fetch_interval_fuzzing_for_testing_ = true;

  // Feature flag to safely introduce token caching by geo.
  const bool enable_token_caching_by_geo_;

  // A timer to run `RefreshProxyList()` when necessary.
  base::OneShotTimer next_refresh_proxy_list_;

  // A callback triggered when an asynchronous proxy-list refresh is complete,
  // for use in testing.
  base::OnceClosure on_proxy_list_refreshed_for_testing_;

  // If true, do not try to automatically refresh the proxy list.
  bool disable_proxy_refreshing_for_testing_ = false;

  base::WeakPtrFactory<IpProtectionProxyConfigManagerImpl> weak_ptr_factory_{
      this};
};

}  // namespace ip_protection

#endif  // COMPONENTS_IP_PROTECTION_COMMON_IP_PROTECTION_PROXY_CONFIG_MANAGER_IMPL_H_
