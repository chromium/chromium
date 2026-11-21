// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "components/enterprise/net/core/types.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/http/http_request_headers.h"

class PrefService;

namespace net {
class AuthChallengeInfo;
class HttpResponseHeaders;
}  // namespace net

namespace enterprise_net {

// Returns a string representation of a `ProvisioningDomainProxyConfig::State`.
std::string_view ProvisioningDomainStateToString(
    ProvisioningDomainProxyConfig::State state);

// Parses a Provisioning Domain proxy protocol string (e.g., "socks5",
// "http-connect", "https-connect") to the corresponding
// net::ProxyServer::Scheme. Returns SCHEME_INVALID for unsupported or invalid
// protocols.
net::ProxyServer::Scheme ParseProvisioningDomainProxyProtocol(
    std::string_view protocol_str);

// Resolves and expands policy extra headers into a net::HttpRequestHeaders
// object.
// - `kConstant` headers are set directly.
// - `kVariable` headers will have their supported placeholders expanded.
//
// Supported variable placeholders:
//   - `${profile_id}`
//   - `${accept_language}`
//
// If a `kVariable` header contains unsupported/unrecognized variable
// placeholders, it will be dropped.
net::HttpRequestHeaders ResolveExtraHeadersWithValues(
    const std::vector<ProxyExtraHeader>& extra_headers,
    const std::string& profile_id,
    const std::string& accept_languages);

// Parses a single "ProxyProvisioningDomains" policy entry dictionary into
// a ProvisioningDomainConfig struct.
// Returns std::nullopt if the entry is invalid or missing a valid pvd_id.
std::optional<ProvisioningDomainConfig> ParseProxyProvisioningDomainPolicy(
    const base::DictValue& domain_dict);

// Parses a PvD configuration dictionary into a
// ProvisioningDomainProxyConfig struct. Returns std::nullopt if the dictionary
// is invalid or missing required fields.
std::optional<ProvisioningDomainProxyConfig> ParseProvisioningDomainConfig(
    const base::DictValue& dict);

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

// Computes a hash string for a `ProvisioningDomainConfig` policy struct. The
// result keys the persisted `kProvisioningDomainProxyConfigs` cache, so it is
// stable across launches and releases.
std::string ComputePolicyHash(const ProvisioningDomainConfig& policy_config);

// Serializes a ProvisioningDomainConfig struct into a base::DictValue
// representation.
base::DictValue ProvisioningDomainConfigToDict(
    const ProvisioningDomainConfig& policy_config);

// Serializes a ProvisioningDomainProxyConfig struct into a base::DictValue
// representation.
base::DictValue ProvisioningDomainProxyConfigToDict(
    const ProvisioningDomainProxyConfig& proxy_config);

// Parses a single "proxy-match" routing rule dictionary into a RoutingRule
// struct.
// On iOS the `ports` field is ignored: WebKit's proxy configuration API cannot
// match on port, so the returned rule is widened to all ports of the matched
// destinations. If no domains/subnets are declared, stripping ports widens the
// rule to all traffic (`*`). See crbug.com/552534571.
// On iOS, `domains` entries naming a single host are rewritten to `*.<host>` to
// mirror WebKit's suffix-based proxy domain matching. Since `*.` entries also
// add the bare domain, the rule matches both `<host>` and its subdomains. See
// crbug.com/567512841.
std::optional<ProvisioningDomainProxyConfig::RoutingRule> ParseRoutingRule(
    const base::DictValue& match_dict);

// Returns true if Secure Gateway is enabled and its privacy disclosure
// should be displayed (currently applicable on Android and iOS).
bool IsSecureGatewayEnabled(PrefService* prefs);

// Case-insensitive parameter map parsed from an HTTP `Basic` authentication
// challenge (all keys normalized to lowercase ASCII).
using BasicAuthChallengeParams = base::flat_map<std::string, std::string>;

// Parses comma-delimited `key=value` / `key="value"` auth-params from a single
// HTTP `Basic` challenge string (e.g. `Basic realm="403",url="https://..."`)
// using `net::HttpUtil::NameValuePairsIterator` (`STRICT_QUOTES`). Enforces RFC
// 9110 `token` syntax for parameter names and unquoted parameter values, and
// unique case-insensitive parameter names. Returns `std::nullopt` if the scheme
// is not `Basic` or any parameter is malformed or duplicated.
std::optional<BasicAuthChallengeParams> ParseBasicAuthChallengeParams(
    std::string_view challenge);

// Finds and parses the HTTP `Basic` challenge matching `auth_info.realm` by
// enumerating `Proxy-Authenticate` headers in `response_headers` (when
// provided) or falling back to `auth_info.challenge`. Returns `std::nullopt` if
// `auth_info.scheme` is not `"basic"` (case-insensitive), any `Basic`
// `Proxy-Authenticate` header is malformed, no matching valid `Basic` challenge
// is found, or multiple `Proxy-Authenticate` headers match the same realm.
std::optional<BasicAuthChallengeParams> ExtractMatchingBasicAuthChallengeParams(
    const net::AuthChallengeInfo& auth_info,
    const net::HttpResponseHeaders* response_headers);

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_UTILS_H_
