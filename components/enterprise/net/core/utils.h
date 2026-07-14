// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/net/core/types.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"

namespace enterprise_net {

// Parses a Provisioning Domain proxy protocol string (e.g., "socks5",
// "http-connect", "https-connect") to the corresponding
// net::ProxyServer::Scheme. Returns SCHEME_INVALID for unsupported or invalid
// protocols.
net::ProxyServer::Scheme ParseProvisioningDomainProxyProtocol(
    std::string_view protocol_str);

// Resolves and expands policy extra headers (including placeholders) into a
// net::HttpRequestHeaders object.
net::HttpRequestHeaders ResolveExtraHeaders(
    const ProvisioningDomainConfig& policy,
    const std::string& profile_id,
    const std::string& accept_languages);

// Parses a single "ProxyProvisioningDomains" policy entry dictionary into
// a ProvisioningDomainConfig struct.
// Returns std::nullopt if the entry is invalid or missing a valid pvd_id.
std::optional<ProvisioningDomainConfig> ParseProxyProvisioningDomainPolicy(
    const base::DictValue& domain_dict);

// Parses a raw PvD JSON configuration string into a
// ProvisioningDomainProxyConfig struct. Returns std::nullopt if the JSON is
// invalid or missing required fields.
std::optional<ProvisioningDomainProxyConfig> ParseProvisioningDomainConfig(
    const std::string& json_response);

// Finds the first proxy endpoint in `config` that matches the given
// `destination_url` and `proxy_chain`. This is used during 407 Proxy
// Authentication handling to look up the authentication configuration
// (`ProxyAuthConfig`) and extra headers for a request.
// Returns nullptr if no matching route or endpoint is found.
const ProvisioningDomainProxyConfig::ProxyEndpoint* FindMatchingProxyEndpoint(
    const ProvisioningDomainProxyConfig& config,
    const GURL& destination_url,
    const net::ProxyChain& proxy_chain);

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_
