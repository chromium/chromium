// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/simple_test_clock.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_compression_stats.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_settings_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_registry_simple.h"
#include "net/base/proxy_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

class DataReductionProxySettingsTest
    : public ConcreteDataReductionProxySettingsTest<
          DataReductionProxySettings> {
 public:
  void CheckMaybeActivateDataReductionProxy(bool initially_enabled,
                                            bool request_succeeded,
                                            bool expected_enabled,
                                            bool expected_restricted,
                                            bool expected_fallback_restricted) {
    test_context_->SetDataReductionProxyEnabled(initially_enabled);
    test_context_->config()->UpdateConfigForTesting(initially_enabled,
                                                    request_succeeded, true);
    ExpectSetProxyPrefs(expected_enabled, false);
    settings_->MaybeActivateDataReductionProxy(false);
    test_context_->RunUntilIdle();
  }

  void InitPrefMembers() {
    settings_->set_data_reduction_proxy_enabled_pref_name_for_test(
        test_context_->GetDataReductionProxyEnabledPrefName());
    settings_->InitPrefMembers();
  }
};

TEST_F(DataReductionProxySettingsTest, TestIsProxyEnabledOrManaged) {
  InitPrefMembers();
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), test_context_->pref_service(),
      test_context_->task_runner());
  test_context_->config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);

  // The proxy is disabled initially.
  test_context_->config()->UpdateConfigForTesting(false, true, true);

  EXPECT_FALSE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_FALSE(settings_->IsDataReductionProxyManaged());

  CheckOnPrefChange(true, true, true);
  EXPECT_TRUE(settings_->IsDataReductionProxyEnabled());
  EXPECT_TRUE(settings_->IsDataReductionProxyManaged());

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestCanUseDataReductionProxy) {
  InitPrefMembers();
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), test_context_->pref_service(),
      test_context_->task_runner());
  test_context_->config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);

  // The proxy is disabled initially.
  test_context_->config()->UpdateConfigForTesting(false, true, true);

  GURL http_gurl("http://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(http_gurl));

  CheckOnPrefChange(true, true, false);
  EXPECT_TRUE(settings_->CanUseDataReductionProxy(http_gurl));

  GURL https_gurl("https://url.com/");
  EXPECT_FALSE(settings_->CanUseDataReductionProxy(https_gurl));

  test_context_->RunUntilIdle();
}

TEST_F(DataReductionProxySettingsTest, TestResetDataReductionStatistics) {
  int64_t original_content_length;
  int64_t received_content_length;
  int64_t last_update_time;
  settings_->ResetDataReductionStatistics();
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(0L, original_content_length);
  EXPECT_EQ(0L, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);
}

TEST_F(DataReductionProxySettingsTest, TestContentLengths) {
  int64_t original_content_length;
  int64_t received_content_length;
  int64_t last_update_time;

  // Request |kNumDaysInHistory| days.
  settings_->GetContentLengths(kNumDaysInHistory,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  const unsigned int days = kNumDaysInHistory;
  // Received content length history values are 0 to |kNumDaysInHistory - 1|.
  int64_t expected_total_received_content_length = (days - 1L) * days / 2;
  // Original content length history values are 0 to
  // |2 * (kNumDaysInHistory - 1)|.
  long expected_total_original_content_length = (days - 1L) * days;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
  EXPECT_EQ(last_update_time_.ToInternalValue(), last_update_time);

  // Request |kNumDaysInHistory - 1| days.
  settings_->GetContentLengths(kNumDaysInHistory - 1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length -= (days - 1);
  expected_total_original_content_length -= 2 * (days - 1);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 0 days.
  settings_->GetContentLengths(0,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  expected_total_received_content_length = 0;
  expected_total_original_content_length = 0;
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);

  // Request 1 day. First day had 0 bytes so should be same as 0 days.
  settings_->GetContentLengths(1,
                               &original_content_length,
                               &received_content_length,
                               &last_update_time);
  EXPECT_EQ(expected_total_original_content_length, original_content_length);
  EXPECT_EQ(expected_total_received_content_length, received_content_length);
}

TEST(DataReductionProxySettingsStandaloneTest, TestEndToEndSecureProxyCheck) {
  const net::ProxyServer kHttpsProxy = net::ProxyServer::FromURI(
      "https://secure_origin.net:443", net::ProxyServer::SCHEME_HTTP);
  const net::ProxyServer kHttpProxy = net::ProxyServer::FromURI(
      "insecure_origin.net:80", net::ProxyServer::SCHEME_HTTP);
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyHttpProxies,
      kHttpsProxy.ToURI() + ";" + kHttpProxy.ToURI());

  base::MessageLoopForIO message_loop;
  struct TestCase {
    const char* response_headers;
    const char* response_body;
    net::Error net_error_code;
    bool expected_restricted;
  };
  const TestCase kTestCases[] {
    { "HTTP/1.1 200 OK\r\n\r\n",
      "OK", net::OK, false,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "Bad", net::OK, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_FAILED, true,
    },
    { "HTTP/1.1 200 OK\r\n\r\n",
      "", net::ERR_ABORTED, true,
    },
    // The secure proxy check shouldn't attempt to follow the redirect.
    { "HTTP/1.1 302 Found\r\nLocation: http://www.google.com/\r\n\r\n",
      "", net::OK, true,
    },
  };

  for (const TestCase& test_case : kTestCases) {
    network::TestURLLoaderFactory test_url_loader_factory;
    auto test_shared_url_loader_factory =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory);

    std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
        DataReductionProxyTestContext::Builder()
            .WithURLLoaderFactory(test_shared_url_loader_factory)
            .SkipSettingsInitialization()
            .Build();

    drp_test_context->DisableWarmupURLFetch();

    // Start with the Data Reduction Proxy disabled.
    drp_test_context->SetDataReductionProxyEnabled(false);
    drp_test_context->InitSettings();
    drp_test_context->RunUntilIdle();

    // Toggle the pref to trigger the secure proxy check.
    drp_test_context->SetDataReductionProxyEnabled(true);
    drp_test_context->RunUntilIdle();

    network::ResourceResponseHead resource_response_head;
    std::string headers(test_case.response_headers);
    resource_response_head.headers = new net::HttpResponseHeaders(
        net::HttpUtil::AssembleRawHeaders(headers.c_str(), headers.size()));
    test_url_loader_factory.SimulateResponseWithoutRemovingFromPendingList(
        test_url_loader_factory.GetPendingRequest(0), resource_response_head,
        test_case.response_body,
        network::URLLoaderCompletionStatus(test_case.net_error_code));

    if (test_case.expected_restricted) {
      EXPECT_EQ(std::vector<net::ProxyServer>(1, kHttpProxy),
                drp_test_context->GetConfiguredProxiesForHttp());
    } else {
      EXPECT_EQ(std::vector<net::ProxyServer>({kHttpsProxy, kHttpProxy}),
                drp_test_context->GetConfiguredProxiesForHttp());
    }
  }
}

TEST(DataReductionProxySettingsStandaloneTest, TestOnProxyEnabledPrefChange) {
  base::MessageLoopForIO message_loop;
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithMockConfig()
          .WithMockDataReductionProxyService()
          .SkipSettingsInitialization()
          .Build();

  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), drp_test_context->pref_service(),
      drp_test_context->task_runner());
  drp_test_context->config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);

  // The proxy is enabled initially.
  drp_test_context->config()->UpdateConfigForTesting(true, true, true);
  drp_test_context->InitSettings();

  MockDataReductionProxyService* mock_service =
      static_cast<MockDataReductionProxyService*>(
          drp_test_context->data_reduction_proxy_service());

  // The pref is disabled, so correspondingly should be the proxy.
  EXPECT_CALL(*mock_service, SetProxyPrefs(false, false));
  drp_test_context->SetDataReductionProxyEnabled(false);

  // The pref is enabled, so correspondingly should be the proxy.
  EXPECT_CALL(*mock_service, SetProxyPrefs(true, false));
  drp_test_context->SetDataReductionProxyEnabled(true);
}

TEST_F(DataReductionProxySettingsTest, TestMaybeActivateDataReductionProxy) {
  // Initialize the pref member in |settings_| without the usual callback
  // so it won't trigger MaybeActivateDataReductionProxy when the pref value
  // is set.
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), test_context_->pref_service(),
      test_context_->task_runner());
  test_context_->config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);

  settings_->spdy_proxy_auth_enabled_.Init(
      test_context_->GetDataReductionProxyEnabledPrefName(),
      settings_->GetOriginalProfilePrefs());

  // TODO(bengr): Test enabling/disabling while a secure proxy check is
  // outstanding.
  // The proxy is enabled and unrestricted initially.
  // Request succeeded but with bad response, expect proxy to be restricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, true, false);
  // Request succeeded with valid response, expect proxy to be unrestricted.
  CheckMaybeActivateDataReductionProxy(true, true, true, false, false);
  // Request failed, expect proxy to be enabled but restricted.
  CheckMaybeActivateDataReductionProxy(true, false, true, true, false);
  // The proxy is disabled initially. No secure proxy checks should take place,
  // and so the state should not change.
  CheckMaybeActivateDataReductionProxy(false, true, false, false, false);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOn) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  test_context_->SetDataReductionProxyEnabled(true);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestInitDataReductionProxyOff) {
  // InitDataReductionProxySettings with the preference off will directly call
  // LogProxyState.
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_DISABLED));

  test_context_->SetDataReductionProxyEnabled(false);
  InitDataReductionProxy(false);
  CheckDataReductionProxySyntheticTrial(false);
}

TEST_F(DataReductionProxySettingsTest, TestEnableProxyFromCommandLine) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));

  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableDataReductionProxy);
  InitDataReductionProxy(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestSetDataReductionProxyEnabled) {
  MockSettings* settings = static_cast<MockSettings*>(settings_.get());
  EXPECT_CALL(*settings, RecordStartupState(PROXY_ENABLED));
  test_context_->SetDataReductionProxyEnabled(true);
  InitDataReductionProxy(true);

  ExpectSetProxyPrefs(false, false);
  settings_->SetDataReductionProxyEnabled(false);
  test_context_->RunUntilIdle();
  CheckDataReductionProxySyntheticTrial(false);

  ExpectSetProxyPrefs(true, false);
  settings->SetDataReductionProxyEnabled(true);
  CheckDataReductionProxySyntheticTrial(true);
}

TEST_F(DataReductionProxySettingsTest, TestSettingsEnabledStateHistograms) {
  const char kUMAEnabledState[] = "DataReductionProxy.EnabledState";
  base::HistogramTester histogram_tester;

  InitPrefMembers();
  settings_->data_reduction_proxy_service_->SetIOData(
      test_context_->io_data()->GetWeakPtr());

  // No settings state histograms should be recorded during startup.
  test_context_->RunUntilIdle();
  histogram_tester.ExpectTotalCount(kUMAEnabledState, 0);

  settings_->SetDataReductionProxyEnabled(true);
  test_context_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUMAEnabledState, DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON, 1);
  histogram_tester.ExpectBucketCount(
      kUMAEnabledState, DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF, 0);

  settings_->SetDataReductionProxyEnabled(false);
  test_context_->RunUntilIdle();
  histogram_tester.ExpectBucketCount(
      kUMAEnabledState, DATA_REDUCTION_SETTINGS_ACTION_OFF_TO_ON, 1);
  histogram_tester.ExpectBucketCount(
      kUMAEnabledState, DATA_REDUCTION_SETTINGS_ACTION_ON_TO_OFF, 1);
}

// Verify that the UMA metric and the pref is recorded correctly when the user
// enables the data reduction proxy.
TEST_F(DataReductionProxySettingsTest, TestDaysSinceEnabledWithTestClock) {
  const char kUMAEnabledState[] = "DataReductionProxy.DaysSinceEnabled";
  base::SimpleTestClock clock;
  clock.Advance(base::TimeDelta::FromDays(1));
  ResetSettings(&clock);

  base::Time last_enabled_time = clock.Now();

  InitPrefMembers();
  {
    base::HistogramTester histogram_tester;
    settings_->data_reduction_proxy_service_->SetIOData(
        test_context_->io_data()->GetWeakPtr());

    test_context_->RunUntilIdle();
    histogram_tester.ExpectTotalCount(kUMAEnabledState, 0);

    // Enable data reduction proxy. The metric should be recorded.
    settings_->SetDataReductionProxyEnabled(true /* enabled */);
    test_context_->RunUntilIdle();

    last_enabled_time = clock.Now();

    EXPECT_EQ(
        last_enabled_time,
        base::Time::FromInternalValue(test_context_->pref_service()->GetInt64(
            prefs::kDataReductionProxyLastEnabledTime)));
    histogram_tester.ExpectUniqueSample(kUMAEnabledState, 0, 1);
  }

  {
    // Simulate turning off and on of data reduction proxy while Chromium is
    // running.
    settings_->SetDataReductionProxyEnabled(false /* enabled */);
    clock.Advance(base::TimeDelta::FromDays(1));
    base::HistogramTester histogram_tester;
    last_enabled_time = clock.Now();

    settings_->spdy_proxy_auth_enabled_.SetValue(true);
    settings_->MaybeActivateDataReductionProxy(false);
    test_context_->RunUntilIdle();
    histogram_tester.ExpectUniqueSample(kUMAEnabledState, 0, 1);
    EXPECT_EQ(
        last_enabled_time,
        base::Time::FromInternalValue(test_context_->pref_service()->GetInt64(
            prefs::kDataReductionProxyLastEnabledTime)));
  }

  {
    // Advance clock by a random number of days.
    int advance_clock_days = 42;
    clock.Advance(base::TimeDelta::FromDays(advance_clock_days));
    base::HistogramTester histogram_tester;
    // Simulate Chromium start up. Data reduction proxy was enabled
    // |advance_clock_days| ago.
    settings_->MaybeActivateDataReductionProxy(true);
    test_context_->RunUntilIdle();
    histogram_tester.ExpectUniqueSample(kUMAEnabledState, advance_clock_days,
                                        1);
    EXPECT_EQ(
        last_enabled_time,
        base::Time::FromInternalValue(test_context_->pref_service()->GetInt64(
            prefs::kDataReductionProxyLastEnabledTime)));
  }
}

// Verify that the pref and the UMA metric are not recorded for existing users
// that already have data reduction proxy on.
TEST_F(DataReductionProxySettingsTest, TestDaysSinceEnabledExistingUser) {
  InitPrefMembers();
  base::HistogramTester histogram_tester;
  settings_->data_reduction_proxy_service_->SetIOData(
      test_context_->io_data()->GetWeakPtr());
  test_context_->RunUntilIdle();

  // Simulate Chromium startup with data reduction proxy already enabled.
  settings_->spdy_proxy_auth_enabled_.SetValue(true);
  settings_->MaybeActivateDataReductionProxy(true /* at_startup */);
  test_context_->RunUntilIdle();
  histogram_tester.ExpectTotalCount("DataReductionProxy.DaysSinceEnabled", 0);
  EXPECT_EQ(0, test_context_->pref_service()->GetInt64(
                   prefs::kDataReductionProxyLastEnabledTime));
}

TEST_F(DataReductionProxySettingsTest, TestDaysSinceSavingsCleared) {
  base::SimpleTestClock clock;
  clock.Advance(base::TimeDelta::FromDays(1));
  ResetSettings(&clock);

  InitPrefMembers();
  base::HistogramTester histogram_tester;
  test_context_->pref_service()->SetInt64(
      prefs::kDataReductionProxySavingsClearedNegativeSystemClock,
      clock.Now().ToInternalValue());

  settings_->data_reduction_proxy_service_->SetIOData(
      test_context_->io_data()->GetWeakPtr());
  test_context_->RunUntilIdle();

  clock.Advance(base::TimeDelta::FromDays(100));

  // Simulate Chromium startup with data reduction proxy already enabled.
  settings_->spdy_proxy_auth_enabled_.SetValue(true);
  settings_->MaybeActivateDataReductionProxy(true /* at_startup */);
  test_context_->RunUntilIdle();
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.DaysSinceSavingsCleared.NegativeSystemClock", 100, 1);
}

TEST_F(DataReductionProxySettingsTest, TestGetDailyContentLengths) {
  ContentLengthList result =
      settings_->GetDailyContentLengths(prefs::kDailyHttpOriginalContentLength);

  ASSERT_FALSE(result.empty());
  ASSERT_EQ(kNumDaysInHistory, result.size());

  for (size_t i = 0; i < kNumDaysInHistory; ++i) {
    long expected_length =
        static_cast<long>((kNumDaysInHistory - 1 - i) * 2);
    ASSERT_EQ(expected_length, result[i]);
  }
}

}  // namespace data_reduction_proxy
