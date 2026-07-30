// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/utils.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/json/json_reader.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/policy/core/common/values_util.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_util.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace enterprise_net {

namespace {

// JSON keys used in parsing ProxyProvisioningDomains policy entries.
constexpr char kPvdIdKey[] = "pvd_id";

// JSON keys used in parsing Provisioning Domain (PvD) server responses.
constexpr char kIdentifierKey[] = "identifier";
constexpr char kExpiresKey[] = "expires";
constexpr char kProxiesKey[] = "proxies";
constexpr char kProtocolKey[] = "protocol";
constexpr char kProxyKey[] = "proxy";
constexpr char kGoogleChromeKey[] = "google_chrome";
constexpr char kProxyMatchKey[] = "proxy-match";
constexpr char kDomainsKey[] = "domains";
constexpr char kSubnetsKey[] = "subnets";
constexpr char kPortsKey[] = "ports";

// JSON keys shared between policy entries, PvD server responses, and headers.
constexpr char kAuthConfigKey[] = "auth_config";
constexpr char kExtraHeadersKey[] = "extra_headers";
constexpr char kAuthKey[] = "auth";
constexpr char kKeyKey[] = "key";
constexpr char kValueKey[] = "value";
constexpr char kTypeKey[] = "type";
constexpr char kScopeKey[] = "scope";

// Constants for placeholders used in extra headers.
constexpr char kProfileIdPlaceholder[] = "${profile_id}";
constexpr char kAcceptLanguagePlaceholder[] = "${accept_language}";

// Auth type and scope string values.
constexpr char kAuthNone[] = "none";
constexpr char kAuthTypeProfileBearerToken[] = "profile_bearer_token";
constexpr char kAuthScopeCloudSecureGateway[] = "cloud_secure_gateway";

std::string AuthTypeToString(AuthType type) {
  switch (type) {
    case AuthType::kNone:
      return kAuthNone;
    case AuthType::kProfileBearerToken:
      return kAuthTypeProfileBearerToken;
  }
}

std::string AuthScopeToString(AuthScope scope) {
  switch (scope) {
    case AuthScope::kNone:
      return kAuthNone;
    case AuthScope::kCloudSecureGateway:
      return kAuthScopeCloudSecureGateway;
  }
}

std::string StateToString(ProvisioningDomainProxyConfig::State state) {
  switch (state) {
    case ProvisioningDomainProxyConfig::State::kRefreshNeeded:
      return "RefreshNeeded";
    case ProvisioningDomainProxyConfig::State::kFetching:
      return "Fetching";
    case ProvisioningDomainProxyConfig::State::kValid:
      return "Valid";
    case ProvisioningDomainProxyConfig::State::kFailedTransient:
      return "FailedTransient";
    case ProvisioningDomainProxyConfig::State::kFailedPermanent:
      return "FailedPermanent";
  }
}

std::string HeaderTypeToString(ProxyExtraHeader::HeaderType type) {
  switch (type) {
    case ProxyExtraHeader::HeaderType::kConstant:
      return "constant";
    case ProxyExtraHeader::HeaderType::kVariable:
      return "variable";
  }
}

base::ListValue ExtraHeadersToList(
    const std::vector<ProxyExtraHeader>& extra_headers) {
  base::ListValue list;
  for (const auto& header : extra_headers) {
    base::DictValue dict;
    dict.Set(kKeyKey, header.key);
    dict.Set(kValueKey, header.value);
    dict.Set(kTypeKey, HeaderTypeToString(header.type));
    list.Append(std::move(dict));
  }
  return list;
}

base::DictValue AuthConfigToDict(const ProxyAuthConfig& auth) {
  base::DictValue dict;
  dict.Set(kTypeKey, AuthTypeToString(auth.type));
  dict.Set(kScopeKey, AuthScopeToString(auth.scope));
  return dict;
}

struct PlaceholderReplacement {
  std::string_view placeholder;
  std::string_view replacement;
};

void ExpandPlaceholders(
    std::string* value,
    std::initializer_list<PlaceholderReplacement> replacements) {
  for (const auto& r : replacements) {
    base::ReplaceSubstringsAfterOffset(value, 0, r.placeholder, r.replacement);
  }
}

AuthType ParseAuthType(std::string_view type_str) {
  std::string normalized = base::ToLowerASCII(type_str);
  static constexpr auto kAuthTypeMap =
      base::MakeFixedFlatMap<std::string_view, AuthType>({
          {kAuthNone, AuthType::kNone},
          {kAuthTypeProfileBearerToken, AuthType::kProfileBearerToken},
      });
  auto it = kAuthTypeMap.find(normalized);
  return it != kAuthTypeMap.end() ? it->second : AuthType::kNone;
}

AuthScope ParseAuthScope(std::string_view scope_str) {
  std::string normalized = base::ToLowerASCII(scope_str);
  static constexpr auto kAuthScopeMap =
      base::MakeFixedFlatMap<std::string_view, AuthScope>({
          {kAuthNone, AuthScope::kNone},
          {kAuthScopeCloudSecureGateway, AuthScope::kCloudSecureGateway},
      });
  auto it = kAuthScopeMap.find(normalized);
  return it != kAuthScopeMap.end() ? it->second : AuthScope::kNone;
}

std::vector<ProxyExtraHeader> ParseExtraHeadersList(
    const base::ListValue* extra_headers_list) {
  std::vector<ProxyExtraHeader> extra_headers;
  if (!extra_headers_list) {
    return extra_headers;
  }
  for (const auto& header_value : *extra_headers_list) {
    if (!header_value.is_dict()) {
      continue;
    }
    const base::DictValue& header_dict = header_value.GetDict();
    const std::string* key = header_dict.FindString(kKeyKey);
    if (!key || key->empty()) {
      continue;
    }

    if (const std::string* constant_val = header_dict.FindString("constant")) {
      extra_headers.emplace_back(*key, *constant_val,
                                 ProxyExtraHeader::HeaderType::kConstant);
    } else if (const std::string* variable_val =
                   header_dict.FindString("variable")) {
      extra_headers.emplace_back(*key, *variable_val,
                                 ProxyExtraHeader::HeaderType::kVariable);
    } else if (const std::string* value = header_dict.FindString(kValueKey)) {
      const std::string* type = header_dict.FindString(kTypeKey);
      auto header_type = (type && *type == "variable")
                             ? ProxyExtraHeader::HeaderType::kVariable
                             : ProxyExtraHeader::HeaderType::kConstant;
      extra_headers.emplace_back(*key, *value, header_type);
    }
  }
  return extra_headers;
}

std::optional<
    std::pair<std::string, ProvisioningDomainProxyConfig::ProxyEndpoint>>
ParseProxy(const base::DictValue& proxy_dict) {
  const std::string* protocol = proxy_dict.FindString(kProtocolKey);
  const std::string* proxy_uri = proxy_dict.FindString(kProxyKey);
  if (!protocol || protocol->empty() || !proxy_uri || proxy_uri->empty()) {
    return std::nullopt;
  }

  net::ProxyServer::Scheme scheme =
      ParseProvisioningDomainProxyProtocol(*protocol);
  if (scheme == net::ProxyServer::SCHEME_INVALID) {
    return std::nullopt;
  }

  net::ProxyServer proxy_server =
      net::ProxyUriToProxyServer(*proxy_uri, scheme);

  if (!proxy_server.is_valid()) {
    return std::nullopt;
  }

  net::ProxyChain proxy_chain(proxy_server);
  if (!proxy_chain.IsValid()) {
    return std::nullopt;
  }

  // "identifier" is optional in the draft; if omitted, fall back to the proxy
  // URI.
  const std::string* identifier = proxy_dict.FindString(kIdentifierKey);
  if (!identifier || identifier->empty()) {
    identifier = proxy_dict.FindString("identity");
  }
  std::string id =
      (identifier && !identifier->empty()) ? *identifier : *proxy_uri;

  std::optional<ProxyAuthConfig> auth;
  std::vector<ProxyExtraHeader> extra_headers;
  // Parse optional google_chrome dictionary.
  if (const base::DictValue* chrome_dict =
          proxy_dict.FindDict(kGoogleChromeKey)) {
    if (const base::DictValue* auth_dict = chrome_dict->FindDict(kAuthKey)) {
      ProxyAuthConfig parsed_auth;
      if (const std::string* type = auth_dict->FindString(kTypeKey)) {
        parsed_auth.type = ParseAuthType(*type);
      }
      if (const std::string* scope = auth_dict->FindString(kScopeKey)) {
        parsed_auth.scope = ParseAuthScope(*scope);
      }
      auth = parsed_auth;
    }
    extra_headers =
        ParseExtraHeadersList(chrome_dict->FindList(kExtraHeadersKey));
  }

  return std::make_pair(
      std::move(id),
      ProvisioningDomainProxyConfig::ProxyEndpoint(
          std::move(proxy_chain), std::move(auth), std::move(extra_headers)));
}

}  // namespace

std::optional<ProvisioningDomainProxyConfig::RoutingRule> ParseRoutingRule(
    const base::DictValue& match_dict) {
  std::vector<std::string> proxies;
  if (const base::ListValue* proxies_list = match_dict.FindList(kProxiesKey)) {
    for (const auto& proxy_value : *proxies_list) {
      if (proxy_value.is_string()) {
        proxies.push_back(proxy_value.GetString());
      }
    }
  }
  // In the Provisioning Domain specification, if `proxies` is empty array or
  // omitted, go DIRECT for matching traffic.
  if (proxies.empty()) {
    proxies.push_back("DIRECT");
  }

  std::vector<std::string> domains;
  if (const base::ListValue* domains_list = match_dict.FindList(kDomainsKey)) {
    for (const auto& domain_value : *domains_list) {
      if (domain_value.is_string()) {
        domains.push_back(domain_value.GetString());
      }
    }
  }

  std::vector<std::string> subnets;
  if (const base::ListValue* subnets_list = match_dict.FindList(kSubnetsKey)) {
    for (const auto& subnet_value : *subnets_list) {
      if (subnet_value.is_string()) {
        subnets.push_back(subnet_value.GetString());
      }
    }
  }

  std::vector<uint16_t> ports;
  if (const base::ListValue* ports_list = match_dict.FindList(kPortsKey)) {
    for (const auto& port_value : *ports_list) {
      // TODO(crbug.com/538199264): PvD standard supports port range strings
      // (e.g. "1024-65535"), and ProxyHostMatchingRules is missing support
      // for this. Only viable method for now is to create a single port matcher
      // for each port in the range, but it can lead to memory explosion.
      // We should add proper port range support to ProxyHostMatchingRules.
      if (port_value.is_string()) {
        uint32_t port = 0;
        if (base::StringToUint(port_value.GetString(), &port) &&
            port <= 65535) {
          ports.push_back(static_cast<uint16_t>(port));
        }
      } else if (port_value.is_int()) {
        int port = port_value.GetInt();
        if (port >= 0 && port <= 65535) {
          ports.push_back(static_cast<uint16_t>(port));
        }
      }
    }
  }

  net::ProxyHostMatchingRules destination_matchers;

  if (domains.empty() && subnets.empty()) {
    // If no domain or subnet patterns are specified, match all traffic ("*").
    if (!ports.empty()) {
      for (uint16_t port : ports) {
        destination_matchers.AddRuleFromString(
            base::StrCat({"*:", base::NumberToString(port)}));
      }
    } else {
      destination_matchers.AddRuleFromString("*");
    }
  } else {
    // Combine domains and ports.
    for (const auto& domain : domains) {
      std::vector<std::string> patterns_to_add = {domain};
      if (domain.starts_with("*.")) {
        // By PvD proxy routing standard, entries that include a wildcard prefix
        // (*.domain.com) also match the FQDN with no subdomain (domain.com)
        patterns_to_add.push_back(domain.substr(2));
      }

      for (const auto& pattern : patterns_to_add) {
        if (!ports.empty()) {
          for (uint16_t port : ports) {
            destination_matchers.AddRuleFromString(
                base::StrCat({pattern, ":", base::NumberToString(port)}));
          }
        } else {
          destination_matchers.AddRuleFromString(pattern);
        }
      }
    }

    // Combine subnets and ports.
    for (const auto& subnet : subnets) {
      if (!ports.empty()) {
        for (uint16_t port : ports) {
          destination_matchers.AddRuleFromString(
              base::StrCat({subnet, ":", base::NumberToString(port)}));
        }
      } else {
        destination_matchers.AddRuleFromString(subnet);
      }
    }
  }

  return ProvisioningDomainProxyConfig::RoutingRule(
      std::move(proxies), std::move(destination_matchers));
}

net::ProxyServer::Scheme ParseProvisioningDomainProxyProtocol(
    std::string_view protocol_str) {
  std::string normalized = base::ToLowerASCII(protocol_str);
  static constexpr auto kProtocolMap =
      base::MakeFixedFlatMap<std::string_view, net::ProxyServer::Scheme>({
          {"socks5", net::ProxyServer::SCHEME_SOCKS5},
          {"http-connect", net::ProxyServer::SCHEME_HTTP},
          {"https-connect", net::ProxyServer::SCHEME_HTTPS},
      });
  auto it = kProtocolMap.find(normalized);
  return it != kProtocolMap.end() ? it->second
                                  : net::ProxyServer::SCHEME_INVALID;
}

net::HttpRequestHeaders ResolveExtraHeadersWithValues(
    const std::vector<ProxyExtraHeader>& extra_headers,
    const std::string& profile_id,
    const std::string& accept_languages) {
  std::initializer_list<PlaceholderReplacement> replacements = {
      {kProfileIdPlaceholder, profile_id},
      {kAcceptLanguagePlaceholder, accept_languages},
  };

  net::HttpRequestHeaders headers;
  for (const auto& header : extra_headers) {
    if (header.type == ProxyExtraHeader::HeaderType::kConstant) {
      headers.SetHeader(header.key, header.value);
      continue;
    }

    if (header.type == ProxyExtraHeader::HeaderType::kVariable) {
      std::string expanded_value = header.value;
      ExpandPlaceholders(&expanded_value, replacements);
      // Drop header if it contains unsupported or unrecognized variable
      // placeholders.
      if (expanded_value.find("${") != std::string::npos) {
        continue;
      }
      headers.SetHeader(header.key, expanded_value);
    }
  }
  return headers;
}

std::optional<ProvisioningDomainConfig> ParseProxyProvisioningDomainPolicy(
    const base::DictValue& domain_dict) {
  const std::string* pvd_id = domain_dict.FindString(kPvdIdKey);
  if (!pvd_id || pvd_id->empty()) {
    return std::nullopt;
  }

  ProvisioningDomainConfig policy;
  policy.pvd_id = *pvd_id;

  if (const base::DictValue* auth_dict = domain_dict.FindDict(kAuthConfigKey)) {
    ProxyAuthConfig auth;
    if (const std::string* type = auth_dict->FindString(kTypeKey)) {
      auth.type = ParseAuthType(*type);
    }
    if (const std::string* scope = auth_dict->FindString(kScopeKey)) {
      auth.scope = ParseAuthScope(*scope);
    }
    policy.auth_config = auth;
  }

  policy.extra_headers =
      ParseExtraHeadersList(domain_dict.FindList(kExtraHeadersKey));

  return policy;
}

std::optional<ProvisioningDomainProxyConfig> ParseProvisioningDomainConfig(
    const base::DictValue& dict) {
  const base::ListValue* proxies_list = dict.FindList(kProxiesKey);
  const base::ListValue* proxy_match_list = dict.FindList(kProxyMatchKey);
  if (!proxies_list || !proxy_match_list) {
    return std::nullopt;
  }

  ProvisioningDomainProxyConfig config_data;

  // Parse identifier and expiration time.
  if (const std::string* identifier = dict.FindString(kIdentifierKey)) {
    config_data.pvd_id = *identifier;
  }

  if (const std::string* expires_str = dict.FindString(kExpiresKey)) {
    base::Time expires_time;
    if (base::Time::FromString(expires_str->c_str(), &expires_time)) {
      config_data.expires = expires_time;
    }
  }

  // Parse proxies.
  for (const auto& proxy_value : *proxies_list) {
    if (!proxy_value.is_dict()) {
      continue;
    }
    auto parsed_proxy = ParseProxy(proxy_value.GetDict());
    if (parsed_proxy.has_value()) {
      config_data.proxy_endpoints.insert(std::move(*parsed_proxy));
    }
  }

  // Parse proxy-match routing rules.
  for (const auto& match_value : *proxy_match_list) {
    if (!match_value.is_dict()) {
      continue;
    }
    std::optional<ProvisioningDomainProxyConfig::RoutingRule> routing_rule =
        ParseRoutingRule(match_value.GetDict());
    if (routing_rule.has_value()) {
      config_data.routing_rules.push_back(std::move(*routing_rule));
    }
  }

  return config_data;
}

std::optional<ProvisioningDomainProxyConfig> ParseProvisioningDomainConfig(
    const std::string& json_response) {
  std::optional<base::DictValue> parsed_json =
      base::JSONReader::ReadDict(json_response, 0);
  if (!parsed_json.has_value()) {
    return std::nullopt;
  }

  return ParseProvisioningDomainConfig(*parsed_json);
}

const ProvisioningDomainProxyConfig::ProxyEndpoint* FindMatchingProxyEndpoint(
    const ProvisioningDomainProxyConfig& config,
    const GURL& destination_url,
    const net::ProxyChain& proxy_chain) {
  for (const auto& rule : config.routing_rules) {
    if (!rule.destination_matchers.Matches(destination_url)) {
      continue;
    }
    for (const std::string& proxy_id : rule.proxies) {
      auto it = config.proxy_endpoints.find(proxy_id);
      if (it != config.proxy_endpoints.end() &&
          it->second.proxy_chain == proxy_chain) {
        return &it->second;
      }
    }
  }
  return nullptr;
}

std::string ComputePolicyHash(const ProvisioningDomainConfig& policy_config) {
  return base::NumberToString(policy::PolicyValueHash(
      base::Value(ProvisioningDomainConfigToDict(policy_config))));
}

base::DictValue ProvisioningDomainConfigToDict(
    const ProvisioningDomainConfig& policy_config) {
  base::DictValue dict;
  dict.Set(kPvdIdKey, policy_config.pvd_id);
  if (policy_config.auth_config.has_value()) {
    dict.Set(kAuthConfigKey, AuthConfigToDict(*policy_config.auth_config));
  }
  if (!policy_config.extra_headers.empty()) {
    dict.Set(kExtraHeadersKey, ExtraHeadersToList(policy_config.extra_headers));
  }
  return dict;
}

base::DictValue ProvisioningDomainProxyConfigToDict(
    const ProvisioningDomainProxyConfig& proxy_config) {
  base::DictValue dict;
  dict.Set(kIdentifierKey, proxy_config.pvd_id);
  dict.Set("state", StateToString(proxy_config.state));
  if (!proxy_config.expires.is_null()) {
    dict.Set(kExpiresKey, net::HttpUtil::TimeFormatHTTP(proxy_config.expires));
  }

  base::ListValue endpoints_list;
  for (const auto& [id, endpoint] : proxy_config.proxy_endpoints) {
    base::DictValue endpoint_dict;
    endpoint_dict.Set(kIdentifierKey, id);
    endpoint_dict.Set(kProtocolKey, "https-connect");
    if (endpoint.proxy_chain.IsValid() && !endpoint.proxy_chain.is_direct()) {
      endpoint_dict.Set(
          kProxyKey, net::ProxyServerToProxyUri(endpoint.proxy_chain.First()));
    }
    base::DictValue chrome_dict;
    if (endpoint.auth.has_value()) {
      chrome_dict.Set(kAuthKey, AuthConfigToDict(*endpoint.auth));
    }
    if (!endpoint.extra_headers.empty()) {
      chrome_dict.Set(kExtraHeadersKey,
                      ExtraHeadersToList(endpoint.extra_headers));
    }
    if (!chrome_dict.empty()) {
      endpoint_dict.Set(kGoogleChromeKey, std::move(chrome_dict));
    }
    endpoints_list.Append(std::move(endpoint_dict));
  }
  dict.Set(kProxiesKey, std::move(endpoints_list));

  base::ListValue rules_list;
  for (const auto& rule : proxy_config.routing_rules) {
    base::DictValue rule_dict;
    base::ListValue proxies_list;
    for (const auto& proxy_id : rule.proxies) {
      proxies_list.Append(proxy_id);
    }
    rule_dict.Set(kProxiesKey, std::move(proxies_list));

    base::ListValue matchers_list;
    for (const auto& matcher_rule : rule.destination_matchers.rules()) {
      matchers_list.Append(matcher_rule->ToString());
    }
    rule_dict.Set(kDomainsKey, std::move(matchers_list));
    rules_list.Append(std::move(rule_dict));
  }
  dict.Set(kProxyMatchKey, std::move(rules_list));

  return dict;
}

}  // namespace enterprise_net
