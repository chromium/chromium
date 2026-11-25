// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_

#include <optional>
#include <string_view>
#include <vector>

#include "components/proxy_config/proxy_config_export.h"
#include "net/base/proxy_chain.h"
#include "net/proxy_resolution/proxy_config.h"
#include "url/scheme_host_port.h"

class PrefService;
class ProxyConfigDictionary;

namespace base {
class ListValue;
class Value;
}  // namespace base

namespace net {
class ProxyConfigWithAnnotation;
struct NetworkTrafficAnnotationTag;
}  // namespace net

namespace policy {
class PolicyService;
}

namespace proxy_config {

// Constants used to parse the "ProxyOverrideRules" policy value.
inline constexpr char kKeyDestinationMatchers[] = "DestinationMatchers";
inline constexpr char kKeyExcludeDestinationMatchers[] =
    "ExcludeDestinationMatchers";
inline constexpr char kKeyProxyList[] = "ProxyList";
inline constexpr char kKeyConditions[] = "Conditions";
inline constexpr char kKeyDnsProbe[] = "DnsProbe";
inline constexpr char kKeyHost[] = "Host";
inline constexpr char kKeyResult[] = "Result";
inline constexpr char kResultResolved[] = "resolved";
inline constexpr char kResultNotFound[] = "not_found";

// Converts a string taken from the "Host" field of the "ProxyOverrideRules"
// policy to a `url::SchemeHostPort` to be used to populate
// `net::ProxyConfig::ProxyOverrideRule::DnsProbeCondition::host`.
PROXY_CONFIG_EXPORT url::SchemeHostPort ProxyOverrideRuleHostFromString(
    std::string_view raw_value);

// Converts a string taken from the "ProxyList" field of the
// "ProxyOverrideRules" policy to a `net::ProxyChain` to be used to populate
// `net::ProxyConfig::ProxyOverrideRule::proxy_list`.
PROXY_CONFIG_EXPORT net::ProxyChain ProxyOverrideRuleProxyFromString(
    std::string_view raw_value);

// Returns whether user-scoped or extension-controlled proxy override rules are
// allowed for the current user based on user affiliation and the
// "EnableProxyOverrideRulesForAllUsers" policy. Unlike
// `ProxyOverrideRulesAllowed()`, this checks whether a user or extension is
// eligible to set or control `kProxyOverrideRules` regardless of whether the
// preference currently has a value or what scope controls it.
// `pref_service` must be non-null. `policy_service` is used to check user
// affiliation and may be null if affiliation is not applicable to the caller
// (in which case the user is treated as unaffiliated). Neither pointer is
// retained.
PROXY_CONFIG_EXPORT bool AreUserProxyOverrideRulesAllowed(
    const PrefService* pref_service,
    policy::PolicyService* policy_service);

// Returns true if `kProxyOverrideRules` is currently controlled by an extension
// or by a user-scoped policy (i.e. sources subject to user affiliation checks
// on desktop and Android). `pref_service` must be non-null.
PROXY_CONFIG_EXPORT bool IsUserOrExtensionProxyOverrideRules(
    const PrefService* pref_service);

// Returns whether the currently configured `kProxyOverrideRules` preference
// value should be applied to `ProxyConfig::ProxyOverrideRule`s. Unlike
// `AreUserProxyOverrideRulesAllowed()`, this inspects the active preference's
// source and scope (allowing unset or machine-scoped rules even for
// unaffiliated users) and delegates to `AreUserProxyOverrideRulesAllowed()`
// when the preference is user-scoped or extension-controlled.
// `pref_service` must be non-null. `policy_service` may be null if user
// affiliation is not applicable to the caller (in which case the user is
// treated as unaffiliated). Neither pointer is retained.
PROXY_CONFIG_EXPORT bool ProxyOverrideRulesAllowed(
    const PrefService* pref_service,
    policy::PolicyService* policy_service);

// Parses a single dictionary entry from the "ProxyOverrideRules" preference
// list into a `net::ProxyConfig::ProxyOverrideRule`, or returns `std::nullopt`
// if the entry is malformed or contains no valid proxy chains.
PROXY_CONFIG_EXPORT std::optional<net::ProxyConfig::ProxyOverrideRule>
ValueToOverrideRule(const base::Value& value);

// Parses a "ProxyOverrideRules" preference list into valid
// `net::ProxyConfig::ProxyOverrideRule` entries, skipping invalid entries.
PROXY_CONFIG_EXPORT std::vector<net::ProxyConfig::ProxyOverrideRule>
ParseProxyOverrideRules(const base::ListValue& rules_list);

// Converts a `ProxyConfigDictionary` to a `net::ProxyConfigWithAnnotation`
// representation using `traffic_annotation` when non-direct settings are
// populated. Returns true if the dictionary specifies an explicit proxy mode
// (including `MODE_DIRECT`), or false if Chrome should fall back to system
// proxy settings (`MODE_SYSTEM` or missing/invalid mode).
PROXY_CONFIG_EXPORT bool PrefConfigToNetConfig(
    const ProxyConfigDictionary& proxy_dict,
    const net::NetworkTrafficAnnotationTag& traffic_annotation,
    net::ProxyConfigWithAnnotation* config);

}  // namespace proxy_config

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
