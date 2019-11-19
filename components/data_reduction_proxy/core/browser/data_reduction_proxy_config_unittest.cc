// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"

#include <stddef.h>
#include <stdint.h>

#include <cstdlib>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/safe_sprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/platform_thread.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_service.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/variations/variations_associated_data.h"
#include "content/public/test/browser_task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/http/http_status_code.h"
#include "net/nqe/effective_connection_type.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/test/test_network_connection_tracker.h"

using testing::_;
using testing::AnyNumber;
using testing::Return;

namespace data_reduction_proxy {

namespace {

void SetProxiesForHttpOnCommandLine(
    const std::vector<net::ProxyServer>& proxies_for_http) {
  std::vector<std::string> proxy_strings;
  for (const net::ProxyServer& proxy : proxies_for_http)
    proxy_strings.push_back(proxy.ToURI());

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyHttpProxies,
      base::JoinString(proxy_strings, ";"));
}

std::string GetRetryMapKeyFromOrigin(const std::string& origin) {
  // The retry map has the scheme prefix for https but not for http.
  return origin;
}

}  // namespace

class DataReductionProxyConfigTest : public testing::Test {
 public:
  DataReductionProxyConfigTest() : mock_config_used_(false) {}
  ~DataReductionProxyConfigTest() override {}

  void SetUp() override {
    base::RunLoop().RunUntilIdle();

    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings();

    expected_params_.reset(new TestDataReductionProxyParams());
  }

  void RecreateContextWithMockConfig() {
    mock_config_used_ = true;
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithMockConfig()
                        .WithMockDataReductionProxyService()
                        .Build();

    ResetSettings();

    expected_params_.reset(new TestDataReductionProxyParams());
  }

  void ResetSettings() {
    if (mock_config_used_)
      mock_config()->ResetParamFlagsForTest();
    else
      test_config()->ResetParamFlagsForTest();
  }

  class TestResponder {
   public:
    void ExecuteCallback(SecureProxyCheckerCallback callback) {
      callback.Run(response, net_error, http_response_code);
    }

    std::string response;
    net::Error net_error;
    int http_response_code;
  };

  void CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType connection_type,
      const std::string& response,
      bool is_captive_portal,
      int response_code,
      net::Error net_error,
      SecureProxyCheckFetchResult expected_fetch_result,
      const std::vector<net::ProxyServer>& expected_proxies_for_http) {
    base::HistogramTester histogram_tester;
    SetConnectionType(connection_type);

    TestResponder responder;
    responder.response = response;
    responder.net_error = net_error;
    responder.http_response_code = response_code;
    EXPECT_CALL(*mock_config(), SecureProxyCheck(_))
        .Times(1)
        .WillRepeatedly(testing::WithArgs<0>(
            testing::Invoke(&responder, &TestResponder::ExecuteCallback)));
    mock_config()->SetIsCaptivePortal(is_captive_portal);
    test_context_->RunUntilIdle();
    EXPECT_EQ(expected_proxies_for_http, GetConfiguredProxiesForHttp());

    if (net_error != net::OK && net_error != net::ERR_INTERNET_DISCONNECTED) {
      histogram_tester.ExpectUniqueSample("DataReductionProxy.ProbeURLNetError",
                                          std::abs(net_error), 1);
    } else {
      histogram_tester.ExpectTotalCount("DataReductionProxy.ProbeURLNetError",
                                        0);
    }
    histogram_tester.ExpectUniqueSample("DataReductionProxy.ProbeURL",
                                        expected_fetch_result, 1);
  }

  void RunUntilIdle() {
    test_context_->RunUntilIdle();
  }

  std::unique_ptr<DataReductionProxyConfig> BuildConfig(
      std::unique_ptr<DataReductionProxyParams> params) {
    return std::make_unique<DataReductionProxyConfig>(
        network::TestNetworkConnectionTracker::GetInstance(), std::move(params),
        test_context_->configurator());
  }

  MockDataReductionProxyConfig* mock_config() {
    DCHECK(mock_config_used_);
    return test_context_->mock_config();
  }

  TestDataReductionProxyConfig* test_config() {
    DCHECK(!mock_config_used_);
    return test_context_->config();
  }

  DataReductionProxyConfigurator* configurator() const {
    return test_context_->configurator();
  }

  TestDataReductionProxyParams* params() const {
    return expected_params_.get();
  }

  void SetConnectionType(network::mojom::ConnectionType connection_type) {
    network::TestNetworkConnectionTracker::GetInstance()->SetConnectionType(
        connection_type);
  }

  std::vector<net::ProxyServer> GetConfiguredProxiesForHttp() const {
    return test_context_->GetConfiguredProxiesForHttp();
  }

  content::BrowserTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  std::unique_ptr<DataReductionProxyTestContext> test_context_;

 private:
  std::unique_ptr<TestDataReductionProxyParams> expected_params_;

  bool mock_config_used_;
};

TEST_F(DataReductionProxyConfigTest, TestReloadConfigHoldback) {
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));

  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings();

  test_config()->UpdateConfigForTesting(true, false, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, TestOnConnectionChangePersistedData) {
  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});

  ResetSettings();
  test_config()->SetProxyConfig(true, true);

  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  test_config()->SetCurrentNetworkID("wifi,test");
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  test_config()->UpdateConfigForTesting(true /* enabled */,
                                        false /* secure_proxies_allowed */,
                                        true /* insecure_proxies_allowed */);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  base::RunLoop().RunUntilIdle();

  test_config()->SetCurrentNetworkID("cell,test");
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_2G);
  base::RunLoop().RunUntilIdle();
  test_config()->UpdateConfigForTesting(true /* enabled */,
                                        false /* secure_proxies_allowed */,
                                        false /* insecure_proxies_allowed */);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());

  // On network change, persisted config should be read, and config reloaded.
  test_config()->SetCurrentNetworkID("wifi,test");
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

// Flaky on Linux. http://crbug.com/973385
// Flaky on Win. http://crbug.com/1010685
#if defined(OS_LINUX) || defined(OS_WIN)
#define MAYBE_TestOnNetworkChanged DISABLED_TestOnNetworkChanged
#else
#define MAYBE_TestOnNetworkChanged TestOnNetworkChanged
#endif
TEST_F(DataReductionProxyConfigTest, MAYBE_TestOnNetworkChanged) {
  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  RecreateContextWithMockConfig();
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  int kInvalidResponseCode = -1;

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled initially.
  mock_config()->UpdateConfigForTesting(true, true, true);
  mock_config()->OnNewClientConfigFetched();

  // Connection change triggers a secure proxy check that succeeds. Proxy
  // remains unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "OK", false,
      net::HTTP_OK, net::OK, SUCCEEDED_PROXY_ALREADY_ENABLED,
      {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that succeeds but captive
  // portal fails. Proxy is restricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "OK", true, net::HTTP_OK,
      net::OK, SUCCEEDED_PROXY_ALREADY_ENABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "Bad", false,
      net::HTTP_OK, net::OK, FAILED_PROXY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that succeeds. Proxies
  // are unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "OK", false,
      net::HTTP_OK, net::OK, SUCCEEDED_PROXY_ENABLED,
      {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails. Proxy is
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "Bad", true,
      net::HTTP_OK, net::OK, FAILED_PROXY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, std::string(), false,
      kInvalidResponseCode, net::ERR_INTERNET_DISCONNECTED,
      INTERNET_DISCONNECTED, std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that fails. Proxy remains
  // restricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "Bad", false,
      net::HTTP_OK, net::OK, FAILED_PROXY_ALREADY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));

  // Connection change triggers a secure proxy check that succeeds. Proxy is
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "OK", false,
      net::HTTP_OK, net::OK, SUCCEEDED_PROXY_ENABLED,
      {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails due to the
  // network changing again. This should be ignored, so the proxy should remain
  // unrestricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, std::string(), false,
      kInvalidResponseCode, net::ERR_INTERNET_DISCONNECTED,
      INTERNET_DISCONNECTED, {kHttpsProxy, kHttpProxy});

  // Connection change triggers a secure proxy check that fails because of a
  // redirect response, e.g. by a captive portal. Proxy is restricted.
  CheckSecureProxyCheckOnNetworkChange(
      network::mojom::ConnectionType::CONNECTION_WIFI, "Bad", false,
      net::HTTP_FOUND, net::ERR_ABORTED, FAILED_PROXY_DISABLED,
      std::vector<net::ProxyServer>(1, kHttpProxy));
}

// Verifies that the warm up URL is fetched correctly.
TEST_F(DataReductionProxyConfigTest, DISABLED_WarmupURL) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);

  // Set up the embedded test server from where the warm up URL will be fetched.
  net::EmbeddedTestServer embedded_test_server;
  embedded_test_server.AddDefaultHandlers(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  EXPECT_TRUE(embedded_test_server.Start());

  GURL warmup_url = embedded_test_server.GetURL("/simple.html");

  const struct {
    bool data_reduction_proxy_enabled;
  } tests[] = {
      {
          false,
      },
      {
          true,
      },
  };
  for (const auto& test : tests) {
    SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
    ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kDisableDataReductionProxyWarmupURLFetch));

    ResetSettings();

    variations::testing::ClearAllVariationParams();
    std::map<std::string, std::string> variation_params;

    test_context_->DisableWarmupURLFetchCallback();

    ASSERT_TRUE(variations::AssociateVariationParams(
        params::GetQuicFieldTrialName(), "Enabled", variation_params));

    base::FieldTrialList field_trial_list(nullptr);
    base::FieldTrialList::CreateFieldTrial(params::GetQuicFieldTrialName(),
                                           "Enabled");

    TestDataReductionProxyConfig config(configurator());

    NetworkPropertiesManager network_properties_manager(
        base::DefaultClock::GetInstance(), test_context_->pref_service());
    config.Initialize(
        test_context_->url_loader_factory(),
        base::BindRepeating([](const std::vector<DataReductionProxyServer>&) {
          return network::mojom::CustomProxyConfig::New();
        }),
        &network_properties_manager, std::string() /* user_agent*/);
    RunUntilIdle();

    {
      base::HistogramTester histogram_tester;

      // Set the connection type to WiFi so that warm up URL is fetched even if
      // the test device does not have connectivity.
      config.connection_type_ = network::mojom::ConnectionType::CONNECTION_WIFI;
      config.SetProxyConfig(test.data_reduction_proxy_enabled, true);
      ASSERT_TRUE(params::FetchWarmupProbeURLEnabled());

      if (test.data_reduction_proxy_enabled) {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchInitiated", 1, 1);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            0 /* kFetchInitiated */, 1);
      } else {
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 1);
      }
    }

    {
      base::HistogramTester histogram_tester;
      // Set the connection type to 4G so that warm up URL is fetched even if
      // the test device does not have connectivity.

      SetConnectionType(network::mojom::ConnectionType::CONNECTION_4G);
      RunUntilIdle();

      if (test.data_reduction_proxy_enabled) {
        EXPECT_EQ(1, histogram_tester.GetBucketCount(
                         "DataReductionProxy.WarmupURL.FetchInitiated", 1));
      } else {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchSuccessful", 0);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 2);
      }
    }

    {
      base::HistogramTester histogram_tester;
      // Warm up URL should not be fetched since the device does not have
      // connectivity.
      SetConnectionType(network::mojom::ConnectionType::CONNECTION_NONE);
      RunUntilIdle();

      if (test.data_reduction_proxy_enabled) {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        EXPECT_LE(1, histogram_tester.GetBucketCount(
                         "DataReductionProxy.WarmupURL.FetchAttemptEvent",
                         1 /* kConnectionTypeNone */));

      } else {
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchInitiated", 0);
        histogram_tester.ExpectTotalCount(
            "DataReductionProxy.WarmupURL.FetchSuccessful", 0);
        histogram_tester.ExpectUniqueSample(
            "DataReductionProxy.WarmupURL.FetchAttemptEvent",
            2 /* kProxyNotEnabledByUser */, 2);
      }
    }
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassed) {
  const struct {
    // proxy flags
    bool allowed;
    bool fallback_allowed;
    // is https request
    bool is_https;
    // proxies in retry map
    bool origin;
    bool fallback_origin;

    bool expected_result;
  } tests[] = {
      {
          // proxy flags
          false, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, false,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          false, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, false,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          true, true,
          // expected result
          true,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          false, false,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          false,
          // proxies in retry map
          false, true,
          // expected result
          false,
      },
      {
          // proxy flags
          true, true,
          // is https request
          true,
          // proxies in retry map
          true, true,
          // expected result
          false,
      },
  };

  // The retry map has the scheme prefix for https but not for http.
  std::string origin = GetRetryMapKeyFromOrigin(params()
                                                    ->proxies_for_http()
                                                    .front()
                                                    .proxy_server()
                                                    .host_port_pair()
                                                    .ToString());
  std::string fallback_origin =
      GetRetryMapKeyFromOrigin(params()
                                   ->proxies_for_http()
                                   .at(1)
                                   .proxy_server()
                                   .host_port_pair()
                                   .ToString());

  for (size_t i = 0; i < base::size(tests); ++i) {
    net::ProxyConfig::ProxyRules rules;
    std::vector<std::string> proxies;

    if (tests[i].allowed)
      proxies.push_back(origin);
    if (tests[i].allowed && tests[i].fallback_allowed)
      proxies.push_back(fallback_origin);

    std::string proxy_rules = "http=" + base::JoinString(proxies, ",") +
                              ",direct://;";

    rules.ParseFromString(proxy_rules);

    std::unique_ptr<TestDataReductionProxyParams> params(
        new TestDataReductionProxyParams());
    std::unique_ptr<DataReductionProxyConfig> config =
        BuildConfig(std::move(params));

    net::ProxyRetryInfoMap retry_map;
    net::ProxyRetryInfo retry_info;
    retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();

    if (tests[i].origin)
      retry_map[origin] = retry_info;
    if (tests[i].fallback_origin)
      retry_map[fallback_origin] = retry_info;

    bool was_bypassed = config->AreProxiesBypassed(retry_map, rules,
                                                   tests[i].is_https, nullptr);

    EXPECT_EQ(tests[i].expected_result, was_bypassed) << i;
  }
}

TEST_F(DataReductionProxyConfigTest, AreProxiesBypassedRetryDelay) {
  std::string origin = GetRetryMapKeyFromOrigin(params()
                                                    ->proxies_for_http()
                                                    .front()
                                                    .proxy_server()
                                                    .host_port_pair()
                                                    .ToString());
  std::string fallback_origin =
      GetRetryMapKeyFromOrigin(params()
                                   ->proxies_for_http()
                                   .at(1)
                                   .proxy_server()
                                   .host_port_pair()
                                   .ToString());

  net::ProxyConfig::ProxyRules rules;
  std::vector<std::string> proxies;

  proxies.push_back(origin);
  proxies.push_back(fallback_origin);

  std::string proxy_rules =
      "http=" + base::JoinString(proxies, ",") + ",direct://;";

  rules.ParseFromString(proxy_rules);

  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams());
  std::unique_ptr<DataReductionProxyConfig> config =
      BuildConfig(std::move(params));

  net::ProxyRetryInfoMap retry_map;
  net::ProxyRetryInfo retry_info;

  retry_info.bad_until = base::TimeTicks() + base::TimeDelta::Max();
  retry_map[origin] = retry_info;

  retry_info.bad_until = base::TimeTicks();
  retry_map[fallback_origin] = retry_info;

  bool was_bypassed =
      config->AreProxiesBypassed(retry_map, rules, false, nullptr);

  EXPECT_FALSE(was_bypassed);

  base::TimeDelta delay = base::TimeDelta::FromHours(2);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[origin] = retry_info;

  delay = base::TimeDelta::FromHours(1);
  retry_info.bad_until = base::TimeTicks::Now() + delay;
  retry_info.current_delay = delay;
  retry_map[fallback_origin] = retry_info;

  base::TimeDelta min_retry_delay;
  was_bypassed = config->AreProxiesBypassed(retry_map,
                                            rules,
                                            false,
                                            &min_retry_delay);
  EXPECT_TRUE(was_bypassed);
  EXPECT_EQ(delay, min_retry_delay);
}

TEST_F(DataReductionProxyConfigTest,
       FindConfiguredDataReductionProxyWithParams) {
  std::unique_ptr<TestDataReductionProxyParams> params(
      new TestDataReductionProxyParams());

  const std::vector<DataReductionProxyServer> expected_proxies =
      params->proxies_for_http();
  ASSERT_LT(0U, expected_proxies.size());

  DataReductionProxyConfig config(
      network::TestNetworkConnectionTracker::GetInstance(), std::move(params),
      configurator());

  for (size_t expected_proxy_index = 0U;
       expected_proxy_index < expected_proxies.size(); ++expected_proxy_index) {
    base::Optional<DataReductionProxyTypeInfo> proxy_type_info =
        config.FindConfiguredDataReductionProxy(
            expected_proxies[expected_proxy_index].proxy_server());

    ASSERT_TRUE(proxy_type_info.has_value());
    EXPECT_EQ(expected_proxies, proxy_type_info->proxy_servers);
    EXPECT_EQ(expected_proxy_index, proxy_type_info->proxy_index);
  }
}

TEST_F(DataReductionProxyConfigTest,
       FindConfiguredDataReductionProxyWithMutableConfig) {
  std::vector<DataReductionProxyServer> proxies_for_http;
  proxies_for_http.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP)));
  proxies_for_http.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "http://origin.net:80", net::ProxyServer::SCHEME_HTTP)));

  proxies_for_http.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      "quic://anotherorigin.net:443", net::ProxyServer::SCHEME_HTTP)));

  const struct {
    DataReductionProxyServer proxy_server;
    bool expected_result;
    size_t expected_proxy_index;
  } tests[] = {
      {proxies_for_http[0], true, 0U},
      {proxies_for_http[1], true, 1U},
      {proxies_for_http[2], true, 2U},
      {DataReductionProxyServer(net::ProxyServer()), false, 0U},
      {DataReductionProxyServer(net::ProxyServer(
           net::ProxyServer::SCHEME_HTTPS,
           net::HostPortPair::FromString("otherorigin.net:443"))),
       false, 0U},

      // Verifies that when determining if a proxy is a valid data reduction
      // proxy, only the host port pairs are compared.
      {DataReductionProxyServer(net::ProxyServer::FromURI(
           "origin.net:443", net::ProxyServer::SCHEME_QUIC)),
       true, 0U},

      {DataReductionProxyServer(net::ProxyServer::FromURI(
           "origin2.net:443", net::ProxyServer::SCHEME_HTTPS)),
       false, 0U},
      {DataReductionProxyServer(net::ProxyServer::FromURI(
           "origin2.net:443", net::ProxyServer::SCHEME_QUIC)),
       false, 0U},
  };

  std::unique_ptr<DataReductionProxyMutableConfigValues> config_values =
      std::make_unique<DataReductionProxyMutableConfigValues>();

  config_values->UpdateValues(proxies_for_http);
  std::unique_ptr<DataReductionProxyConfig> config(new DataReductionProxyConfig(
      network::TestNetworkConnectionTracker::GetInstance(),
      std::move(config_values), configurator()));
  for (const auto& test : tests) {
    base::Optional<DataReductionProxyTypeInfo> proxy_type_info =
        config->FindConfiguredDataReductionProxy(
            test.proxy_server.proxy_server());
    EXPECT_EQ(test.expected_result, proxy_type_info.has_value());

    if (proxy_type_info) {
      EXPECT_EQ(proxies_for_http, proxy_type_info->proxy_servers);
      EXPECT_EQ(test.expected_proxy_index, proxy_type_info->proxy_index);
    }
  }
}

TEST_F(DataReductionProxyConfigTest, HandleWarmupFetcherResponse) {
  base::HistogramTester histogram_tester;
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kNonDataSaverProxy = net::ProxyServer::FromURI(
      "https://non-data-saver-proxy.net:443", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled.
  test_config()->UpdateConfigForTesting(true, true, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Set the details of the proxy to which the warmup URL probe is in-flight to
  // avoid triggering the DCHECKs in HandleWarmupFetcherResponse method.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  // Report successful warmup of |kHttpsProxy|.
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kSuccessful);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 1);

  // Report failed warmup |kHttpsProxy| and verify it is removed from the list
  // of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 1);

  // Report failed warmup |kHttpsProxy| again, and verify it does not change the
  // list of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 2);

  // |kHttpsProxy| should now be added back to the list of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kSuccessful);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 2);

  // Report successful warmup |kHttpsProxy| again, and verify that there is no
  // change in the list of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kSuccessful);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 3);

  // |kHttpsProxy| should be removed again from the list of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 3);

  // Now report failed warmup for |kHttpProxy| and verify that it is also
  // removed from the list of proxies.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(false /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      0, 1);

  // Both proxies should be added back.
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(true /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kSuccessful);
  test_config()->SetInFlightWarmupProxyDetails(
      std::make_pair(false /* is_secure_proxy */, true /* is_core_proxy */));
  test_config()->HandleWarmupFetcherResponse(
      kHttpProxy, WarmupURLFetcher::FetchResult::kSuccessful);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 4);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      0, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      1, 1);

  // If the warmup URL is unsuccessfully fetched using a non-data saver proxy,
  // then there is no change in the list of proxies.
  test_config()->HandleWarmupFetcherResponse(
      kNonDataSaverProxy, WarmupURLFetcher::FetchResult::kFailed);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

// Tests that the proxy server last used for fetching the warmup URL is marked
// as failed when the warmup fetched callback returns an invalid proxy.
TEST_F(DataReductionProxyConfigTest,
       HandleWarmupFetcherResponse_InvalidProxyServer) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled.
  test_config()->UpdateConfigForTesting(true, true, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Report failed warmup for a non-DataSaver proxy, and verify that it
  // changes the list of data saver proxies.
  test_config()->HandleWarmupFetcherResponse(
      net::ProxyServer(),
      WarmupURLFetcher::FetchResult::kFailed /* success_response */);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

// Tests that the proxy server last used for fetching the warmup URL is marked
// as failed when the warmup fetched callback returns a direct proxy.
TEST_F(DataReductionProxyConfigTest,
       HandleWarmupFetcherResponse_DirectProxyServer) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // The proxy is enabled.
  test_config()->UpdateConfigForTesting(true, true, true);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  // Report failed warmup for a non-DataSaver proxy, and verify that it
  // changes the list of data saver proxies.
  test_config()->HandleWarmupFetcherResponse(
      net::ProxyServer::Direct(),
      WarmupURLFetcher::FetchResult::kFailed /* success_response */);
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
}

TEST_F(DataReductionProxyConfigTest, HandleWarmupFetcherRetry) {
  constexpr size_t kMaxWarmupURLFetchAttempts = 3;

  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  base::HistogramTester histogram_tester;
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // Enable the proxy.
  test_config()->SetWarmupURLFetchAttemptCounts(0);
  test_config()->UpdateConfigForTesting(true, true, true);

  test_config()->SetIsFetchInFlight(true);

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 0);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.NonCore",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1);

  // The first probe should go through the HTTPS data saver proxy.
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 1);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 2);
  test_config()->SetInFlightWarmupProxyDetails(base::nullopt);
  EXPECT_EQ(std::make_pair(false, true),
            test_config()->GetInFlightWarmupProxyDetails());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 2);

  // The second probe should go through the HTTP data saver proxy.
  test_config()->HandleWarmupFetcherResponse(
      kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      0, 1);
  EXPECT_EQ(std::make_pair(true, true),
            test_config()->GetInFlightWarmupProxyDetails());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 3);

  EXPECT_EQ(std::make_pair(true, true),
            test_config()->GetInFlightWarmupProxyDetails());

  for (size_t i = 1; i <= 4; ++i) {
    // Two more probes should go through the HTTPS data saver proxy, and two
    // more probes through the HTTP proxy.
    if (i <= 2) {
      EXPECT_EQ(std::make_pair(true, true),
                test_config()->GetInFlightWarmupProxyDetails());

      test_config()->HandleWarmupFetcherResponse(
          kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
    } else {
      EXPECT_EQ(std::make_pair(false, true),
                test_config()->GetInFlightWarmupProxyDetails());

      test_config()->HandleWarmupFetcherResponse(
          kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
    }
    base::RunLoop().RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.WarmupURL.FetchInitiated",
        std::min(3 + i,
                 kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts));
  }
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      1, 0);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 3);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      1, 0);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.Core",
      0, 3);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 6);

  for (size_t i = 1; i <= 10; ++i) {
    // Set the details of the proxy to which the warmup URL probe is in-flight
    // to avoid triggering the DCHECKs in HandleWarmupFetcherResponse method.
    test_config()->SetInFlightWarmupProxyDetails(
        std::make_pair(false /* is_secure_proxy */, true /* is_core_proxy */));

    // Fetcher callback should not trigger fetching of probe URL since
    // kMaxWarmupURLFetchAttempts probes have been tried through each of the
    // data saver proxy.
    test_config()->HandleWarmupFetcherResponse(
        kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
    base::RunLoop().RunUntilIdle();
    // At most kMaxWarmupURLFetchAttempts warmup URLs should be fetched via
    // each of the two insecure proxies.
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.WarmupURL.FetchInitiated",
        kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts);
  }

  test_config()->SetInFlightWarmupProxyDetails(base::nullopt);
}

// Tests the behavior when warmup URL fetcher times out.
TEST_F(DataReductionProxyConfigTest, HandleWarmupFetcherTimeout) {
  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  base::HistogramTester histogram_tester;
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // Enable the proxy.
  test_config()->SetWarmupURLFetchAttemptCounts(0);
  test_config()->UpdateConfigForTesting(true, true, true);

  test_config()->SetIsFetchInFlight(true);

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 0);
  test_config()->OnNewClientConfigFetched();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
            GetConfiguredProxiesForHttp());

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "InsecureProxy.NonCore",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.NonCore",
      0);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1);

  // The first probe should go through the HTTPS data saver proxy. On fetch
  // timeout, the HTTPS proxy must be disabled even though the callback did
  // not specify a proxy.
  test_config()->HandleWarmupFetcherResponse(
      net::ProxyServer(), WarmupURLFetcher::FetchResult::kTimedOut);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({kHttpProxy}),
            GetConfiguredProxiesForHttp());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.WarmupURLFetcherCallback.SuccessfulFetch."
      "SecureProxy.Core",
      0, 1);

  // Warmup URL should be fetched from the next proxy.
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 2);
}

// https://crbug.com/974895: Flaky test.
TEST_F(DataReductionProxyConfigTest,
       DISABLED_HandleWarmupFetcherRetryWithConnectionChange) {
  // The test manually controls the fetch of warmup URL and the response.
  test_context_->DisableWarmupURLFetchCallback();

  constexpr size_t kMaxWarmupURLFetchAttempts = 3;

  base::HistogramTester histogram_tester;
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "fallback.net:80", net::ProxyServer::SCHEME_HTTP);

  SetProxiesForHttpOnCommandLine({kHttpsProxy, kHttpProxy});
  ResetSettings();

  // Enable the proxy.
  test_config()->SetWarmupURLFetchAttemptCounts(0);
  test_config()->UpdateConfigForTesting(true, true, true);

  test_config()->SetIsFetchInFlight(true);

  test_config()->OnNewClientConfigFetched();

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 1);

  // The first probe should go through the HTTPS data saver proxy.
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 2);
  test_config()->SetInFlightWarmupProxyDetails(base::nullopt);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 2);

  // The second probe should go through the HTTP data saver proxy.
  test_config()->HandleWarmupFetcherResponse(
      kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 3);

  EXPECT_EQ(std::make_pair(true, true),
            test_config()->GetInFlightWarmupProxyDetails());

  for (size_t i = 1; i <= 4; ++i) {
    // Two more probes should go through the HTTPS data saver proxy, and two
    // more probes through the HTTP proxy.
    if (i <= 2) {
      test_config()->HandleWarmupFetcherResponse(
          kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
    } else {
      test_config()->HandleWarmupFetcherResponse(
          kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
    }
    base::RunLoop().RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.WarmupURL.FetchInitiated",
        std::min(3 + i,
                 kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts));
  }
  EXPECT_EQ(std::make_pair(false, true),
            test_config()->GetInFlightWarmupProxyDetails());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated", 6);

  test_config()->SetInFlightWarmupProxyDetails(base::nullopt);
  EXPECT_EQ(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());

  // A change in the connection type should reset the probe fetch attempt count,
  // and trigger fetching of the probe URL.
  test_config()->SetCurrentNetworkID("wifi,test");
  SetConnectionType(network::mojom::ConnectionType::CONNECTION_WIFI);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::make_pair(true, true),
            test_config()->GetInFlightWarmupProxyDetails());
  EXPECT_NE(std::vector<net::ProxyServer>({}), GetConfiguredProxiesForHttp());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated",
      kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts + 1);

  // At most kMaxWarmupURLFetchAttempts warmup URLs should be fetched via
  // secure proxy, and kMaxWarmupURLFetchAttempts via insecure.
  test_config()->HandleWarmupFetcherResponse(
      kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  test_config()->HandleWarmupFetcherResponse(
      kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
  base::RunLoop().RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated",
      kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts + 3);

  for (size_t i = 1; i <= 2; ++i) {
    test_config()->HandleWarmupFetcherResponse(
        kHttpsProxy, WarmupURLFetcher::FetchResult::kFailed);
    base::RunLoop().RunUntilIdle();
  }
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated",
      kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts + 5);

  for (size_t i = 1; i <= 2; ++i) {
    test_config()->HandleWarmupFetcherResponse(
        kHttpProxy, WarmupURLFetcher::FetchResult::kFailed);
    base::RunLoop().RunUntilIdle();
  }

  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.WarmupURL.FetchInitiated",
      kMaxWarmupURLFetchAttempts + kMaxWarmupURLFetchAttempts + 6);
}

}  // namespace data_reduction_proxy
