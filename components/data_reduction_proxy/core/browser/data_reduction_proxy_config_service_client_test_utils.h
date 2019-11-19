// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_

#include <stdint.h>
#include <string>

#include "components/data_reduction_proxy/proto/client_config.pb.h"

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
                          bool ignore_long_term_black_list_rules);

// Creates a new ClientConfig with no proxies from the given parameters.
ClientConfig CreateEmptyProxyConfig(const std::string& session_key,
                                    int64_t expire_duration_seconds,
                                    int64_t expire_duration_nanoseconds,
                                    float reporting_fraction,
                                    bool ignore_long_term_black_list_rules);

// Takes |config| and returns the base64 encoding of its serialized byte stream.
std::string EncodeConfig(const ClientConfig& config);

// Returns a valid ClientConfig in base64 full of dummy values.
std::string DummyBase64Config();

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_CONFIG_SERVICE_CLIENT_TEST_UTILS_H_
