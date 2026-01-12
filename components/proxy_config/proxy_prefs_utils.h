// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_

#include "components/proxy_config/proxy_config_export.h"
#include "net/base/proxy_chain.h"
#include "url/scheme_host_port.h"

class PrefService;

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

// Returns false if the proxy override prefs should not be converted into
// `ProxyConfig::ProxyOverrideRule`s. This is the case when they are set from an
// unaffiliated user and the "EnableProxyOverrideRulesForAllUsers" is not set to
// allow it.
PROXY_CONFIG_EXPORT bool ProxyOverrideRulesAllowed(
    const PrefService* pref_service);

}  // namespace proxy_config

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_PREFS_UTILS_H_
