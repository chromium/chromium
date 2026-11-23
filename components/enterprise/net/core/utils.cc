// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/net/core/utils.h"

#include <algorithm>
#include <initializer_list>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
#include "base/hash/hash.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "components/enterprise/net/core/auth_scope_metadata.h"
#include "components/enterprise/net/core/features.h"
#include "components/enterprise/net/core/prefs.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "net/base/auth.h"
#include "net/base/proxy_chain.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/http/http_auth_scheme.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_IOS)
#include "base/logging.h"
#include "net/base/ip_address.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#endif  // BUILDFLAG(IS_IOS)

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
constexpr char kDirectProxyId[] = "DIRECT";

// Prefix of a PvD domain pattern matching all subdomains of a domain.
constexpr std::string_view kWildcardSubdomainPrefix = "*.";

// JSON keys shared between policy entries, PvD server responses, and headers.
constexpr char kAuthConfigKey[] = "auth_config";
constexpr char kExtraHeadersKey[] = "extra_headers";
constexpr char kExtraHeadersHyphenKey[] = "extra-headers";
constexpr char kAuthKey[] = "auth";
constexpr char kKeyKey[] = "key";
constexpr char kValueKey[] = "value";
constexpr char kTypeKey[] = "type";
constexpr char kScopeKey[] = "scope";

// Constants for placeholders used in extra headers.
constexpr char kProfileIdPlaceholder[] = "${profile_id}";
constexpr char kProfileIdCamelPlaceholder[] = "${profileId}";
constexpr char kAcceptLanguagePlaceholder[] = "${accept_language}";
constexpr char kAcceptLanguageCamelPlaceholder[] = "${acceptLanguage}";

// Auth type string values.
constexpr char kAuthNone[] = "none";
constexpr char kAuthTypeProfileBearerToken[] = "profile_bearer_token";

// HTTP proxy authentication challenge constants.
constexpr std::string_view kProxyAuthenticateHeader = "Proxy-Authenticate";
constexpr std::string_view kRealmAuthParam = "realm";

bool HasBasicAuthScheme(std::string_view challenge) {
  base::StringViewTokenizer scheme_tok(challenge, HTTP_LWS);
  return scheme_tok.GetNext() &&
         base::EqualsCaseInsensitiveASCII(scheme_tok.token_piece(),
                                          net::kBasicAuthScheme);
}

std::optional<std::string_view> ExtractBasicAuthParamList(
    std::string_view challenge) {
  base::StringViewTokenizer scheme_tok(challenge, HTTP_LWS);
  if (!scheme_tok.GetNext() ||
      !base::EqualsCaseInsensitiveASCII(scheme_tok.token_piece(),
                                        net::kBasicAuthScheme)) {
    return std::nullopt;
  }
  std::string_view param_list = net::HttpUtil::TrimLWS(
      std::string_view(scheme_tok.token_end(), challenge.end()));
  if (param_list.empty()) {
    return std::nullopt;
  }
  return param_list;
}

bool HasMatchingRealm(const BasicAuthChallengeParams& params,
                      std::string_view expected_realm) {
  auto it = params.find(kRealmAuthParam);
  return it != params.end() && it->second == expected_realm;
}

std::string AuthTypeToString(AuthType type) {
  switch (type) {
    case AuthType::kNone:
      return kAuthNone;
    case AuthType::kProfileBearerToken:
      return kAuthTypeProfileBearerToken;
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
  if (std::optional<std::string_view> scope = AuthScopeToString(auth.scope)) {
    dict.Set(kScopeKey, *scope);
  }
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
      ProxyExtraHeader::HeaderType header_type =
          ProxyExtraHeader::HeaderType::kConstant;
      if (type) {
        if (base::EqualsCaseInsensitiveASCII(*type, "variable")) {
          header_type = ProxyExtraHeader::HeaderType::kVariable;
        }
      } else if (value->find("${") != std::string::npos) {
        header_type = ProxyExtraHeader::HeaderType::kVariable;
      }
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
      // Browser-handled proxy authentication (attaching OAuth bearer tokens
      // and honoring disguised error realms) requires TLS (`https://`). Plain
      // HTTP/SOCKS PvD proxies remain allowed only when no browser auth is
      // configured (no `auth` block or explicit `"type": "none"`), leaving any
      // 407 challenges to extensions or the login prompt.
      //
      // Check the raw `type` string rather than `ParseAuthType()` because
      // `ParseAuthType()` currently maps unrecognized auth types to
      // `AuthType::kNone` (see crbug.com/542666426); unrecognized or missing
      // `type` values must still fail closed on non-HTTPS proxies.
      const std::string* type = auth_dict->FindString(kTypeKey);
      const bool has_browser_auth =
          !type || !base::EqualsCaseInsensitiveASCII(*type, kAuthNone);
      if (has_browser_auth && !proxy_server.is_https()) {
        LOG_POLICY(WARNING, ENTERPRISE_PROXY)
            << "Ignoring non-HTTPS proxy '" << id
            << "' because browser proxy authentication requires HTTPS.";
        return std::nullopt;
      }

      ProxyAuthConfig parsed_auth;
      if (type) {
        parsed_auth.type = ParseAuthType(*type);
      }
      if (const std::string* scope = auth_dict->FindString(kScopeKey)) {
        parsed_auth.scope = ParseAuthScope(*scope);
      }
      auth = parsed_auth;
    }
    const base::ListValue* extra_headers_list =
        chrome_dict->FindList(kExtraHeadersKey);
    if (!extra_headers_list) {
      extra_headers_list = chrome_dict->FindList(kExtraHeadersHyphenKey);
    }
    extra_headers = ParseExtraHeadersList(extra_headers_list);
  }

  return std::make_pair(
      std::move(id),
      ProvisioningDomainProxyConfig::ProxyEndpoint(
          std::move(proxy_chain), std::move(auth), std::move(extra_headers)));
}

#if BUILDFLAG(IS_IOS)
// Clears `ports` so that the rule matches every port of its destinations.
// WebKit's proxy configuration API cannot match on port, so the only way to
// honor a rule that declares ports is to proxy the whole host/address.
void StripPortMatchers(std::vector<uint16_t>& ports, bool is_wildcard_rule) {
  if (ports.empty()) {
    return;
  }

  const char* affected_destinations =
      is_wildcard_rule ? "all destinations" : "the matched destinations";
  DVLOG(1) << "PvD routing rule declares " << ports.size()
           << " port matcher(s), but WebKit's proxy configuration API cannot "
              "match on port. Proxying all ports for "
           << affected_destinations << ".";

  ports.clear();
}

// Returns whether `domain` names a single host. Wildcard patterns (including
// the leading-dot form `net` treats as a wildcard), subnets and IP literals are
// excluded. Subnets and IPs show up in `domains` when a cached config is parsed
// because its serialized rules keep every matcher under `domains`.
bool IsPlainHostname(std::string_view domain) {
  if (domain.empty() || domain.starts_with('.') ||
      domain.find('*') != std::string_view::npos) {
    return false;
  }
  net::IPAddress address;
  size_t prefix_length_in_bits = 0;
  return !net::ParseCIDRBlock(domain, &address, &prefix_length_in_bits) &&
         !net::ParseURLHostnameToAddress(domain, &address);
}

// Rewrites plain hostnames in `domains` to `*.<hostname>`. WebKit's proxy
// configuration API matches domains as suffixes, so it routes every subdomain
// of `example.com` through the proxy. The rewrite gives the parsed rule the
// same reach so auth challenges for those subdomains resolve to this rule. The
// host itself keeps matching because `*.` entries also add the bare domain.
// `example.com` and `*.example.com` collapse into the same entry, so duplicates
// are dropped to keep re-parsing a serialized config stable. Duplicates are
// compared case-insensitively because `net` lowercases every pattern.
void WidenDomainsToSubdomains(std::vector<std::string>& domains) {
  absl::flat_hash_set<std::string> seen_domains;
  std::vector<std::string> widened_domains;
  widened_domains.reserve(domains.size());
  for (std::string& domain : domains) {
    if (IsPlainHostname(domain)) {
      domain = base::StrCat({kWildcardSubdomainPrefix, domain});
    }
    if (seen_domains.insert(base::ToLowerASCII(domain)).second) {
      widened_domains.push_back(std::move(domain));
    }
  }
  domains = std::move(widened_domains);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace

std::string_view ProvisioningDomainStateToString(
    ProvisioningDomainProxyConfig::State state) {
  switch (state) {
    case ProvisioningDomainProxyConfig::State::kRefreshNeeded:
      return "RefreshNeeded";
    case ProvisioningDomainProxyConfig::State::kFetching:
      return "Fetching";
    case ProvisioningDomainProxyConfig::State::kValid:
      return "Valid";
    case ProvisioningDomainProxyConfig::State::kFailedTransient:
      return "FailedTransient";
    case ProvisioningDomainProxyConfig::State::kFailedBlocked:
      return "FailedBlocked";
    case ProvisioningDomainProxyConfig::State::kFailedPermanent:
      return "FailedPermanent";
  }
}

std::optional<ProvisioningDomainProxyConfig::RoutingRule> ParseRoutingRule(
    const base::DictValue& match_dict) {
  std::vector<std::string> proxies;
  if (const base::Value* proxies_value = match_dict.Find(kProxiesKey)) {
    const base::ListValue* proxies_list = proxies_value->GetIfList();
    if (!proxies_list) {
      return std::nullopt;
    }
    for (const auto& proxy_value : *proxies_list) {
      if (proxy_value.is_string()) {
        proxies.push_back(proxy_value.GetString());
      }
    }
    if (!proxies_list->empty() && proxies.empty()) {
      return std::nullopt;
    }
  }
  // In the Provisioning Domain specification, if `proxies` is empty array or
  // omitted, go DIRECT for matching traffic.
  if (proxies.empty()) {
    proxies.push_back(kDirectProxyId);
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

  // A rule without domains and subnets is scoped only by its ports, so dropping
  // them widens it to every destination.
  const bool is_wildcard_rule = domains.empty() && subnets.empty();

#if BUILDFLAG(IS_IOS)
  StripPortMatchers(ports, is_wildcard_rule);
  WidenDomainsToSubdomains(domains);
#endif  // BUILDFLAG(IS_IOS)

  net::ProxyHostMatchingRules destination_matchers;

  if (is_wildcard_rule) {
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
      if (domain.starts_with(kWildcardSubdomainPrefix)) {
        // By PvD proxy routing standard, entries that include a wildcard prefix
        // (*.domain.com) also match the FQDN with no subdomain (domain.com)
        patterns_to_add.push_back(
            domain.substr(kWildcardSubdomainPrefix.size()));
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
      {kProfileIdCamelPlaceholder, profile_id},
      {kAcceptLanguagePlaceholder, accept_languages},
      {kAcceptLanguageCamelPlaceholder, accept_languages},
  };

  net::HttpRequestHeaders headers;
  for (const auto& header : extra_headers) {
    if (header.type == ProxyExtraHeader::HeaderType::kConstant) {
      headers.SetHeader(header.key, header.value);
      continue;
    }

    if (header.type == ProxyExtraHeader::HeaderType::kVariable) {
      std::string expanded_value = header.value;
      std::string normalized_val = base::ToLowerASCII(header.value);
      if (normalized_val == "profileid" || normalized_val == "profile_id") {
        expanded_value = profile_id;
      } else if (normalized_val == "acceptlanguage" ||
                 normalized_val == "accept_language") {
        expanded_value = accept_languages;
      } else {
        ExpandPlaceholders(&expanded_value, replacements);
        // Drop header if it contains unsupported or unrecognized variable
        // placeholders.
        if (expanded_value.find("${") != std::string::npos) {
          continue;
        }
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

  const base::ListValue* policy_extra_headers =
      domain_dict.FindList(kExtraHeadersKey);
  if (!policy_extra_headers) {
    policy_extra_headers = domain_dict.FindList(kExtraHeadersHyphenKey);
  }
  policy.extra_headers = ParseExtraHeadersList(policy_extra_headers);

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
    if (base::Time::FromUTCString(expires_str->c_str(), &expires_time) ||
        base::Time::FromString(expires_str->c_str(), &expires_time)) {
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

  // Parse proxy-match routing rules, pruning any referenced proxy IDs that were
  // unrecognized or rejected by `ParseProxy()` (while keeping explicit
  // `"DIRECT"` entries). If a rule listed proxy IDs and none of them resolved
  // to a valid endpoint, drop the rule at parse time so it cannot produce an
  // empty `net::ProxyList` (which `net::ConfiguredProxyResolutionService`
  // treats as forcing `DIRECT` instead of falling through to lower-priority
  // rules, PAC scripts, or system proxy settings).
  for (const auto& match_value : *proxy_match_list) {
    if (!match_value.is_dict()) {
      continue;
    }
    std::optional<ProvisioningDomainProxyConfig::RoutingRule> routing_rule =
        ParseRoutingRule(match_value.GetDict());
    if (!routing_rule.has_value()) {
      continue;
    }
    std::erase_if(routing_rule->proxies, [&](const std::string& proxy_id) {
      return proxy_id != kDirectProxyId &&
             !config_data.proxy_endpoints.contains(proxy_id);
    });
    if (routing_rule->proxies.empty()) {
      LOG_POLICY(WARNING, ENTERPRISE_PROXY)
          << "Ignoring proxy-match rule in PvD '" << config_data.pvd_id
          << "' because none of its referenced proxy endpoints are valid.";
      continue;
    }
    config_data.routing_rules.push_back(std::move(*routing_rule));
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
  if (!destination_url.is_valid()) {
    return nullptr;
  }

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
  // `policy::PolicyValueHash()` can't be used: it builds on `base::FastHash()`
  // and `base::HashInts()`, which are seeded per process when DCHECKs are on
  // and aren't guaranteed stable across releases. `base::DictValue` keys are
  // sorted, so the JSON form is canonical.
  std::optional<std::string> json =
      base::WriteJson(ProvisioningDomainConfigToDict(policy_config));
  CHECK(json.has_value());
  return base::NumberToString(base::PersistentHash(*json));
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
  dict.Set("state", ProvisioningDomainStateToString(proxy_config.state));
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

bool IsSecureGatewayEnabled(PrefService* prefs) {
  CHECK(prefs);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
  return IsDynamicRouteFetchingEnabled() &&
         !prefs->GetList(kProxyProvisioningDomains).empty();
#else
  return false;
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
}

std::optional<BasicAuthChallengeParams> ParseBasicAuthChallengeParams(
    std::string_view challenge) {
  std::optional<std::string_view> param_list =
      ExtractBasicAuthParamList(challenge);
  if (!param_list.has_value()) {
    return std::nullopt;
  }

  BasicAuthChallengeParams params;
  net::HttpUtil::NameValuePairsIterator iter(
      *param_list, /*delimiter=*/',',
      net::HttpUtil::NameValuePairsIterator::Values::REQUIRED,
      net::HttpUtil::NameValuePairsIterator::Quotes::STRICT_QUOTES);
  while (iter.GetNext()) {
    if (!net::HttpUtil::IsToken(iter.name()) ||
        (!iter.value_is_quoted() && !net::HttpUtil::IsToken(iter.value()))) {
      return std::nullopt;
    }
    auto [_, inserted] =
        params.try_emplace(base::ToLowerASCII(iter.name()), iter.value());
    if (!inserted) {
      return std::nullopt;
    }
  }
  if (!iter.valid() || params.empty()) {
    return std::nullopt;
  }
  return params;
}

std::optional<BasicAuthChallengeParams> ExtractMatchingBasicAuthChallengeParams(
    const net::AuthChallengeInfo& auth_info,
    const net::HttpResponseHeaders* response_headers) {
  if (!base::EqualsCaseInsensitiveASCII(auth_info.scheme,
                                        net::kBasicAuthScheme)) {
    return std::nullopt;
  }

  if (response_headers) {
    std::optional<BasicAuthChallengeParams> matched_params;
    size_t iter = 0;
    std::optional<std::string_view> header_value;
    while ((header_value = response_headers->EnumerateHeader(
                &iter, kProxyAuthenticateHeader))) {
      if (!HasBasicAuthScheme(*header_value)) {
        continue;
      }
      // Fail closed if any `Basic` challenge header is malformed under strict
      // parsing so we cannot skip past a header that `//net`'s non-strict
      // parser selected and inadvertently read `url=` from a later header.
      std::optional<BasicAuthChallengeParams> params =
          ParseBasicAuthChallengeParams(*header_value);
      if (!params.has_value()) {
        return std::nullopt;
      }
      if (!HasMatchingRealm(*params, auth_info.realm)) {
        continue;
      }
      if (matched_params.has_value()) {
        return std::nullopt;
      }
      matched_params = std::move(params);
    }
    return matched_params;
  }

  if (!auth_info.challenge.empty()) {
    std::optional<BasicAuthChallengeParams> params =
        ParseBasicAuthChallengeParams(auth_info.challenge);
    if (params.has_value() && HasMatchingRealm(*params, auth_info.realm)) {
      return params;
    }
  }
  return std::nullopt;
}

}  // namespace enterprise_net
