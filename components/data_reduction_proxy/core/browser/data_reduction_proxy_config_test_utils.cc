// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"

#include <stddef.h>

#include <utility>

#include "base/single_thread_task_runner.h"
#include "base/time/tick_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::_;

namespace data_reduction_proxy {

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DataReductionProxyConfigurator* configurator)
    : TestDataReductionProxyConfig(
          std::make_unique<TestDataReductionProxyParams>(),
          io_task_runner,
          ui_task_runner,
          configurator) {}

TestDataReductionProxyConfig::TestDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DataReductionProxyConfigurator* configurator)
    : DataReductionProxyConfig(
          io_task_runner,
          ui_task_runner,
          network::TestNetworkConnectionTracker::GetInstance(),
          std::move(config_values),
          configurator),
      tick_clock_(nullptr),
      is_captive_portal_(false),
      add_default_proxy_bypass_rules_(true) {}

TestDataReductionProxyConfig::~TestDataReductionProxyConfig() {
}

void TestDataReductionProxyConfig::ResetParamFlagsForTest() {
  config_values_ = std::make_unique<TestDataReductionProxyParams>();
}

TestDataReductionProxyParams* TestDataReductionProxyConfig::test_params() {
  return static_cast<TestDataReductionProxyParams*>(config_values_.get());
}

DataReductionProxyConfigValues* TestDataReductionProxyConfig::config_values() {
  return config_values_.get();
}

void TestDataReductionProxyConfig::SetTickClock(
    const base::TickClock* tick_clock) {
  tick_clock_ = tick_clock;
}

base::TimeTicks TestDataReductionProxyConfig::GetTicksNow() const {
  if (tick_clock_)
    return tick_clock_->NowTicks();
  return DataReductionProxyConfig::GetTicksNow();
}

void TestDataReductionProxyConfig::SetIsCaptivePortal(bool is_captive_portal) {
  is_captive_portal_ = is_captive_portal;
}

bool TestDataReductionProxyConfig::GetIsCaptivePortal() const {
  return is_captive_portal_;
}

bool TestDataReductionProxyConfig::ShouldAddDefaultProxyBypassRules() const {
  return add_default_proxy_bypass_rules_;
}

void TestDataReductionProxyConfig::SetShouldAddDefaultProxyBypassRules(
    bool add_default_proxy_bypass_rules) {
  add_default_proxy_bypass_rules_ = add_default_proxy_bypass_rules;
}

std::string TestDataReductionProxyConfig::GetCurrentNetworkID() const {
  if (current_network_id_) {
    return current_network_id_.value();
  }
  return DataReductionProxyConfig::GetCurrentNetworkID();
}

void TestDataReductionProxyConfig::SetCurrentNetworkID(
    const std::string& network_id) {
  current_network_id_ = network_id;
}

base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
TestDataReductionProxyConfig::GetInFlightWarmupProxyDetails() const {
  if (in_flight_warmup_proxy_details_)
    return in_flight_warmup_proxy_details_;
  return DataReductionProxyConfig::GetInFlightWarmupProxyDetails();
}

void TestDataReductionProxyConfig::SetInFlightWarmupProxyDetails(
    base::Optional<
        std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
        in_flight_warmup_proxy_details) {
  in_flight_warmup_proxy_details_ = in_flight_warmup_proxy_details;
}

bool TestDataReductionProxyConfig::IsFetchInFlight() const {
  if (fetch_in_flight_)
    return fetch_in_flight_.value();
  return DataReductionProxyConfig::IsFetchInFlight();
}

void TestDataReductionProxyConfig::SetIsFetchInFlight(bool fetch_in_flight) {
  fetch_in_flight_ = fetch_in_flight;
}

size_t TestDataReductionProxyConfig::GetWarmupURLFetchAttemptCounts() const {
  if (!previous_attempt_counts_)
    return DataReductionProxyConfig::GetWarmupURLFetchAttemptCounts();
  return previous_attempt_counts_.value();
}

void TestDataReductionProxyConfig::SetWarmupURLFetchAttemptCounts(
    base::Optional<size_t> previous_attempt_counts) {
  previous_attempt_counts_ = previous_attempt_counts;
}

MockDataReductionProxyConfig::MockDataReductionProxyConfig(
    std::unique_ptr<DataReductionProxyConfigValues> config_values,
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner,
    DataReductionProxyConfigurator* configurator)
    : TestDataReductionProxyConfig(std::move(config_values),
                                   io_task_runner,
                                   ui_task_runner,
                                   configurator) {}

MockDataReductionProxyConfig::~MockDataReductionProxyConfig() {
}

}  // namespace data_reduction_proxy
