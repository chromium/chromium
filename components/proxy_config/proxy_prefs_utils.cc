// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/proxy_config/proxy_prefs_utils.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/net/core/proxy_policy_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_service.h"
#include "components/policy/core/common/policy_types.h"
#include "components/prefs/pref_service.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "components/proxy_config/proxy_config_pref_names.h"
#include "components/proxy_config/proxy_prefs.h"
#include "net/base/proxy_server.h"
#include "net/base/proxy_string_util.h"
#include "net/base/url_util.h"
#include "net/net_buildflags.h"
#include "net/proxy_resolution/proxy_config_with_annotation.h"
#include "net/proxy_resolution/proxy_host_matching_rules.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "url/gurl.h"
#include "url/scheme_host_port.h"
#include "url/url_constants.h"

namespace proxy_config {

url::SchemeHostPort ProxyOverrideRuleHostFromString(
    std::string_view raw_value) {
  GURL url(raw_value);
  std::string scheme = url.GetScheme();
  std::string host = url.GetHost();
  int port = url.IntPort();

  // If the value used to initialize `url` is missing parts (for example because
  // it's only a hostname), then try to specify a replacement scheme and/or
  // port. This gives flexibility to the policy field to only specify certain
  // values.
  if (host.empty()) {
    // This path is reached when only the host is provided (ex. "foo.com").
    // In this case, `url` will be invalid and see "foo.com" as a scheme, so if
    // that happens the string is instead parsed as a host:port pair, with HTTP
    // as the default scheme.
    scheme = url::kHttpScheme;
    if (!net::ParseHostAndPort(raw_value, &host, &port)) {
      return url::SchemeHostPort();
    }
  }
  if (scheme.empty()) {
    scheme = url::kHttpScheme;
  }
  if (port == url::PORT_UNSPECIFIED) {
    port = url::DefaultPortForScheme(scheme);
  }

  return url::SchemeHostPort(scheme, host, port);
}

net::ProxyChain ProxyOverrideRuleProxyFromString(std::string_view raw_value) {
  GURL url(raw_value);
  if (url.is_valid()) {
    net::ProxyServer::Scheme scheme = net::GetSchemeFromUriScheme(url.scheme());
    if (scheme == net::ProxyServer::SCHEME_INVALID) {
      return net::ProxyChain();
    }
    return net::ProxyChain::FromSchemeHostAndPort(scheme, url.host(),
                                                  url.port());
  }
  return net::PacResultElementToProxyChain(raw_value);
}

bool AreUserProxyOverrideRulesAllowed(policy::PolicyService* policy_service) {
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  if (policy_service) {
    const policy::PolicyMap& policies = policy_service->GetPolicies(
        policy::PolicyNamespace(policy::POLICY_DOMAIN_CHROME, std::string()));
    if (policies.IsUserAffiliated() ||
        !enterprise_net::IsDeviceManaged(policies) ||
        enterprise_net::IsUnaffiliatedUserProxyPolicyAllowed(policies)) {
      return true;
    }
  }

  return false;
#else   // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  return true;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

bool IsUserOrExtensionProxyOverrideRules(const PrefService* pref_service) {
  CHECK(pref_service);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kProxyOverrideRules);
  CHECK(pref);

  return pref->IsExtensionControlled() ||
         (pref->IsManaged() &&
          pref_service->GetInteger(prefs::kProxyOverrideRulesScope) ==
              policy::POLICY_SCOPE_USER);
#else   // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  return false;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

bool ProxyOverrideRulesAllowed(const PrefService* pref_service,
                               policy::PolicyService* policy_service) {
  CHECK(pref_service);
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID)
  return AreUserProxyOverrideRulesAllowed(policy_service) ||
         !IsUserOrExtensionProxyOverrideRules(pref_service);
#else   // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
  return true;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) ||
        // BUILDFLAG(IS_ANDROID)
}

namespace {

std::optional<net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition>
ValueToDnsCondition(const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  // Expected schema:
  // {
  //   "DnsProbe": {
  //     "Host": "corp.ads",
  //     "Result": "resolved", // or "not_found"
  //   }
  // }
  // For now "DnsProbe" is always expected, but eventually other types of
  // conditions will be possible.
  const base::DictValue& dict = value.GetDict();
  auto* dns_probe_value = dict.FindDict(kKeyDnsProbe);
  if (!dns_probe_value) {
    return std::nullopt;
  }

  const std::string* host_value = dns_probe_value->FindString(kKeyHost);
  const std::string* result_value = dns_probe_value->FindString(kKeyResult);
  if (!host_value || !result_value ||
      (*result_value != kResultResolved && *result_value != kResultNotFound)) {
    return std::nullopt;
  }

  net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition dns_probe_condition;
  dns_probe_condition.host = ProxyOverrideRuleHostFromString(*host_value);
  if (!dns_probe_condition.host.IsValid()) {
    return std::nullopt;
  }

  dns_probe_condition.result =
      *result_value == kResultResolved
          ? net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kResolved
          : net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::kNotFound;
  return dns_probe_condition;
}

// Generic parser for a list of URL patterns to populate into the passed
// `rules`. Returns false if an unexpected value was found in the passed
// `value`. `optional_field` indicates if the matcher list is required to be
// present in the rule or not, and will be returned directly if `value` doesn't
// have an entry for `key`.
//
// Implicit rules are not applied to URL pattern lists of the
// "ProxyOverrideRules" policy, so on a valid `value` there is always an extra
// rule added to subtract them from the matcher evaluation.
bool AddUrlMatcher(const base::DictValue& value,
                   net::ProxyHostMatchingRules& rules,
                   const std::string& key,
                   bool optional_field) {
  // Expected schema:
  // {
  //   ...
  //   "<key>": [ "https://app1.com", "https://app2.com" ],
  //   ...
  // }
  auto* matchers_value = value.FindList(key);
  if (!matchers_value) {
    rules.AddRulesToSubtractImplicit();
    return optional_field;
  }

  for (const auto& matcher : *matchers_value) {
    if (matcher.is_string()) {
      rules.AddRuleFromString(matcher.GetString());
    } else {
      return false;
    }
  }

  rules.AddRulesToSubtractImplicit();
  return true;
}

// Returns false if an unexpected value was found in the passed `value`.
bool AddDestinationMatchers(const base::DictValue& value,
                            net::ProxyConfig::ProxyOverrideRule& rule) {
  return AddUrlMatcher(value, rule.destination_matchers,
                       kKeyDestinationMatchers,
                       /*optional_field=*/false);
}

// Returns false if an unexpected value was found in the passed `value`.
bool AddExcludeDestinationMatchers(const base::DictValue& value,
                                   net::ProxyConfig::ProxyOverrideRule& rule) {
  return AddUrlMatcher(value, rule.exclude_destination_matchers,
                       kKeyExcludeDestinationMatchers,
                       /*optional_field=*/true);
}

// Returns false if an unexpected value was found in the passed `value`, or if
// the "ProxyList" key is missing.
bool AddProxyChain(const base::DictValue& value,
                   net::ProxyConfig::ProxyOverrideRule& rule) {
  // Expected schema:
  // {
  //   ...
  //   "ProxyList": [ "HTTPS proxy.app:443", "DIRECT" ],
  //   ...
  // }
  //
  // The entries of the list can have the PAC format as above, or a regular URL
  // format of "scheme://host:port".
  auto* proxy_list_value = value.FindList(kKeyProxyList);
  if (!proxy_list_value) {
    return false;
  }

  // Invalid entries don't return false since it's possible the received policy
  // is meant for a future version of Chrome, so they are just discarded.
  for (const auto& entry : *proxy_list_value) {
    if (!entry.is_string()) {
      return false;
    }

    net::ProxyChain chain = ProxyOverrideRuleProxyFromString(entry.GetString());
    if (chain.IsValid()) {
      rule.proxy_list.AddProxyChain(std::move(chain));
    }
  }

  // If none of the entries of `proxy_list_value` were valid, then the overall
  // rule is not considered valid as it cannot be applied even if other fields
  // are present.
  return !rule.proxy_list.IsEmpty();
}

// Returns false if an unexpected value was found in the passed `value`.
bool AddConditions(const base::DictValue& value,
                   net::ProxyConfig::ProxyOverrideRule& rule) {
  // Expected schema:
  // {
  //   ...
  //   "Conditions": [
  //        {
  //            "DnsProbe": {
  //                "Host": "corp.ads",
  //                "Result": "resolved"
  //            }
  //        }
  //   ]
  //   ...
  // }
  auto* conditions_value = value.FindList(kKeyConditions);
  if (!conditions_value) {
    // This field is optional, so it being missing isn't considered an error.
    return true;
  }

  for (const auto& condition_value : *conditions_value) {
    auto condition = ValueToDnsCondition(condition_value);
    if (condition) {
      rule.dns_conditions.push_back(std::move(*condition));
    } else {
      return false;
    }
  }

  return true;
}

}  // namespace

std::optional<net::ProxyConfig::ProxyOverrideRule> ValueToOverrideRule(
    const base::Value& value) {
  if (!value.is_dict()) {
    return std::nullopt;
  }

  // Expected schema:
  // {
  //   "DestinationMatchers": [ "https://app1.com", "https://app2.com" ],
  //   "ExcludeDestinationMatchers: ["https://exception.com"],
  //   "ProxyList": [ "HTTPS proxy.app:443", "DIRECT" ],
  //   "Conditions": [
  //        {
  //            "DnsProbe": {
  //                "Host": "corp.ads",
  //                "Result": "resolved"
  //            }
  //        }
  //   ]
  // }
  const base::DictValue& dict = value.GetDict();
  net::ProxyConfig::ProxyOverrideRule rule;

  if (!AddDestinationMatchers(dict, rule) ||
      !AddExcludeDestinationMatchers(dict, rule) ||
      !AddProxyChain(dict, rule) || !AddConditions(dict, rule)) {
    return std::nullopt;
  }

  return rule;
}

std::vector<net::ProxyConfig::ProxyOverrideRule> ParseProxyOverrideRules(
    const base::ListValue& rules_list) {
  std::vector<net::ProxyConfig::ProxyOverrideRule> proxy_override_rules;
  for (const auto& value : rules_list) {
    auto rule = ValueToOverrideRule(value);
    if (rule) {
      proxy_override_rules.push_back(std::move(*rule));
    }
  }
  return proxy_override_rules;
}

bool PrefConfigToNetConfig(
    const ProxyConfigDictionary& proxy_dict,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    net::ProxyConfigWithAnnotation* config) {
  ProxyPrefs::ProxyMode mode;
  if (!proxy_dict.GetMode(&mode)) {
    // Fall back to system settings if the mode preference is invalid.
    return false;
  }
  net::ProxyConfig proxy_config = config->value();
  switch (mode) {
    case ProxyPrefs::MODE_SYSTEM:
      // Use system settings.
      return false;
    case ProxyPrefs::MODE_DIRECT:
      // Ignore all the other proxy config preferences if the use of a proxy
      // has been explicitly disabled.
      return true;
    case ProxyPrefs::MODE_AUTO_DETECT:
      proxy_config.set_auto_detect(true);
      *config =
          net::ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
      return true;
    case ProxyPrefs::MODE_PAC_SCRIPT: {
      std::string proxy_pac;
      if (!proxy_dict.GetPacUrl(&proxy_pac)) {
        LOG(ERROR) << "Proxy settings request PAC script but do not specify "
                   << "its URL. Falling back to direct connection.";
        return true;
      }
      GURL proxy_pac_url(proxy_pac);
      if (!proxy_pac_url.is_valid()) {
        LOG(ERROR) << "Invalid proxy PAC url: " << proxy_pac;
        return true;
      }
      proxy_config.set_pac_url(proxy_pac_url);
      bool pac_mandatory = false;
      proxy_dict.GetPacMandatory(&pac_mandatory);
      proxy_config.set_pac_mandatory(pac_mandatory);
      *config =
          net::ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
      return true;
    }
    case ProxyPrefs::MODE_FIXED_SERVERS: {
      std::string proxy_server;
      if (!proxy_dict.GetProxyServer(&proxy_server)) {
        LOG(ERROR) << "Proxy settings request fixed proxy servers but do not "
                   << "specify their URLs. Falling back to direct connection.";
        return true;
      }

      bool allow_bracketed_proxy_chains = false;
      bool allow_quic_proxy_support = false;

#if BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
      allow_bracketed_proxy_chains = true;
#endif  // BUILDFLAG(ENABLE_BRACKETED_PROXY_URIS)
#if BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)
      allow_quic_proxy_support = true;
#endif  // BUILDFLAG(ENABLE_QUIC_PROXY_SUPPORT)

      proxy_config.proxy_rules().ParseFromString(
          proxy_server, allow_bracketed_proxy_chains, allow_quic_proxy_support);

      std::string proxy_bypass;
      if (proxy_dict.GetBypassList(&proxy_bypass)) {
        proxy_config.proxy_rules().bypass_rules.ParseFromString(proxy_bypass);
      }
      *config =
          net::ProxyConfigWithAnnotation(proxy_config, traffic_annotation);
      return true;
    }
    case ProxyPrefs::kModeCount: {
      // Fall through to NOTREACHED().
    }
  }
  NOTREACHED() << "Unknown proxy mode, falling back to system settings.";
}

}  // namespace proxy_config
