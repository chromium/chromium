// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_config_service_client_test_utils.h"

#include <string>

#include "base/base64.h"
#include "components/data_reduction_proxy/proto/client_config.pb.h"
#include "net/base/proxy_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace data_reduction_proxy {

// Creates a new ClientConfig from the given parameters.
ClientConfig CreateConfig(const std::string& session_key,
                          int64_t expire_duration_seconds,
                          int64_t expire_duration_nanoseconds,
                          ProxyServer_ProxyScheme primary_scheme,
                          const std::string& primary_host,
                          int primary_port,
                          ProxyServer_ProxyScheme secondary_scheme,
                          const std::string& secondary_host,
                          int secondary_port,
                          float reporting_fraction,
                          bool ignore_long_term_black_list_rules) {
  ClientConfig config;

  config.set_session_key(session_key);
  config.mutable_refresh_duration()->set_seconds(expire_duration_seconds);
  config.mutable_refresh_duration()->set_nanos(expire_duration_nanoseconds);

  // Leave the pageload_metrics_config empty when |reporting_fraction| is not
  // inclusively between zero and one.
  if (reporting_fraction >= 0.0f && reporting_fraction <= 1.0f) {
    config.mutable_pageload_metrics_config()->set_reporting_fraction(
        reporting_fraction);
  }
  config.set_ignore_long_term_black_list_rules(
      ignore_long_term_black_list_rules);
  ProxyServer* primary_proxy =
      config.mutable_proxy_config()->add_http_proxy_servers();
  primary_proxy->set_scheme(primary_scheme);
  primary_proxy->set_host(primary_host);
  primary_proxy->set_port(primary_port);

  ProxyServer* secondary_proxy =
      config.mutable_proxy_config()->add_http_proxy_servers();
  secondary_proxy->set_scheme(secondary_scheme);
  secondary_proxy->set_host(secondary_host);
  secondary_proxy->set_port(secondary_port);

  return config;
}

// Creates a new ClientConfig with no proxies from the given parameters.
ClientConfig CreateEmptyProxyConfig(const std::string& session_key,
                                    int64_t expire_duration_seconds,
                                    int64_t expire_duration_nanoseconds,
                                    float reporting_fraction,
                                    bool ignore_long_term_black_list_rules) {
  ClientConfig config;

  config.set_session_key(session_key);
  config.mutable_refresh_duration()->set_seconds(expire_duration_seconds);
  config.mutable_refresh_duration()->set_nanos(expire_duration_nanoseconds);

  // Leave the pageload_metrics_config empty when |reporting_fraction| is not
  // inclusively between zero and one.
  if (reporting_fraction >= 0.0f && reporting_fraction <= 1.0f) {
    config.mutable_pageload_metrics_config()->set_reporting_fraction(
        reporting_fraction);
  }
  config.set_ignore_long_term_black_list_rules(
      ignore_long_term_black_list_rules);
  config.mutable_proxy_config()->clear_http_proxy_servers();
  return config;
}

// Takes |config| and returns the base64 encoding of its serialized byte stream.
std::string EncodeConfig(const ClientConfig& config) {
  std::string config_data;
  std::string encoded_data;
  EXPECT_TRUE(config.SerializeToString(&config_data));
  base::Base64Encode(config_data, &encoded_data);
  return encoded_data;
}

std::string DummyBase64Config() {
  ClientConfig config = CreateConfig(
      "secureSessionKey", 600, 0, ProxyServer_ProxyScheme_HTTPS, "origin.net",
      443, ProxyServer_ProxyScheme_HTTP, "fallback.net", 80, 0.5f, false);
  return EncodeConfig(config);
}

}  // namespace data_reduction_proxy
