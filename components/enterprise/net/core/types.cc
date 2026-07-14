// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/types.h"

#include <utility>

namespace enterprise_net {

// ProxyExtraHeader
ProxyExtraHeader::ProxyExtraHeader() = default;
ProxyExtraHeader::ProxyExtraHeader(const ProxyExtraHeader&) = default;
ProxyExtraHeader::ProxyExtraHeader(ProxyExtraHeader&&) noexcept = default;
ProxyExtraHeader& ProxyExtraHeader::operator=(const ProxyExtraHeader&) =
    default;
ProxyExtraHeader& ProxyExtraHeader::operator=(ProxyExtraHeader&&) noexcept =
    default;
ProxyExtraHeader::~ProxyExtraHeader() = default;

ProxyExtraHeader::ProxyExtraHeader(std::string key,
                                   std::string value,
                                   HeaderType type)
    : key(std::move(key)), value(std::move(value)), type(type) {}

// ProvisioningDomainConfig
ProvisioningDomainConfig::ProvisioningDomainConfig() = default;
ProvisioningDomainConfig::ProvisioningDomainConfig(
    const ProvisioningDomainConfig&) = default;
ProvisioningDomainConfig::ProvisioningDomainConfig(
    ProvisioningDomainConfig&&) noexcept = default;
ProvisioningDomainConfig& ProvisioningDomainConfig::operator=(
    const ProvisioningDomainConfig&) = default;
ProvisioningDomainConfig& ProvisioningDomainConfig::operator=(
    ProvisioningDomainConfig&&) noexcept = default;
ProvisioningDomainConfig::~ProvisioningDomainConfig() = default;

// ProvisioningDomainProxyConfig::ProxyEndpoint
ProvisioningDomainProxyConfig::ProxyEndpoint::ProxyEndpoint() = default;
ProvisioningDomainProxyConfig::ProxyEndpoint::ProxyEndpoint(
    const ProxyEndpoint&) = default;
ProvisioningDomainProxyConfig::ProxyEndpoint::ProxyEndpoint(
    ProxyEndpoint&&) noexcept = default;
ProvisioningDomainProxyConfig::ProxyEndpoint&
ProvisioningDomainProxyConfig::ProxyEndpoint::operator=(const ProxyEndpoint&) =
    default;
ProvisioningDomainProxyConfig::ProxyEndpoint&
ProvisioningDomainProxyConfig::ProxyEndpoint::operator=(
    ProxyEndpoint&&) noexcept = default;
ProvisioningDomainProxyConfig::ProxyEndpoint::~ProxyEndpoint() = default;

ProvisioningDomainProxyConfig::ProxyEndpoint::ProxyEndpoint(
    const net::ProxyChain& proxy_chain)
    : proxy_chain(proxy_chain) {}

ProvisioningDomainProxyConfig::ProxyEndpoint::ProxyEndpoint(
    const net::ProxyChain& proxy_chain,
    std::optional<ProxyAuthConfig> auth,
    std::vector<ProxyExtraHeader> extra_headers)
    : proxy_chain(proxy_chain),
      auth(auth),
      extra_headers(std::move(extra_headers)) {}

// ProvisioningDomainProxyConfig::RoutingRule
ProvisioningDomainProxyConfig::RoutingRule::RoutingRule() = default;
ProvisioningDomainProxyConfig::RoutingRule::RoutingRule(const RoutingRule&) =
    default;
ProvisioningDomainProxyConfig::RoutingRule::RoutingRule(
    RoutingRule&&) noexcept = default;
ProvisioningDomainProxyConfig::RoutingRule&
ProvisioningDomainProxyConfig::RoutingRule::operator=(const RoutingRule&) =
    default;
ProvisioningDomainProxyConfig::RoutingRule&
ProvisioningDomainProxyConfig::RoutingRule::operator=(RoutingRule&&) noexcept =
    default;
ProvisioningDomainProxyConfig::RoutingRule::~RoutingRule() = default;

ProvisioningDomainProxyConfig::RoutingRule::RoutingRule(
    std::vector<std::string> proxies,
    const net::ProxyHostMatchingRules& destination_matchers)
    : proxies(std::move(proxies)), destination_matchers(destination_matchers) {}

// ProvisioningDomainProxyConfig
ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig() = default;
ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig(
    const ProvisioningDomainProxyConfig&) = default;
ProvisioningDomainProxyConfig::ProvisioningDomainProxyConfig(
    ProvisioningDomainProxyConfig&&) noexcept = default;
ProvisioningDomainProxyConfig& ProvisioningDomainProxyConfig::operator=(
    const ProvisioningDomainProxyConfig&) = default;
ProvisioningDomainProxyConfig& ProvisioningDomainProxyConfig::operator=(
    ProvisioningDomainProxyConfig&&) noexcept = default;
ProvisioningDomainProxyConfig::~ProvisioningDomainProxyConfig() = default;

net::ProxyConfig::DynamicRoutingConfig
ProvisioningDomainProxyConfig::ToDynamicRoutingConfig() const {
  net::ProxyConfig::DynamicRoutingConfig config;
  config.routing_rules.reserve(routing_rules.size());
  for (const auto& rule : routing_rules) {
    net::ProxyConfig::DynamicRoutingRule dynamic_rule;
    dynamic_rule.destination_matchers = rule.destination_matchers;
    net::ProxyList proxy_list;
    for (const std::string& proxy_id : rule.proxies) {
      if (proxy_id == "DIRECT") {
        proxy_list.AddProxyChain(net::ProxyChain::Direct());
      } else {
        auto it = proxy_endpoints.find(proxy_id);
        if (it != proxy_endpoints.end()) {
          proxy_list.AddProxyChain(it->second.proxy_chain);
        }
      }
    }
    dynamic_rule.proxy_list = std::move(proxy_list);
    config.routing_rules.push_back(std::move(dynamic_rule));
  }
  return config;
}

}  // namespace enterprise_net
