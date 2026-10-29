// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_

#include "components/proxy_config/proxy_config_export.h"
#include "net/base/proxy_chain.h"
#include "url/scheme_host_port.h"

class PrefService;

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

}  // namespace proxy_config

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
