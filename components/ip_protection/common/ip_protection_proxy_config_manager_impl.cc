// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ip_protection/common/ip_protection_proxy_config_manager_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/rand_util.h"
#include "base/time/time.h"
#include "components/ip_protection/common/ip_protection_core.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_fetcher.h"
#include "components/ip_protection/common/ip_protection_telemetry.h"
#include "net/base/features.h"
#include "net/base/proxy_chain.h"

namespace ip_protection {

namespace {

// Based on the logic in the `IpProtectionProxyConfigDirectFetcher`, if there is
// a non-empty proxy list with an empty `GeoHint`, it would be considered a
// failed call which means a `proxy_list` with a value of `std::nullopt` would
// be returned. Thus, the following function captures all states of failure
// accurately even though we only check the `proxy_chain`.
void RecordTelemetry(
    const std::optional<std::vector<net::ProxyChain>>& proxy_list,
    base::TimeDelta duration) {
  GetProxyListResult result;
  if (!proxy_list.has_value()) {
    result = GetProxyListResult::kFailed;
  } else if (proxy_list->empty()) {
    result = GetProxyListResult::kEmptyList;
  } else {
    result = GetProxyListResult::kPopulatedList;
  }

  Telemetry().ProxyListRefreshComplete(result,
                                       result == GetProxyListResult::kFailed
                                           ? std::nullopt
                                           : std::make_optional(duration));
}

}  // namespace

IpProtectionProxyConfigManagerImpl::IpProtectionProxyConfigManagerImpl(
    IpProtectionCore* core,
    std::unique_ptr<IpProtectionProxyConfigFetcher> fetcher,
    bool disable_proxy_refreshing_for_testing)
    : ip_protection_core_(core),
      fetcher_(std::move(fetcher)),
      proxy_list_min_age_(
          net::features::kIpPrivacyProxyListMinFetchInterval.Get()),
      proxy_list_refresh_interval_(
          net::features::kIpPrivacyProxyListFetchInterval.Get()),
      disable_proxy_refreshing_for_testing_(
          disable_proxy_refreshing_for_testing) {
  if (!disable_proxy_refreshing_for_testing_) {
    // Refresh the proxy list immediately.
    RefreshProxyList();
  }
}

IpProtectionProxyConfigManagerImpl::~IpProtectionProxyConfigManagerImpl() =
    default;

bool IpProtectionProxyConfigManagerImpl::IsProxyListAvailable() {
  return have_fetched_proxy_list_;
}

const std::vector<net::ProxyChain>&
IpProtectionProxyConfigManagerImpl::ProxyList() {
  return proxy_list_;
}

const std::string& IpProtectionProxyConfigManagerImpl::CurrentGeo() {
  return current_geo_id_;
}

void IpProtectionProxyConfigManagerImpl::RequestRefreshProxyList() {
  if (IsProxyListOlderThanMinAge()) {
    RefreshProxyList();
    return;
  }

  // If list is not older than min interval, schedule refresh as soon as
  // possible.
  base::TimeDelta time_since_last_refresh =
      base::Time::Now() - last_successful_proxy_list_refresh_;

  base::TimeDelta delay = proxy_list_min_age_ - time_since_last_refresh;
  ScheduleRefreshProxyList(delay.is_negative() ? base::TimeDelta() : delay);
}

void IpProtectionProxyConfigManagerImpl::RefreshProxyList() {
  if (fetching_proxy_list_) {
    return;
  }

  fetching_proxy_list_ = true;
  last_successful_proxy_list_refresh_ = base::Time::Now();
  const base::TimeTicks refresh_start_time_for_metrics = base::TimeTicks::Now();

  fetcher_->GetProxyConfig(base::BindOnce(
      &IpProtectionProxyConfigManagerImpl::OnGotProxyList,
      weak_ptr_factory_.GetWeakPtr(), refresh_start_time_for_metrics));
}

void IpProtectionProxyConfigManagerImpl::OnGotProxyList(
    const base::TimeTicks refresh_start_time_for_metrics,
    std::optional<std::vector<net::ProxyChain>> proxy_list,
    std::optional<GeoHint> geo_hint) {
  fetching_proxy_list_ = false;

  RecordTelemetry(proxy_list,
                  base::TimeTicks::Now() - refresh_start_time_for_metrics);

  // If the request for fetching the proxy list is successful, utilize the new
  // proxy list, otherwise, continue using the existing list, if any.
  if (proxy_list.has_value()) {
    proxy_list_ = std::move(*proxy_list);
    have_fetched_proxy_list_ = true;

    // Only trigger a callback to the config cache if the following requirements
    // are met:
    // 1. The proxy_list is non-empty. An empty list implies there is no
    //    geo_hint present.
    // 2. The new geo is different than the existing geo.
    if (!proxy_list_.empty()) {
      CHECK(geo_hint.has_value());
      current_geo_id_ = GetGeoIdFromGeoHint(std::move(geo_hint));
      ip_protection_core_->GeoObserved(current_geo_id_);
    }
  } else {
    // The request was not successful, so do not count this toward the
    // minimum time between refreshes. Note that the proxy config fetcher
    // implements its own backoff on errors, so this does not risk overwhelming
    // the server.
    last_successful_proxy_list_refresh_ = base::Time();
  }

  base::TimeDelta fuzzed_proxy_list_refresh_interval =
      FuzzProxyListFetchInterval(proxy_list_refresh_interval_);
  ScheduleRefreshProxyList(fuzzed_proxy_list_refresh_interval);

  if (on_proxy_list_refreshed_for_testing_) {
    std::move(on_proxy_list_refreshed_for_testing_).Run();
  }
}

base::TimeDelta IpProtectionProxyConfigManagerImpl::FuzzProxyListFetchInterval(
    base::TimeDelta delay) {
  if (!enable_proxy_list_fetch_interval_fuzzing_) {
    return delay;
  }

  // Randomize the next fetch interval, ensuring it's not less than the minimum
  // age of proxy list refresh allowed.
  base::TimeDelta fuzz_range =
      net::features::kIpPrivacyProxyListFetchIntervalFuzz.Get();
  return std::max(proxy_list_min_age_,
                  delay + base::RandTimeDelta(-fuzz_range, fuzz_range));
}

bool IpProtectionProxyConfigManagerImpl::IsProxyListOlderThanMinAge() const {
  return base::Time::Now() - last_successful_proxy_list_refresh_ >=
         proxy_list_min_age_;
}

void IpProtectionProxyConfigManagerImpl::SetProxyListForTesting(
    std::vector<net::ProxyChain> proxy_list,
    std::optional<GeoHint> geo_hint) {
  current_geo_id_ = GetGeoIdFromGeoHint(std::move(geo_hint));
  proxy_list_ = std::move(proxy_list);
  have_fetched_proxy_list_ = true;
}

void IpProtectionProxyConfigManagerImpl::
    EnableProxyListFetchIntervalFuzzingForTesting(bool enable) {
  enable_proxy_list_fetch_interval_fuzzing_ = enable;
}

void IpProtectionProxyConfigManagerImpl::ScheduleRefreshProxyList(
    base::TimeDelta delay) {
  CHECK(!delay.is_negative());

  // Nothing to schedule if refreshing is disabled for testing.
  if (disable_proxy_refreshing_for_testing_) {
    return;
  }

  if (fetching_proxy_list_) {
    next_refresh_proxy_list_.Stop();
    return;
  }

  if (delay.is_negative()) {
    delay = base::TimeDelta();
  }

  // Schedule the next refresh. If this timer was already running, this will
  // reschedule it for the given time.
  next_refresh_proxy_list_.Start(
      FROM_HERE, delay,
      base::BindOnce(&IpProtectionProxyConfigManagerImpl::RefreshProxyList,
                     weak_ptr_factory_.GetWeakPtr()));
}
}  // namespace ip_protection
