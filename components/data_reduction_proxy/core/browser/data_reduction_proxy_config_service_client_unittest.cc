// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/base64.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_features.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "components/prefs/pref_registry_simple.h"
#include "net/base/network_change_notifier.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.config_:
const char kSuccessOrigin[] = "https://origin.net:443";
const char kSuccessFallback[] = "fallback.net:80";
const char kSuccessSessionKey[] = "SecretSessionKey";

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.previous_config_:
const char kOldSuccessOrigin[] = "https://old.origin.net:443";
const char kOldSuccessFallback[] = "old.fallback.net:80";
const char kOldSuccessSessionKey[] = "OldSecretSessionKey";

// The following values should match those in
// DataReductionProxyConfigServiceClientTest.loaded_config_:
const char kPersistedFallback[] = "persisted.net:80";
const char kPersistedSessionKey[] = "PersistedSessionKey";

// Duration (in seconds) after which the config should be refreshed.
const int kConfigRefreshDurationSeconds = 600;

#if defined(OS_ANDROID)
// Maximum duration  to wait before fetching the config, while the application
// is in background.
const uint32_t kMaxBackgroundFetchIntervalSeconds = 6 * 60 * 60;  // 6 hours.
#endif

}  // namespace

namespace data_reduction_proxy {

class DataReductionProxyConfigServiceClientTest : public testing::Test {
 protected:
  DataReductionProxyConfigServiceClientTest(
      bool enable_aggressive_config_fetch_feature = false)
      : test_shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {
    if (enable_aggressive_config_fetch_feature) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kDataReductionProxyAggressiveConfigFetch);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kDataReductionProxyAggressiveConfigFetch);
    }
  }

  void SetUp() override {
    testing::Test::SetUp();

    // Install an interceptor here to process the queue of responses
    // (fed by calls to AddMock{Success,PreviousSuccess,Failure}.
    test_url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [&](const network::ResourceRequest& request) {
          // Some tests trigger loading, without having an respective
          // mock response set. Ignore such cases.
          if (mock_responses_.size() <= curr_mock_response_index_) {
            test_url_loader_factory_.pending_requests()->clear();
            return;
          }
          test_url_loader_factory_.ClearResponses();
          test_url_loader_factory_.AddResponse(
              request.url.spec(),
              mock_responses_[curr_mock_response_index_].response,
              mock_responses_[curr_mock_response_index_].http_code);
          curr_mock_response_index_++;
          RunUntilIdle();
        }));
  }

  void Init() {
    test_context_ = DataReductionProxyTestContext::Builder()
                        .WithURLLoaderFactory(test_shared_url_loader_factory_)
                        .WithMockRequestOptions()
                        .WithTestConfigClient()
                        .SkipSettingsInitialization()
                        .Build();

    // Disable fetching of warmup URL to avoid generating extra traffic which
    // would need to be satisfied using mock sockets.
    test_context_->DisableWarmupURLFetch();

    test_context_->InitSettings();
    ResetBackoffEntryReleaseTime();
    test_context_->test_config_client()->SetNow(base::Time::UnixEpoch());
    test_context_->test_config_client()->SetEnabled(true);
    test_context_->test_config_client()->SetConfigServiceURL(
        GURL("http://configservice.com"));

    // Set up the various test ClientConfigs.
    ClientConfig config = CreateConfig(
        kSuccessSessionKey, kConfigRefreshDurationSeconds, 0,
        ProxyServer_ProxyScheme_HTTPS, "origin.net", 443,
        ProxyServer_ProxyScheme_HTTP, "fallback.net", 80, 0.5f, false);
    config.SerializeToString(&config_);
    encoded_config_ = EncodeConfig(config);

    ClientConfig previous_config = CreateConfig(
        kOldSuccessSessionKey, kConfigRefreshDurationSeconds, 0,
        ProxyServer_ProxyScheme_HTTPS, "old.origin.net", 443,
        ProxyServer_ProxyScheme_HTTP, "old.fallback.net", 80, 0.0f, false);
    previous_config.SerializeToString(&previous_config_);

    ClientConfig persisted = CreateConfig(
        kPersistedSessionKey, kConfigRefreshDurationSeconds, 0,
        ProxyServer_ProxyScheme_HTTPS, "persisted.net", 443,
        ProxyServer_ProxyScheme_HTTP, "persisted.net", 80, 0.0f, false);
    loaded_config_ = EncodeConfig(persisted);

    ClientConfig ignore_black_list_config =
        CreateConfig(kSuccessSessionKey, kConfigRefreshDurationSeconds, 0,
                     ProxyServer_ProxyScheme_HTTPS, "origin.net", 443,
                     ProxyServer_ProxyScheme_HTTP, "origin.net", 0, 0.5f, true);
    ignore_black_list_encoded_config_ = EncodeConfig(ignore_black_list_config);

    ClientConfig no_proxies_config;
    no_proxies_config.set_session_key(kSuccessSessionKey);
    no_proxies_config.mutable_refresh_duration()->set_seconds(
        kConfigRefreshDurationSeconds);
    no_proxies_config.mutable_refresh_duration()->set_nanos(0);
    no_proxies_config_ = EncodeConfig(no_proxies_config);
  }

  void SetDataReductionProxyEnabled(bool enabled, bool secure_proxy_allowed) {
    test_context_->config()->UpdateConfigForTesting(enabled,
                                                    secure_proxy_allowed, true);
  }

  void ResetBackoffEntryReleaseTime() {
    config_client()->SetCustomReleaseTime(base::TimeTicks::UnixEpoch());
  }

  void VerifyRemoteSuccess(bool expect_secure_proxies) {
    std::vector<DataReductionProxyServer> expected_http_proxies;
    if (expect_secure_proxies) {
      expected_http_proxies.push_back(DataReductionProxyServer(net::ProxyServer(
          net::ProxyServer::SCHEME_HTTPS, net::HostPortPair("origin.net", 443),
          true /* is_trusted_proxy */)));
    }
    expected_http_proxies.push_back(
        DataReductionProxyServer(net::ProxyServer::FromURI(
            kSuccessFallback, net::ProxyServer::SCHEME_HTTP)));

    EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
              config_client()->GetDelay());
    EXPECT_EQ(DataReductionProxyServer::ConvertToNetProxyServers(
                  expected_http_proxies),
              GetConfiguredProxiesForHttp());
    EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
    // The config should be persisted on the pref.
    EXPECT_EQ(encoded_config(), persisted_config());

    // Verify that the data reduction proxy servers are correctly set.
    // The first proxy must have type CORE. The second proxy must have type
    // UNSPECIFIED_TYPE since these are the types specified in the encoded
    // configs.
    ASSERT_EQ(
        2U, test_context_->mutable_config_values()->proxies_for_http().size());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(0)
                    .IsCoreProxy());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(1)
                    .IsCoreProxy());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(0)
                    .IsSecureProxy());
    EXPECT_FALSE(test_context_->mutable_config_values()
                     ->proxies_for_http()
                     .at(1)
                     .IsSecureProxy());
  }

  void VerifyRemoteSuccessWithOldConfig() {
    std::vector<DataReductionProxyServer> expected_http_proxies;
    expected_http_proxies.push_back(DataReductionProxyServer(
        net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                         net::HostPortPair("old.origin.net", 443),
                         true /* is_trusted_proxy */)));
    expected_http_proxies.push_back(
        DataReductionProxyServer(net::ProxyServer::FromURI(
            kOldSuccessFallback, net::ProxyServer::SCHEME_HTTP)));

    EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
              config_client()->GetDelay());
    EXPECT_EQ(DataReductionProxyServer::ConvertToNetProxyServers(
                  expected_http_proxies),
              GetConfiguredProxiesForHttp());
    EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());

    // Verify that the data reduction proxy servers are correctly set.
    ASSERT_EQ(
        2U, test_context_->mutable_config_values()->proxies_for_http().size());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(0)
                    .IsCoreProxy());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(1)
                    .IsCoreProxy());
  }

  void VerifySuccessWithLoadedConfig(bool expect_secure_proxies) {
    std::vector<DataReductionProxyServer> expected_http_proxies;
    if (expect_secure_proxies) {
      expected_http_proxies.push_back(DataReductionProxyServer(
          net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                           net::HostPortPair("persisted.net", 443),
                           true /* is_trusted_proxy */)));
    }
    expected_http_proxies.push_back(
        DataReductionProxyServer(net::ProxyServer::FromURI(
            kPersistedFallback, net::ProxyServer::SCHEME_HTTP)));
    EXPECT_EQ(DataReductionProxyServer::ConvertToNetProxyServers(
                  expected_http_proxies),
              GetConfiguredProxiesForHttp());
    EXPECT_EQ(kPersistedSessionKey, request_options()->GetSecureSession());

    // Verify that the data reduction proxy servers are correctly set.
    ASSERT_EQ(
        2U, test_context_->mutable_config_values()->proxies_for_http().size());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(0)
                    .IsCoreProxy());
    EXPECT_TRUE(test_context_->mutable_config_values()
                    ->proxies_for_http()
                    .at(1)
                    .IsCoreProxy());
  }

  TestDataReductionProxyConfigServiceClient* config_client() {
    return test_context_->test_config_client();
  }

  DataReductionProxyConfigurator* configurator() {
    return test_context_->configurator();
  }

  TestDataReductionProxyConfig* config() { return test_context_->config(); }

  MockDataReductionProxyRequestOptions* request_options() {
    return test_context_->mock_request_options();
  }

  std::vector<net::ProxyServer> GetConfiguredProxiesForHttp() const {
    return test_context_->GetConfiguredProxiesForHttp();
  }

  bool ignore_blacklist() const {
    return test_context_->test_data_reduction_proxy_service()
        ->ignore_blacklist();
  }

  void RunUntilIdle() { test_context_->RunUntilIdle(); }

  void AddMockSuccess() {
    mock_responses_.push_back({success_response(), net::HTTP_OK});
  }

  void AddMockPreviousSuccess() {
    mock_responses_.push_back({previous_success_response(), net::HTTP_OK});
  }

  void AddMockFailure() {
    mock_responses_.push_back({std::string(), net::HTTP_NOT_FOUND});
  }

  std::string persisted_config() const {
    return test_context_->pref_service()->GetString(
        prefs::kDataReductionProxyConfig);
  }

  base::Time persisted_config_retrieval_time() const {
    return base::Time() +
           base::TimeDelta::FromMicroseconds(
               test_context_->pref_service()->GetInt64(
                   prefs::kDataReductionProxyLastConfigRetrievalTime));
  }

  const std::string& success_response() const { return config_; }

  const std::string& encoded_config() const { return encoded_config_; }

  const std::string& previous_success_response() const {
    return previous_config_;
  }
  const std::string& ignore_black_list_encoded_config() const {
    return ignore_black_list_encoded_config_;
  }
  const std::string& no_proxies_config() const { return no_proxies_config_; }

  const std::string& loaded_config() const { return loaded_config_; }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;

 protected:
  std::unique_ptr<DataReductionProxyTestContext> test_context_;

 private:
  std::unique_ptr<DataReductionProxyRequestOptions> request_options_;

  // A configuration from the current remote request. The encoded version is
  // also stored.
  std::string config_;
  std::string encoded_config_;

  // A configuration from a previous remote request.
  std::string previous_config_;

  // An encoded config that represents a previously saved configuration.
  std::string loaded_config_;

  // A configuration where the black list rules are ignored.
  std::string ignore_black_list_encoded_config_;

  // A configuration where no proxies are configured.
  std::string no_proxies_config_;

  struct MockResponse {
    std::string response;
    net::HttpStatusCode http_code;
  };
  std::vector<MockResponse> mock_responses_;
  size_t curr_mock_response_index_ = 0;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyConfigServiceClientTest);
};

// Tests that backoff values increases with every time config cannot be fetched.
TEST_F(DataReductionProxyConfigServiceClientTest, EnsureBackoff) {
  Init();
  // Use a local/static config.
  base::HistogramTester histogram_tester;
  AddMockFailure();
  AddMockFailure();
  AddMockSuccess();

  EXPECT_EQ(0, config_client()->failed_attempts_before_success());

  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());

  // First attempt should be unsuccessful.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), config_client()->GetDelay());

#if defined(OS_ANDROID)
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
#endif

  // Second attempt should be unsuccessful and backoff time should increase.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_EQ(base::TimeDelta::FromSeconds(90), config_client()->GetDelay());
  EXPECT_TRUE(persisted_config().empty());
  EXPECT_TRUE(persisted_config_retrieval_time().is_null());

#if defined(OS_ANDROID)
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
#endif

  EXPECT_EQ(2, config_client()->failed_attempts_before_success());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess", 0);
}

// Tests that the config is read successfully on the first attempt.
TEST_F(DataReductionProxyConfigServiceClientTest, RemoteConfigSuccess) {
  Init();
  base::HistogramTester histogram_tester;

  AddMockSuccess();
  SetDataReductionProxyEnabled(true, true);
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.HttpRequestRTT", 1);
  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
#if defined(OS_ANDROID)
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
#endif
}

// Tests that the config is read successfully on the first attempt, and secure
// proxies are not used if the secure check failed.
TEST_F(DataReductionProxyConfigServiceClientTest,
       RemoteConfigSuccessWithSecureCheckFail) {
  Init();
  AddMockSuccess();
  SetDataReductionProxyEnabled(true, false);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(false);
#if defined(OS_ANDROID)
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
#endif
}

// Tests that the config is read successfully on the first attempt, and secure
// proxies are not used if the secure proxy check fails later after some time.
TEST_F(DataReductionProxyConfigServiceClientTest,
       RemoteConfigSuccessWithDelayedSecureCheckFail) {
  Init();
  AddMockSuccess();
  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);
#if defined(OS_ANDROID)
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
#endif

  std::vector<DataReductionProxyServer> http_proxies;
  http_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      kSuccessOrigin, net::ProxyServer::SCHEME_HTTP)));
  http_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      kSuccessFallback, net::ProxyServer::SCHEME_HTTP)));

  // Secure check failed.
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  NetworkPropertiesManager manager(base::DefaultClock::GetInstance(),
                                   &test_prefs);
  manager.SetIsSecureProxyDisallowedByCarrier(true);
  configurator()->Enable(manager, http_proxies);
  VerifyRemoteSuccess(false);
}

// Tests that the config is read successfully on the second attempt.
TEST_F(DataReductionProxyConfigServiceClientTest,
       RemoteConfigSuccessAfterFailure) {
  Init();
  base::HistogramTester histogram_tester;

  AddMockFailure();
  AddMockSuccess();

  EXPECT_EQ(0, config_client()->failed_attempts_before_success());

  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());

  // First attempt should be unsuccessful.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  EXPECT_EQ(1, config_client()->failed_attempts_before_success());
  EXPECT_EQ(base::TimeDelta::FromSeconds(30), config_client()->GetDelay());
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_TRUE(request_options()->GetSecureSession().empty());

  // Second attempt should be successful.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);
  EXPECT_EQ(0, config_client()->failed_attempts_before_success());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess", 1,
      1);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.HttpRequestRTT", 1);
}

// Verifies that the config is fetched successfully after IP address changes.
TEST_F(DataReductionProxyConfigServiceClientTest, OnIPAddressChange) {
  Init();
  const struct {
    bool secure_proxies_allowed;
  } tests[] = {
      {
          true,
      },
      {
          false,
      },
  };

  for (size_t i = 0; i < base::size(tests); ++i) {
    SetDataReductionProxyEnabled(true, tests[i].secure_proxies_allowed);
    config_client()->RetrieveConfig();

    const int kFailureCount = 5;

    for (int i = 0; i < kFailureCount; ++i) {
      AddMockFailure();
      config_client()->RetrieveConfig();
      RunUntilIdle();
    }

    // Verify that the backoff increased exponentially.
    EXPECT_EQ(base::TimeDelta::FromSeconds(2430),
              config_client()->GetDelay());  // 2430 = 30 * 3^(5-1)
    EXPECT_EQ(kFailureCount, config_client()->GetBackoffErrorCount());

    // IP address change should reset.
    config_client()->OnConnectionChanged(
        network::mojom::ConnectionType::CONNECTION_WIFI);
    EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
    EXPECT_EQ(i == 0, persisted_config().empty());
    EXPECT_EQ(i == 0, persisted_config_retrieval_time().is_null());
    ResetBackoffEntryReleaseTime();

    // Fetching the config should be successful.
    AddMockSuccess();
    config_client()->RetrieveConfig();
    RunUntilIdle();
    VerifyRemoteSuccess(tests[i].secure_proxies_allowed);

    // Disable in preparation for the next test.
    configurator()->Disable();
  }
}

// Verifies that the config is fetched successfully after IP address changes,
// and secure proxies are not used if the secure proxy check fails later after
// some time.
TEST_F(DataReductionProxyConfigServiceClientTest,
       OnIPAddressChangeDelayedSecureProxyCheckFail) {
  Init();

  SetDataReductionProxyEnabled(true, true);
  config_client()->RetrieveConfig();

  const int kFailureCount = 5;

  for (int i = 0; i < kFailureCount; ++i) {
    AddMockFailure();
    config_client()->RetrieveConfig();
    RunUntilIdle();
  }

  // Verify that the backoff increased exponentially.
  EXPECT_EQ(base::TimeDelta::FromSeconds(2430),
            config_client()->GetDelay());  // 2430 = 30 * 3^(5-1)
  EXPECT_EQ(kFailureCount, config_client()->GetBackoffErrorCount());

  // IP address change should reset.
  config_client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  EXPECT_TRUE(persisted_config().empty());
  EXPECT_TRUE(persisted_config_retrieval_time().is_null());
  ResetBackoffEntryReleaseTime();

  // Fetching the config should be successful.
  AddMockSuccess();
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);

  std::vector<DataReductionProxyServer> http_proxies;
  http_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      kSuccessOrigin, net::ProxyServer::SCHEME_HTTP)));
  http_proxies.push_back(DataReductionProxyServer(net::ProxyServer::FromURI(
      kSuccessFallback, net::ProxyServer::SCHEME_HTTP)));

  // Secure check failed.
  TestingPrefServiceSimple test_prefs;
  test_prefs.registry()->RegisterDictionaryPref(prefs::kNetworkProperties);
  NetworkPropertiesManager manager(base::DefaultClock::GetInstance(),
                                   &test_prefs);
  manager.SetIsSecureProxyDisallowedByCarrier(true);
  configurator()->Enable(manager, http_proxies);
  VerifyRemoteSuccess(false);
}

// Verifies that fetching the remote config has no effect if the config client
// is disabled.
TEST_F(DataReductionProxyConfigServiceClientTest, OnIPAddressChangeDisabled) {
  Init();
  config_client()->SetEnabled(false);
  SetDataReductionProxyEnabled(true, true);
  config_client()->RetrieveConfig();
  EXPECT_TRUE(request_options()->GetSecureSession().empty());

  enum : int { kFailureCount = 5 };

  for (int i = 0; i < kFailureCount; ++i) {
    config_client()->RetrieveConfig();
    RunUntilIdle();
    EXPECT_TRUE(request_options()->GetSecureSession().empty());
  }

  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  config_client()->OnConnectionChanged(
      network::mojom::ConnectionType::CONNECTION_WIFI);
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());

  config_client()->RetrieveConfig();
  RunUntilIdle();

  EXPECT_TRUE(request_options()->GetSecureSession().empty());
}

// Verifies the correctness of AuthFailure when the session key in the request
// headers matches the currrent session key.
TEST_F(DataReductionProxyConfigServiceClientTest, AuthFailure) {
  Init();
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      "chrome-proxy", "something=something_else, s=" +
                          std::string(kOldSuccessSessionKey) + ", key=value");

  base::HistogramTester histogram_tester;
  AddMockPreviousSuccess();
  AddMockSuccess();
  AddMockPreviousSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthExpired", 0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  // First remote config should be fetched.
  VerifyRemoteSuccessWithOldConfig();
  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
  EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);

  // Trigger an auth failure.
  scoped_refptr<net::HttpResponseHeaders> parsed(new net::HttpResponseHeaders(
      "HTTP/1.1 407 Proxy Authentication Required\n"));
  net::ProxyServer origin = net::ProxyServer::FromURI(
      kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  // Calling ShouldRetryDueToAuthFailure should trigger fetching of remote
  // config.
  net::LoadTimingInfo load_timing_info;
  load_timing_info.request_start =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1);
  load_timing_info.send_start = load_timing_info.request_start;
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(1, config_client()->GetBackoffErrorCount());
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());

  // Persisted config on pref should be cleared.
  EXPECT_TRUE(persisted_config().empty());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);
  RunUntilIdle();
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty", 1);

  // Second remote config should be fetched.
  VerifyRemoteSuccess(true);

  // Trigger a second auth failure.
  origin =
      net::ProxyServer::FromURI(kSuccessOrigin, net::ProxyServer::SCHEME_HTTP);

  EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
  request_headers.SetHeader(
      "chrome-proxy", "something=something_else, s=" +
                          std::string(kSuccessSessionKey) + ", key=value");
  // Calling ShouldRetryDueToAuthFailure should trigger fetching of remote
  // config.
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(2, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 2);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 2);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty", 2);
  RunUntilIdle();
  // Third remote config should be fetched.
  VerifyRemoteSuccessWithOldConfig();

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey",
      1 /* AUTH_EXPIRED_SESSION_KEY_MATCH */, 2);
}

// Verifies that the persisted client config is fetched from the disk at
// startup, and that the persisted client config is used only if it is less
// than 24 hours old.
TEST_F(DataReductionProxyConfigServiceClientTest,
       ValidatePersistedClientConfig) {
  Init();
  SetDataReductionProxyEnabled(true, true);

  const struct {
    base::Optional<base::TimeDelta> staleness;
    bool expect_valid_config;
  } tests[] = {
      {
          base::nullopt,
          true,
      },
      {
          base::TimeDelta::FromHours(25),
          false,
      },
      {
          base::TimeDelta::FromHours(1),
          true,
      },
  };

  for (const auto& test : tests) {
    base::HistogramTester histogram_tester;
    // Reset the state.
    request_options()->Invalidate();
    test_context_->pref_service()->ClearPref(
        prefs::kDataReductionProxyLastConfigRetrievalTime);
    RunUntilIdle();
    EXPECT_TRUE(request_options()->GetSecureSession().empty());

    // Apply a valid config. This should be stored on the disk.
    AddMockSuccess();
    config_client()->RetrieveConfig();

    RunUntilIdle();
    EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());

    if (test.staleness) {
      base::TimeDelta last_config_retrieval_time =
          base::Time::Now() - base::Time() - test.staleness.value();

      test_context_->pref_service()->SetInt64(
          prefs::kDataReductionProxyLastConfigRetrievalTime,
          last_config_retrieval_time.InMicroseconds());
      test_context_->pref_service()->SchedulePendingLossyWrites();
      test_context_->pref_service()->CommitPendingWrite();
    }

    RunUntilIdle();
    EXPECT_FALSE(persisted_config().empty());
    EXPECT_FALSE(persisted_config_retrieval_time().is_null());

    // Simulate startup which should cause the empty persisted client config to
    // be read from the disk.
    config_client()->SetRemoteConfigApplied(false);
    request_options()->Invalidate();
    test_context_->data_reduction_proxy_service()->ReadPersistedClientConfig();
    RunUntilIdle();
    EXPECT_NE(test.expect_valid_config,
              request_options()->GetSecureSession().empty());
    if (test.expect_valid_config) {
      EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
    }
  }
}

// Verifies that a new config is not fetched due to auth failure while a
// previous client config fetch triggered due to auth failure is already in
// progress.
TEST_F(DataReductionProxyConfigServiceClientTest, MultipleAuthFailures) {
  Init();
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      "chrome-proxy", "something=something_else, s=" +
                          std::string(kOldSuccessSessionKey) + ", key=value");

  base::HistogramTester histogram_tester;
  AddMockPreviousSuccess();
  AddMockSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthExpired", 0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  // First remote config should be fetched.
  VerifyRemoteSuccessWithOldConfig();
  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
  EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);

  // Trigger an auth failure.
  scoped_refptr<net::HttpResponseHeaders> parsed(new net::HttpResponseHeaders(
      "HTTP/1.1 407 Proxy Authentication Required\n"));
  net::ProxyServer origin = net::ProxyServer::FromURI(
      kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  // Calling ShouldRetryDueToAuthFailure should trigger fetching of remote
  // config.
  net::LoadTimingInfo load_timing_info;
  load_timing_info.request_start =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1);
  load_timing_info.send_start = load_timing_info.request_start;
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(1, config_client()->GetBackoffErrorCount());
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());

  // Persisted config on pref should be cleared.
  EXPECT_TRUE(persisted_config().empty());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty", 1);

  // Trigger a second auth failure.
  EXPECT_EQ(std::string(), request_options()->GetSecureSession());
  request_headers.SetHeader(
      "chrome-proxy", "something=something_else, s=" +
                          std::string(kSuccessSessionKey) + ", key=value");
  // Calling ShouldRetryDueToAuthFailure should trigger fetching of remote
  // config.
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(1, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthFailure.LatencyPenalty", 1);

  RunUntilIdle();
  VerifyRemoteSuccess(true);

  // Config should be fetched successfully.
  EXPECT_FALSE(persisted_config().empty());
  EXPECT_FALSE(persisted_config_retrieval_time().is_null());
  // The persisted config retrieval time should be very recent (less than 2
  // minutes old). 2 minutes of buffer is allowed to account for delays due to
  // delays in running tests.
  EXPECT_LE(persisted_config_retrieval_time(), base::Time::Now());
  EXPECT_GE(persisted_config_retrieval_time() + base::TimeDelta::FromMinutes(2),
            base::Time::Now());

  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey",
      0 /* AUTH_EXPIRED_SESSION_KEY_MISMATCH */, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey",
      1 /* AUTH_EXPIRED_SESSION_KEY_MATCH */, 1);

  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 2);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);

  DCHECK(test_context_->data_reduction_proxy_service());
  request_options()->Invalidate();
  test_context_->data_reduction_proxy_service()->ReadPersistedClientConfig();
  test_context_->RunUntilIdle();
}

// Verifies that a new config is not fetched due to auth failure while a
// previous client config fetch triggered due to IP address changeis already
// in progress.
TEST_F(DataReductionProxyConfigServiceClientTest,
       IPAddressChangeWithAuthFailure) {
  Init();
  net::HttpRequestHeaders request_headers;
  request_headers.SetHeader(
      "chrome-proxy", "something=something_else, s=" +
                          std::string(kOldSuccessSessionKey) + ", key=value");

  base::HistogramTester histogram_tester;
  AddMockPreviousSuccess();
  AddMockSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthExpired", 0);
  config_client()->RetrieveConfig();

  RunUntilIdle();
  // First remote config should be fetched.
  VerifyRemoteSuccessWithOldConfig();
  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
  EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);

  // Trigger IP address change again.
  AddMockPreviousSuccess();
  AddMockPreviousSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
  config_client()->RetrieveConfig();

  // Trigger an auth failure.
  scoped_refptr<net::HttpResponseHeaders> parsed(new net::HttpResponseHeaders(
      "HTTP/1.1 407 Proxy Authentication Required\n"));
  net::ProxyServer origin = net::ProxyServer::FromURI(
      kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  // Calling ShouldRetryDueToAuthFailure should trigger fetching of remote
  // config.
  net::LoadTimingInfo load_timing_info;
  load_timing_info.request_start =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1);
  load_timing_info.send_start = load_timing_info.request_start;
  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(1, config_client()->GetBackoffErrorCount());
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());

  // Persisted config on pref should be cleared.
  EXPECT_TRUE(persisted_config().empty());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);

  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
  // Persisted config on pref should be cleared.
  EXPECT_TRUE(persisted_config().empty());

  // Config should be fetched now.
  RunUntilIdle();
  VerifyRemoteSuccess(true);

  EXPECT_FALSE(configurator()->GetProxyConfig().proxy_rules().empty());
  // Persisted config on pref should be cleared.
  EXPECT_FALSE(persisted_config().empty());
  EXPECT_FALSE(persisted_config_retrieval_time().is_null());
  EXPECT_LE(persisted_config_retrieval_time(), base::Time::Now());
  EXPECT_GE(persisted_config_retrieval_time() + base::TimeDelta::FromMinutes(2),
            base::Time::Now());
  EXPECT_EQ(kSuccessSessionKey, request_options()->GetSecureSession());
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 2);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 1);
}

// Verifies the correctness of AuthFailure when the session key in the request
// headers do not match the currrent session key.
TEST_F(DataReductionProxyConfigServiceClientTest,
       AuthFailureWithRequestHeaders) {
  Init();
  net::HttpRequestHeaders request_headers;
  const char kSessionKeyRequestHeaders[] = "123";
  ASSERT_NE(kOldSuccessSessionKey, kSessionKeyRequestHeaders);
  request_headers.SetHeader("chrome-proxy",
                            "s=" + std::string(kSessionKeyRequestHeaders));
  base::HistogramTester histogram_tester;
  AddMockPreviousSuccess();
  AddMockSuccess();
  AddMockPreviousSuccess();

  SetDataReductionProxyEnabled(true, true);
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.AuthExpired", 0);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  // First remote config should be fetched.
  VerifyRemoteSuccessWithOldConfig();
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);

  // Trigger an auth failure.
  scoped_refptr<net::HttpResponseHeaders> parsed(new net::HttpResponseHeaders(
      "HTTP/1.1 407 Proxy Authentication Required\n"));
  net::ProxyServer origin = net::ProxyServer::FromURI(
      kOldSuccessOrigin, net::ProxyServer::SCHEME_HTTP);
  // Calling ShouldRetryDueToAuthFailure should not trigger fetching of remote
  // config since the session key in the request headers do not match the
  // current session key, but the request should be retried.
  net::LoadTimingInfo load_timing_info;
  load_timing_info.request_start =
      base::TimeTicks::Now() - base::TimeDelta::FromSeconds(1);
  load_timing_info.send_start = load_timing_info.request_start;

  EXPECT_TRUE(config_client()->ShouldRetryDueToAuthFailure(
      request_headers, parsed.get(), origin, load_timing_info));
  EXPECT_EQ(0, config_client()->GetBackoffErrorCount());
  // Persisted config on pref should be cleared.
  EXPECT_FALSE(persisted_config().empty());
  EXPECT_FALSE(persisted_config_retrieval_time().is_null());
  EXPECT_LE(persisted_config_retrieval_time(), base::Time::Now());
  EXPECT_GE(persisted_config_retrieval_time() + base::TimeDelta::FromMinutes(2),
            base::Time::Now());
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", false, 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.AuthExpired", true, 0);
  RunUntilIdle();
  EXPECT_EQ(kOldSuccessSessionKey, request_options()->GetSecureSession());

  histogram_tester.ExpectUniqueSample(
      "DataReductionProxy.ConfigService.AuthExpiredSessionKey",
      0 /* AUTH_EXPIRED_SESSION_KEY_MISMATCH */, 1);
}

// Tests that the config is overriden by kDataReductionProxyServerClientConfig.
TEST_F(DataReductionProxyConfigServiceClientTest, ApplyClientConfigOverride) {
  const std::string override_key = "OverrideSecureSession";
  std::string encoded_config;
  ClientConfig config = CreateConfig(
      override_key, kConfigRefreshDurationSeconds, 0,
      ProxyServer_ProxyScheme_HTTPS, "origin.net", 443,
      ProxyServer_ProxyScheme_HTTP, "fallback.net", 80, 0.5f, false);
  config.SerializeToString(&encoded_config);
  base::Base64Encode(encoded_config, &encoded_config);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      data_reduction_proxy::switches::kDataReductionProxyServerClientConfig,
      encoded_config);
  Init();

  AddMockSuccess();
  SetDataReductionProxyEnabled(true, true);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  // Make sure repeated fetches won't change the overridden config.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  EXPECT_EQ(request_options()->GetSecureSession(), override_key);
}

// Tests that remote config can be applied after the serialized config has
// been applied.
TEST_F(DataReductionProxyConfigServiceClientTest, ApplySerializedConfig) {
  Init();
  AddMockSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  config_client()->ApplySerializedConfig(loaded_config());
  VerifySuccessWithLoadedConfig(true);
  EXPECT_TRUE(persisted_config().empty());

  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);
}

// Tests that remote config can be applied after the serialized config has
// been applied. Verifies that if the secure transport is restricted, then the
// secure proxies are not used.
TEST_F(DataReductionProxyConfigServiceClientTest,
       ApplySerializedConfigWithSecureTransportRestricted) {
  Init();

  AddMockSuccess();

  SetDataReductionProxyEnabled(true, false);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  config_client()->ApplySerializedConfig(loaded_config());
  VerifySuccessWithLoadedConfig(false);
  EXPECT_TRUE(persisted_config().empty());

  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(false);
}

// Tests that serialized config has no effect after the config has been
// retrieved successfully.
TEST_F(DataReductionProxyConfigServiceClientTest,
       ApplySerializedConfigAfterReceipt) {
  Init();
  AddMockSuccess();

  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_TRUE(request_options()->GetSecureSession().empty());

  // Retrieve the remote config.
  config_client()->RetrieveConfig();
  RunUntilIdle();
  VerifyRemoteSuccess(true);

  // ApplySerializedConfig should not have any effect since the remote config
  // is already applied.
  config_client()->ApplySerializedConfig(encoded_config());
  VerifyRemoteSuccess(true);
}

// Tests that a local serialized config can be applied successfully if remote
// config has not been fetched so far.
TEST_F(DataReductionProxyConfigServiceClientTest, ApplySerializedConfigLocal) {
  Init();
  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_TRUE(request_options()->GetSecureSession().empty());
  EXPECT_TRUE(persisted_config_retrieval_time().is_null());

  // ApplySerializedConfig should apply the encoded config.
  config_client()->ApplySerializedConfig(encoded_config());
  EXPECT_EQ(std::vector<net::ProxyServer>(
                {net::ProxyServer(net::ProxyServer::SCHEME_HTTPS,
                                  net::HostPortPair("origin.net", 443),
                                  true /* is_trusted_proxy */),
                 net::ProxyServer::FromURI(kSuccessFallback,
                                           net::ProxyServer::SCHEME_HTTP)}),
            GetConfiguredProxiesForHttp());
  EXPECT_TRUE(persisted_config().empty());
  EXPECT_TRUE(persisted_config_retrieval_time().is_null());
  EXPECT_FALSE(request_options()->GetSecureSession().empty());
}

TEST_F(DataReductionProxyConfigServiceClientTest,
       ApplySerializedConfigIgnoreBlackList) {
  Init();

  config_client()->ApplySerializedConfig(ignore_black_list_encoded_config());
  EXPECT_TRUE(ignore_blacklist());
}

TEST_F(DataReductionProxyConfigServiceClientTest, EmptyConfigDisablesDRP) {
  Init();
  SetDataReductionProxyEnabled(true, true);
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());

  config_client()->ApplySerializedConfig(no_proxies_config());
  EXPECT_EQ(std::vector<net::ProxyServer>(), GetConfiguredProxiesForHttp());
  EXPECT_TRUE(configurator()->GetProxyConfig().proxy_rules().empty());
}

#if defined(OS_ANDROID)
// Verifies the correctness of fetching config when Chromium is in background
// and foreground.
TEST_F(DataReductionProxyConfigServiceClientTest, FetchConfigOnForeground) {
  Init();
  SetDataReductionProxyEnabled(true, true);

  {
    // Tests that successful config fetches while Chromium is in background,
    // does not trigger refetches when Chromium comes to foreground.
    base::HistogramTester histogram_tester;
    AddMockSuccess();
    config_client()->set_application_state_background(true);
    config_client()->RetrieveConfig();
    RunUntilIdle();
    VerifyRemoteSuccess(true);
    EXPECT_FALSE(config_client()->foreground_fetch_pending());
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 1);
    EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
              config_client()->GetDelay());
    config_client()->set_application_state_background(false);
    config_client()->TriggerApplicationStatusToForeground();
    RunUntilIdle();
    EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
              config_client()->GetDelay());
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 1);
  }

  {
    // Tests that config fetch failures while Chromium is in foreground does not
    // trigger refetches when Chromium comes to foreground again.
    base::HistogramTester histogram_tester;
    AddMockFailure();
    config_client()->set_application_state_background(false);
    config_client()->RetrieveConfig();
    RunUntilIdle();
    EXPECT_FALSE(config_client()->foreground_fetch_pending());
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 0);
    EXPECT_EQ(base::TimeDelta::FromSeconds(30), config_client()->GetDelay());
    config_client()->TriggerApplicationStatusToForeground();
    RunUntilIdle();
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 0);
    EXPECT_EQ(base::TimeDelta::FromSeconds(30), config_client()->GetDelay());
  }

  {
    // Tests that config fetch failures while Chromium is in background, trigger
    // a refetch when Chromium comes to foreground.
    base::HistogramTester histogram_tester;
    AddMockFailure();
    AddMockSuccess();
    config_client()->set_application_state_background(true);
    config_client()->RetrieveConfig();
    RunUntilIdle();
    EXPECT_TRUE(config_client()->foreground_fetch_pending());
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 0);
    EXPECT_EQ(base::TimeDelta::FromSeconds(kMaxBackgroundFetchIntervalSeconds),
              config_client()->GetDelay());
    config_client()->set_application_state_background(false);
    config_client()->TriggerApplicationStatusToForeground();
    RunUntilIdle();
    EXPECT_FALSE(config_client()->foreground_fetch_pending());
    histogram_tester.ExpectTotalCount(
        "DataReductionProxy.ConfigService.FetchLatency", 1);
    EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
              config_client()->GetDelay());
    VerifyRemoteSuccess(true);
  }
}

class DataReductionProxyAggressiveConfigServiceClientTest
    : public DataReductionProxyConfigServiceClientTest {
 public:
  DataReductionProxyAggressiveConfigServiceClientTest()
      : DataReductionProxyConfigServiceClientTest(true) {}
};

TEST_F(DataReductionProxyAggressiveConfigServiceClientTest,
       AggressiveFetchConfigOnBackground) {
  Init();
  SetDataReductionProxyEnabled(true, true);

  // Tests that config fetch failures while Chromium is in background, trigger
  // refetches while still in background, and no refetch happens Chromium
  // comes to foreground, when the aggressive client config fetch feature is
  // enabled.
  base::HistogramTester histogram_tester;
  AddMockFailure();
  AddMockFailure();
  AddMockSuccess();
  config_client()->set_application_state_background(true);
  config_client()->RetrieveConfig();
  RunUntilIdle();
  // Three fetches are triggered in background without any backoff. First two
  // fail, while the third succeeds.
  EXPECT_EQ(base::TimeDelta::FromSeconds(0),
            config_client()->GetBackoffTimeUntilRelease());
  EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
            config_client()->GetDelay());
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.FetchLatency", 1);
  histogram_tester.ExpectBucketCount(
      "DataReductionProxy.ConfigService.FetchFailedAttemptsBeforeSuccess", 2,
      1);

  // No new fetch should happen when Chromium comes to foreground.
  config_client()->set_application_state_background(false);
  config_client()->TriggerApplicationStatusToForeground();
  RunUntilIdle();
  EXPECT_FALSE(config_client()->foreground_fetch_pending());
  histogram_tester.ExpectTotalCount(
      "DataReductionProxy.ConfigService.FetchLatency", 1);
  EXPECT_EQ(base::TimeDelta::FromSeconds(kConfigRefreshDurationSeconds),
            config_client()->GetDelay());
  VerifyRemoteSuccess(true);
}

#endif

}  // namespace data_reduction_proxy
