// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_mutable_config_values.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/optional.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_params.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_server.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_switches.h"
#include "components/data_reduction_proxy/core/common/data_reduction_proxy_type_info.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

namespace {

void ExpectTypeInfo(
    base::Optional<DataReductionProxyTypeInfo> type_info,
    const std::vector<DataReductionProxyServer>& expected_proxy_servers,
    size_t expected_proxy_index) {
  ASSERT_TRUE(type_info);
  EXPECT_EQ(expected_proxy_servers, type_info->proxy_servers);
  EXPECT_EQ(expected_proxy_index, type_info->proxy_index);
}

class DataReductionProxyMutableConfigValuesTest : public testing::Test {
 public:
  DataReductionProxyMutableConfigValuesTest() {}
  ~DataReductionProxyMutableConfigValuesTest() override {}

  void Init() {
    mutable_config_values_ =
        std::make_unique<DataReductionProxyMutableConfigValues>();
  }

  DataReductionProxyMutableConfigValues* mutable_config_values() const {
    return mutable_config_values_.get();
  }

 private:
  std::unique_ptr<DataReductionProxyMutableConfigValues> mutable_config_values_;
};

TEST_F(DataReductionProxyMutableConfigValuesTest, UpdateValuesAndInvalidate) {
  Init();
  EXPECT_EQ(std::vector<DataReductionProxyServer>(),
            mutable_config_values()->proxies_for_http());

  std::vector<DataReductionProxyServer> proxies_for_http;

  net::ProxyServer first_proxy_server(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  proxies_for_http.push_back(DataReductionProxyServer(first_proxy_server));

  net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP);
  proxies_for_http.push_back(DataReductionProxyServer(second_proxy_server));

  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      first_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      second_proxy_server));

  mutable_config_values()->UpdateValues(proxies_for_http);
  EXPECT_EQ(proxies_for_http, mutable_config_values()->proxies_for_http());

  // The configured proxies should be recognized as Data Reduction Proxies.
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     first_proxy_server),
                 proxies_for_http, 0U);
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     second_proxy_server),
                 proxies_for_http, 1U);

  // Invalidation must clear out the list of proxies and their properties.
  mutable_config_values()->Invalidate();
  EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());

  // The previously configured proxies should still be recognized as Data
  // Reduction Proxies, even though the config was invalidated.
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     first_proxy_server),
                 proxies_for_http, 0U);
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     second_proxy_server),
                 proxies_for_http, 1U);
}

// Tests if HTTP proxies are overridden when |kDataReductionProxyHttpProxies|
// switch is specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideProxiesForHttp) {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kDataReductionProxyHttpProxies,
      "http://override-first.net;http://override-second.net");
  Init();

  net::ProxyServer first_override_proxy_server = net::ProxyServer::FromURI(
      "http://override-first.net", net::ProxyServer::SCHEME_HTTP);
  net::ProxyServer second_override_proxy_server = net::ProxyServer::FromURI(
      "http://override-second.net", net::ProxyServer::SCHEME_HTTP);

  net::ProxyServer first_proxy_server(net::ProxyServer::FromURI(
      "http://first.net", net::ProxyServer::SCHEME_HTTP));
  net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
      "http://second.net", net::ProxyServer::SCHEME_HTTP);

  EXPECT_EQ(std::vector<DataReductionProxyServer>(),
            mutable_config_values()->proxies_for_http());

  // No proxy servers should be recognized as Data Reduction Proxies.
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      first_override_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      second_override_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      first_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      second_proxy_server));

  std::vector<DataReductionProxyServer> proxies_for_http;
  proxies_for_http.push_back(DataReductionProxyServer(first_proxy_server));
  proxies_for_http.push_back(DataReductionProxyServer(second_proxy_server));

  mutable_config_values()->UpdateValues(proxies_for_http);

  std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
  expected_override_proxies_for_http.push_back(
      DataReductionProxyServer(first_override_proxy_server));
  expected_override_proxies_for_http.push_back(
      DataReductionProxyServer(second_override_proxy_server));

  EXPECT_EQ(expected_override_proxies_for_http,
            mutable_config_values()->proxies_for_http());

  // The overriding proxy servers should be recognized as Data Reduction
  // Proxies.
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     first_override_proxy_server),
                 expected_override_proxies_for_http, 0U);
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     second_override_proxy_server),
                 expected_override_proxies_for_http, 1U);

  // The proxy servers that were overriden should not be recognized as Data
  // Reduction Proxies.
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      first_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      second_proxy_server));

  // Invalidation must clear out the list of proxies and their properties.
  mutable_config_values()->Invalidate();
  EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());

  // The overriding proxy servers should be recognized as Data Reduction
  // Proxies.
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     first_override_proxy_server),
                 expected_override_proxies_for_http, 0U);
  ExpectTypeInfo(mutable_config_values()->FindConfiguredDataReductionProxy(
                     second_override_proxy_server),
                 expected_override_proxies_for_http, 1U);

  // The proxy servers that were overriden should not be recognized as Data
  // Reduction Proxies.
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      first_proxy_server));
  EXPECT_FALSE(mutable_config_values()->FindConfiguredDataReductionProxy(
      second_proxy_server));
}

// Tests if HTTP proxies are overridden when |kDataReductionProxy| or
// |kDataReductionProxyFallback| switches are specified.
TEST_F(DataReductionProxyMutableConfigValuesTest, OverrideDataReductionProxy) {
  const struct {
    bool set_primary;
    bool set_fallback;
  } tests[] = {
      {false, false}, {true, false}, {false, true}, {true, true},
  };

  for (const auto& test : tests) {
    // Reset all flags.
    base::CommandLine::ForCurrentProcess()->InitFromArgv(0, nullptr);
    if (test.set_primary) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxy, "http://override-first.net");
    }
    if (test.set_fallback) {
      base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
          switches::kDataReductionProxyFallback, "http://override-second.net");
    }
    Init();

    EXPECT_EQ(std::vector<DataReductionProxyServer>(),
              mutable_config_values()->proxies_for_http());

    std::vector<DataReductionProxyServer> proxies_for_http;

    if (test.set_primary) {
      net::ProxyServer first_proxy_server = (net::ProxyServer::FromURI(
          "http://first.net", net::ProxyServer::SCHEME_HTTP));
      proxies_for_http.push_back(DataReductionProxyServer(first_proxy_server));
    }
    if (test.set_fallback) {
      net::ProxyServer second_proxy_server = net::ProxyServer::FromURI(
          "http://second.net", net::ProxyServer::SCHEME_HTTP);

      proxies_for_http.push_back(DataReductionProxyServer(second_proxy_server));
    }

    mutable_config_values()->UpdateValues(proxies_for_http);

    // Overriding proxies must have type UNSPECIFIED_TYPE.
    std::vector<DataReductionProxyServer> expected_override_proxies_for_http;
    if (test.set_primary) {
      expected_override_proxies_for_http.push_back(
          DataReductionProxyServer(net::ProxyServer::FromURI(
              "http://override-first.net", net::ProxyServer::SCHEME_HTTP)));
    }
    if (test.set_fallback) {
      expected_override_proxies_for_http.push_back(
          DataReductionProxyServer(net::ProxyServer::FromURI(
              "http://override-second.net", net::ProxyServer::SCHEME_HTTP)));
    }

    EXPECT_EQ(expected_override_proxies_for_http,
              mutable_config_values()->proxies_for_http());

    // Invalidation must clear out the list of proxies and their properties.
    mutable_config_values()->Invalidate();
    EXPECT_TRUE(mutable_config_values()->proxies_for_http().empty());
  }
}

TEST_F(DataReductionProxyMutableConfigValuesTest, GetAllConfiguredProxies) {
  Init();
  net::ProxyList expected_proxies;
  EXPECT_TRUE(mutable_config_values()->GetAllConfiguredProxies().Equals(
      expected_proxies));

  net::ProxyServer proxy_server1 =
      net::ProxyServer::FromPacString("PROXY proxy1.net");
  mutable_config_values()->UpdateValues(
      {DataReductionProxyServer(proxy_server1)});
  expected_proxies.SetSingleProxyServer(proxy_server1);

  EXPECT_TRUE(mutable_config_values()->GetAllConfiguredProxies().Equals(
      expected_proxies));

  net::ProxyServer proxy_server2 =
      net::ProxyServer::FromPacString("PROXY proxy2.net");
  mutable_config_values()->UpdateValues(
      {DataReductionProxyServer(proxy_server2)});

  // First proxy server should also still be in proxy list.
  expected_proxies.Clear();
  expected_proxies.AddProxyServer(proxy_server2);
  expected_proxies.AddProxyServer(proxy_server1);
  EXPECT_TRUE(mutable_config_values()->GetAllConfiguredProxies().Equals(
      expected_proxies));
}

}  // namespace

}  // namespace data_reduction_proxy
