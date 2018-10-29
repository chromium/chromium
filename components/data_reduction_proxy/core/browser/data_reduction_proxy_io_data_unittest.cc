// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_io_data.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "base/time/time.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_configurator.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_prefs.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_request_options.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_test_utils.h"
#include "components/data_reduction_proxy/core/browser/network_properties_manager.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params_test_utils.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_pref_names.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/testing_pref_service.h"
#include "net/http/http_network_session.h"
#include "net/proxy_resolution/proxy_info.h"
#include "net/proxy_resolution/proxy_resolution_service.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_network_quality_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {
namespace {
// Used only to verify that a wrapped network delegate gets called.
class CountingNetworkDelegate : public net::NetworkDelegateImpl {
 public:
  CountingNetworkDelegate() : created_requests_(0) {
  }

  ~CountingNetworkDelegate() final {
  }

  int OnBeforeURLRequest(net::URLRequest* request,
                         net::CompletionOnceCallback callback,
                         GURL* new_url) final {
    created_requests_++;
    return net::OK;
  }

  int created_requests() const {
    return created_requests_;
  }

 private:
  int created_requests_;
};

std::string CreateEncodedConfig(
    const std::vector<DataReductionProxyServer> proxy_servers) {
  ClientConfig config;
  config.set_session_key("session");
  for (const auto& proxy_server : proxy_servers) {
    ProxyServer* config_proxy =
        config.mutable_proxy_config()->add_http_proxy_servers();
    net::HostPortPair host_port_pair =
        proxy_server.proxy_server().host_port_pair();
    config_proxy->set_scheme(ProxyServer_ProxyScheme_HTTP);
    config_proxy->set_host(host_port_pair.host());
    config_proxy->set_port(host_port_pair.port());
    config_proxy->set_type(proxy_server.IsCoreProxy()
                               ? ProxyServer_ProxyType_CORE
                               : ProxyServer_ProxyType_UNSPECIFIED_TYPE);
  }
  return EncodeConfig(config);
}
}  // namespace

class DataReductionProxyIODataTest : public testing::Test {
 public:
  DataReductionProxyIODataTest()
      : scoped_task_environment_(
            base::test::ScopedTaskEnvironment::MainThreadType::IO) {}

  void SetUp() override {
    RegisterSimpleProfilePrefs(prefs_.registry());
  }

  void RequestCallback(int err) {
  }

  net::TestDelegate* delegate() {
    return &delegate_;
  }

  const net::TestURLRequestContext& context() const {
    return context_;
  }

  PrefService* prefs() {
    return &prefs_;
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;

 private:
  net::TestDelegate delegate_;
  net::TestURLRequestContext context_;
  TestingPrefServiceSimple prefs_;
};

TEST_F(DataReductionProxyIODataTest, TestConstruction) {
  std::unique_ptr<DataReductionProxyIOData> io_data(
      new DataReductionProxyIOData(
          Client::UNKNOWN, prefs(),
          network::TestNetworkConnectionTracker::GetInstance(),
          scoped_task_environment_.GetMainThreadTaskRunner(),
          scoped_task_environment_.GetMainThreadTaskRunner(),
          false /* enabled */, std::string() /* user_agent */,
          std::string() /* channel */));

  // Check that io_data creates an interceptor. Such an interceptor is
  // thoroughly tested by DataReductionProxyInterceptoTest.
  std::unique_ptr<net::URLRequestInterceptor> interceptor =
      io_data->CreateInterceptor();
  EXPECT_NE(nullptr, interceptor.get());

  // When creating a network delegate, expect that it properly wraps a
  // network delegate. Such a network delegate is thoroughly tested by
  // DataReductionProxyNetworkDelegateTest.
  std::unique_ptr<net::URLRequest> fake_request =
      context().CreateRequest(GURL("http://www.foo.com/"), net::IDLE,
                              delegate(), TRAFFIC_ANNOTATION_FOR_TESTS);
  CountingNetworkDelegate* wrapped_network_delegate =
      new CountingNetworkDelegate();
  std::unique_ptr<DataReductionProxyNetworkDelegate> network_delegate =
      io_data->CreateNetworkDelegate(base::WrapUnique(wrapped_network_delegate),
                                     false);
  network_delegate->NotifyBeforeURLRequest(
      fake_request.get(),
      base::BindOnce(&DataReductionProxyIODataTest::RequestCallback,
                     base::Unretained(this)),
      nullptr);
  EXPECT_EQ(1, wrapped_network_delegate->created_requests());
  EXPECT_NE(nullptr, io_data->bypass_stats());

  // Creating a second delegate with bypass statistics tracking should result
  // in usage stats being created.
  io_data->CreateNetworkDelegate(std::make_unique<CountingNetworkDelegate>(),
                                 true);
  EXPECT_NE(nullptr, io_data->bypass_stats());

  io_data->ShutdownOnUIThread();
}

TEST_F(DataReductionProxyIODataTest, TestResetBadProxyListOnDisableDataSaver) {
  net::TestURLRequestContext context(false);
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithURLRequestContext(&context)
          .SkipSettingsInitialization()
          .Build();

  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->InitSettings();
  DataReductionProxyIOData* io_data = drp_test_context->io_data();
  std::vector<net::ProxyServer> proxies;
  proxies.push_back(net::ProxyServer::FromURI("http://foo1.com",
                                              net::ProxyServer::SCHEME_HTTP));
  net::ProxyResolutionService* proxy_resolution_service =
      io_data->url_request_context_getter_->GetURLRequestContext()
          ->proxy_resolution_service();
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("http://foo2.com");
  const net::ProxyRetryInfoMap& bad_proxy_list =
      proxy_resolution_service->proxy_retry_info();

  // Simulate network error to add proxies to the bad proxy list.
  proxy_resolution_service->MarkProxiesAsBadUntil(
      proxy_info, base::TimeDelta::FromDays(1), proxies,
      net::NetLogWithSource());
  base::RunLoop().RunUntilIdle();

  // Verify that there are 2 proxies in the bad proxies list.
  EXPECT_EQ(2UL, bad_proxy_list.size());

  // Turn Data Saver off.
  drp_test_context->settings()->SetDataReductionProxyEnabled(false);
  base::RunLoop().RunUntilIdle();

  // Verify that bad proxy list is empty.
  EXPECT_EQ(0UL, bad_proxy_list.size());
}

TEST_F(DataReductionProxyIODataTest, HoldbackConfiguresProxies) {
  net::TestURLRequestContext context(false);
  base::FieldTrialList field_trial_list(nullptr);
  ASSERT_TRUE(base::FieldTrialList::CreateFieldTrial(
      "DataCompressionProxyHoldback", "Enabled"));
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithURLRequestContext(&context)
          .SkipSettingsInitialization()
          .Build();

  EXPECT_TRUE(drp_test_context->test_params()->proxies_for_http().size() > 0);
  EXPECT_FALSE(drp_test_context->test_params()
                   ->proxies_for_http()
                   .front()
                   .proxy_server()
                   .is_direct());
}

class TestCustomProxyConfigClient
    : public network::mojom::CustomProxyConfigClient {
 public:
  TestCustomProxyConfigClient(
      network::mojom::CustomProxyConfigClientRequest request)
      : binding_(this, std::move(request)) {}

  // network::mojom::CustomProxyConfigClient implementation:
  void OnCustomProxyConfigUpdated(
      network::mojom::CustomProxyConfigPtr proxy_config) override {
    config = std::move(proxy_config);
  }

  network::mojom::CustomProxyConfigPtr config;

 private:
  mojo::Binding<network::mojom::CustomProxyConfigClient> binding_;
};

TEST_F(DataReductionProxyIODataTest, TestCustomProxyConfigClient) {
  auto proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies, proxy_server.ToURI());

  net::TestURLRequestContext context(false);
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithURLRequestContext(&context)
          .Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  DataReductionProxyIOData* io_data = drp_test_context->io_data();

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data->SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server);
  EXPECT_TRUE(
      client.config->post_cache_headers.HasHeader(chrome_proxy_header()));
  EXPECT_TRUE(
      client.config->pre_cache_headers.HasHeader(chrome_proxy_ect_header()));
  // Alternate proxy list should be empty because there are no core proxies.
  EXPECT_TRUE(client.config->alternate_proxy_list.IsEmpty());
}

TEST_F(DataReductionProxyIODataTest, TestCustomProxyConfigUpdatedOnECTChange) {
  net::TestURLRequestContext context(false);
  std::unique_ptr<DataReductionProxyTestContext> drp_test_context =
      DataReductionProxyTestContext::Builder()
          .WithURLRequestContext(&context)
          .Build();
  drp_test_context->SetDataReductionProxyEnabled(true);
  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_4G);
  DataReductionProxyIOData* io_data = drp_test_context->io_data();

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data->SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  std::string value;
  EXPECT_TRUE(client.config->pre_cache_headers.GetHeader(
      chrome_proxy_ect_header(), &value));
  EXPECT_EQ(value, "4G");

  drp_test_context->test_network_quality_tracker()
      ->ReportEffectiveConnectionTypeForTesting(
          net::EFFECTIVE_CONNECTION_TYPE_2G);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(client.config->pre_cache_headers.GetHeader(
      chrome_proxy_ect_header(), &value));
  EXPECT_EQ(value, "2G");
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigUpdatedOnHeaderChange) {
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  std::string value;
  EXPECT_TRUE(client.config->post_cache_headers.GetHeader(chrome_proxy_header(),
                                                          &value));

  io_data.request_options()->SetSecureSession("session_value");
  base::RunLoop().RunUntilIdle();
  std::string changed_value;
  EXPECT_TRUE(client.config->post_cache_headers.GetHeader(chrome_proxy_header(),
                                                          &changed_value));
  EXPECT_NE(value, changed_value);
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigUpdatedOnProxyChange) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), prefs(),
      scoped_task_environment_.GetMainThreadTaskRunner());
  io_data.config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);
  io_data.config()->UpdateConfigForTesting(true, true, true);

  auto proxy_server1 = net::ProxyServer::FromPacString("PROXY foo");
  io_data.config_client()->ApplySerializedConfig(CreateEncodedConfig(
      {DataReductionProxyServer(proxy_server1, ProxyServer_ProxyType_CORE)}));

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server1);

  auto proxy_server2 = net::ProxyServer::FromPacString("PROXY bar");
  io_data.config_client()->SetRemoteConfigAppliedForTesting(false);
  io_data.config_client()->ApplySerializedConfig(CreateEncodedConfig(
      {DataReductionProxyServer(proxy_server2, ProxyServer_ProxyType_CORE)}));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(client.config->rules.proxies_for_http.Get(), proxy_server2);
}

TEST_F(DataReductionProxyIODataTest,
       TestCustomProxyConfigHasAlternateProxyListOfCoreProxies) {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableDataReductionProxyWarmupURLFetch);
  DataReductionProxyIOData io_data(
      Client::UNKNOWN, prefs(),
      network::TestNetworkConnectionTracker::GetInstance(),
      scoped_task_environment_.GetMainThreadTaskRunner(),
      scoped_task_environment_.GetMainThreadTaskRunner(), false /* enabled */,
      std::string() /* user_agent */, std::string() /* channel */);
  NetworkPropertiesManager network_properties_manager(
      base::DefaultClock::GetInstance(), prefs(),
      scoped_task_environment_.GetMainThreadTaskRunner());
  io_data.config()->SetNetworkPropertiesManagerForTesting(
      &network_properties_manager);
  io_data.config()->UpdateConfigForTesting(true, true, true);

  auto core_proxy_server = net::ProxyServer::FromPacString("PROXY foo");
  io_data.config_client()->ApplySerializedConfig(CreateEncodedConfig(
      {DataReductionProxyServer(core_proxy_server, ProxyServer_ProxyType_CORE),
       DataReductionProxyServer(net::ProxyServer::FromPacString("PROXY bar"),
                                ProxyServer_ProxyType_UNSPECIFIED_TYPE)}));

  network::mojom::CustomProxyConfigClientPtrInfo client_ptr_info;
  TestCustomProxyConfigClient client(mojo::MakeRequest(&client_ptr_info));
  io_data.SetCustomProxyConfigClient(std::move(client_ptr_info));
  base::RunLoop().RunUntilIdle();

  net::ProxyList expected_proxy_list;
  expected_proxy_list.SetSingleProxyServer(core_proxy_server);
  EXPECT_TRUE(client.config->alternate_proxy_list.Equals(expected_proxy_list));
}

}  // namespace data_reduction_proxy
