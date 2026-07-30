// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_
#define COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/time/time.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_config.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"
#include "url/gurl.h"

namespace enterprise_net {

// Supported authentication types for Provisioning Domains.
enum class AuthType {
  kNone = 0,
  kProfileBearerToken,
};

// Supported OAuth scopes for Provisioning Domains authentication.
enum class AuthScope {
  kNone = 0,
  kCloudSecureGateway,
};

// Authentication configuration for a proxy.
struct ProxyAuthConfig {
  AuthType type = AuthType::kNone;
  AuthScope scope = AuthScope::kNone;

  bool operator==(const ProxyAuthConfig&) const = default;
};

// Extra header associated with a proxy or a provisioning domain.
struct ProxyExtraHeader {
  enum class HeaderType {
    kConstant = 0,
    kVariable,
  };

  ProxyExtraHeader();
  ProxyExtraHeader(const ProxyExtraHeader&);
  ProxyExtraHeader(ProxyExtraHeader&&) noexcept;
  ProxyExtraHeader& operator=(const ProxyExtraHeader&);
  ProxyExtraHeader& operator=(ProxyExtraHeader&&) noexcept;
  ~ProxyExtraHeader();

  ProxyExtraHeader(std::string key,
                   std::string value,
                   HeaderType type = HeaderType::kConstant);

  bool operator==(const ProxyExtraHeader&) const = default;

  std::string key;
  std::string value;
  HeaderType type = HeaderType::kConstant;
};

// A single config entry defined in the "ProxyProvisioningDomains" policy.
struct ProvisioningDomainConfig {
  ProvisioningDomainConfig();
  ProvisioningDomainConfig(const ProvisioningDomainConfig&);
  ProvisioningDomainConfig(ProvisioningDomainConfig&&) noexcept;
  ProvisioningDomainConfig& operator=(const ProvisioningDomainConfig&);
  ProvisioningDomainConfig& operator=(ProvisioningDomainConfig&&) noexcept;
  ~ProvisioningDomainConfig();

  bool operator==(const ProvisioningDomainConfig&) const = default;

  // Provisioning Domain Identifier (FQDN).
  std::string pvd_id;

  // Optional authentication configuration used when querying the PvD endpoint.
  std::optional<ProxyAuthConfig> auth_config;

  // Additional HTTP headers to attach when fetching the PvD JSON configuration
  // from the PvD endpoint.
  std::vector<ProxyExtraHeader> extra_headers;
};

// Public structure representing a fetched Provisioning Domain configuration
// alongside its current fetch state.
// TODO(crbug.com/540422559): Change the transient error definition to
// distinguish between blocked and currently retry-able transient errors.
struct ProvisioningDomainProxyConfig {
  enum class State {
    kRefreshNeeded = 0,
    kFetching,
    kValid,
    kFailedTransient,
    kFailedPermanent,
  };

  struct ProxyEndpoint {
    ProxyEndpoint();
    ProxyEndpoint(const ProxyEndpoint&);
    ProxyEndpoint(ProxyEndpoint&&) noexcept;
    ProxyEndpoint& operator=(const ProxyEndpoint&);
    ProxyEndpoint& operator=(ProxyEndpoint&&) noexcept;
    ~ProxyEndpoint();

    explicit ProxyEndpoint(const net::ProxyChain& proxy_chain);
    ProxyEndpoint(const net::ProxyChain& proxy_chain,
                  std::optional<ProxyAuthConfig> auth,
                  std::vector<ProxyExtraHeader> extra_headers);

    bool operator==(const ProxyEndpoint&) const = default;

    // The proxy chain endpoint (e.g., "[https://proxy1.example.com:443]").
    net::ProxyChain proxy_chain;
    // Optional authentication configuration for this proxy endpoint.
    std::optional<ProxyAuthConfig> auth;
    // Optional extra headers to be sent to this proxy endpoint.
    std::vector<ProxyExtraHeader> extra_headers;
  };

  // Defines the routing rules in a Provisioning Domain configuration.
  // Combines:
  //   1) `proxies`: browser-only proxy identifier strings used when
  //   intercepting
  //      407 Proxy Authentication requests to map back to the matching
  //      `ProxyEndpoint` in `proxy_endpoints`.
  //   2) `destination_matchers`: destination patterns matched against
  //      request URLs.
  struct RoutingRule {
    RoutingRule();
    RoutingRule(const RoutingRule&);
    RoutingRule(RoutingRule&&) noexcept;
    RoutingRule& operator=(const RoutingRule&);
    RoutingRule& operator=(RoutingRule&&) noexcept;
    ~RoutingRule();

    RoutingRule(std::vector<std::string> proxies,
                const net::ProxyHostMatchingRules& destination_matchers);

    bool operator==(const RoutingRule&) const = default;

    // List of proxy identifier strings that matching requests
    // resolve against `proxy_endpoints` to obtain authentication method and
    // extra headers.
    std::vector<std::string> proxies;

    // Destination matching rules for URLs.
    net::ProxyHostMatchingRules destination_matchers;
  };

  ProvisioningDomainProxyConfig();
  ProvisioningDomainProxyConfig(const ProvisioningDomainProxyConfig&);
  ProvisioningDomainProxyConfig(ProvisioningDomainProxyConfig&&) noexcept;
  ProvisioningDomainProxyConfig& operator=(
      const ProvisioningDomainProxyConfig&);
  ProvisioningDomainProxyConfig& operator=(
      ProvisioningDomainProxyConfig&&) noexcept;
  ~ProvisioningDomainProxyConfig();

  bool operator==(const ProvisioningDomainProxyConfig&) const = default;

  // Converts this configuration's routing rules into the network stack's
  // `DynamicRoutingConfig` representation (`net::ProxyConfig`).
  // Strips all browser-side PvD metadata (such as proxy identifiers and auth
  // configuration) and exports only the merged list of `DynamicRoutingRule`s
  // from all `routing_rules`, with the relative ordering preserved.
  //
  // Returns any cached routing rules regardless of config state (e.g. during
  // `kRefreshNeeded` or fetch failures) so existing connections remain active.
  net::ProxyConfig::DynamicRoutingConfig ToDynamicRoutingConfig() const;

  std::string pvd_id;
  base::Time expires;
  // Map of proxy identity to its configuration.
  base::flat_map<std::string, ProxyEndpoint> proxy_endpoints;
  std::vector<RoutingRule> routing_rules;
  State state = State::kRefreshNeeded;
};

}  // namespace enterprise_net

#endif  // COMPONENTS_ENTERPRISE_NET_CORE_TYPES_H_
