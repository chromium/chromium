// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
class TickClock;
}

namespace data_reduction_proxy {

class DataReductionProxyConfigurator;
class DataReductionProxyMutableConfigValues;
class TestDataReductionProxyParams;

// Test version of |DataReductionProxyConfig|, which uses an underlying
// |TestDataReductionProxyParams| to permit overriding of default values
// returning from |DataReductionProxyParams|, as well as exposing methods to
// change the underlying state.
class TestDataReductionProxyConfig : public DataReductionProxyConfig {
 public:
  TestDataReductionProxyConfig(
      DataReductionProxyConfigurator* configurator);

  // Creates a |TestDataReductionProxyConfig| with the provided |config_values|.
  // This permits any DataReductionProxyConfigValues to be used (such as
  // DataReductionProxyParams or DataReductionProxyMutableConfigValues).
  TestDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      DataReductionProxyConfigurator* configurator);

  ~TestDataReductionProxyConfig() override;

  // Allows tests to reset the params being used for configuration.
  void ResetParamFlagsForTest();

  // Retrieves the test params being used for the configuration.
  TestDataReductionProxyParams* test_params();

  // Retrieves the underlying config values.
  // TODO(jeremyim): Rationalize with test_params().
  DataReductionProxyConfigValues* config_values();

  // Sets the |tick_clock_| to |tick_clock|. Ownership of |tick_clock| is not
  // passed to the callee.
  void SetTickClock(const base::TickClock* tick_clock);

  base::TimeTicks GetTicksNow() const override;

  // Sets if the captive portal probe has been blocked for the current network.
  void SetIsCaptivePortal(bool is_captive_portal);

  void SetConnectionTypeForTesting(
      network::mojom::ConnectionType connection_type) {
    connection_type_ = connection_type;
  }

  void AddDefaultProxyBypassRules() override;

  void SetShouldAddDefaultProxyBypassRules(bool add_default_proxy_bypass_rules);

  std::string GetCurrentNetworkID() const override;

  void SetCurrentNetworkID(const std::string& network_id);

  base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
  GetInFlightWarmupProxyDetails() const override;

  void SetInFlightWarmupProxyDetails(
      base::Optional<
          std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
          in_flight_warmup_proxy_details);

  bool IsFetchInFlight() const override;

  void SetIsFetchInFlight(bool fetch_in_flight);

  size_t GetWarmupURLFetchAttemptCounts() const override;

  void SetWarmupURLFetchAttemptCounts(
      base::Optional<size_t> previous_attempt_counts);

  using DataReductionProxyConfig::UpdateConfigForTesting;
  using DataReductionProxyConfig::HandleWarmupFetcherResponse;

 private:
  bool GetIsCaptivePortal() const override;

  const base::TickClock* tick_clock_;

  base::Optional<size_t> previous_attempt_counts_;

  base::Optional<std::string> current_network_id_;

  base::Optional<std::pair<bool /* is_secure_proxy */, bool /*is_core_proxy */>>
      in_flight_warmup_proxy_details_;

  // Set to true if the captive portal probe for the current network has been
  // blocked.
  bool is_captive_portal_;

  // True if the default bypass rules should be added. Should be set to false
  // when fetching resources from an embedded test server running on localhost.
  bool add_default_proxy_bypass_rules_;

  base::Optional<bool> fetch_in_flight_;

  DISALLOW_COPY_AND_ASSIGN(TestDataReductionProxyConfig);
};

// A |TestDataReductionProxyConfig| which permits mocking of methods for
// testing.
class MockDataReductionProxyConfig : public TestDataReductionProxyConfig {
 public:
  // Creates a |MockDataReductionProxyConfig|.
  MockDataReductionProxyConfig(
      std::unique_ptr<DataReductionProxyConfigValues> config_values,
      DataReductionProxyConfigurator* configurator);
  ~MockDataReductionProxyConfig() override;

  MOCK_CONST_METHOD1(ContainsDataReductionProxy,
                     bool(const net::ProxyConfig::ProxyRules& proxy_rules));
  MOCK_METHOD1(SecureProxyCheck,
               void(SecureProxyCheckerCallback fetcher_callback));
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_TEST_UTILS_H_
