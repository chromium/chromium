// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_

#include "build/build_config.h"

namespace proxy_config::prefs {

// Preference to store proxy settings.
inline constexpr char kProxy[] = "proxy";

// A boolean pref that controls whether proxy settings from shared network
// settings (accordingly from device policy) are applied or ignored.
inline constexpr char kUseSharedProxies[] = "settings.use_shared_proxies";

// Preference to store the value of the "ProxyOverrideRules" policy.
inline constexpr char kProxyOverrideRules[] = "proxy_override_rules";

#if !BUILDFLAG(IS_CHROMEOS)
// Preference to store the scope (user vs machine) corresponding to the value
// set in `kProxyOverrideRules`. This is used to handle the policy differently
// when its source is a cloud user depending on its affiliation status and the
// value of the "EnableProxyOverrideRulesForAllUsers" policy. On CrOS, this is
// not used as there isn't a way for the admin to set non-user cloud policies.
inline constexpr char kProxyOverrideRulesScope[] = "proxy_override_rules_scope";
#endif  // !BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Preference to store the value of the "EnableProxyOverrideRulesForAllUsers"
// policy.
inline constexpr char kEnableProxyOverrideRulesForAllUsers[] =
    "enable_proxy_override_rules_for_users";
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

}  // namespace proxy_config::prefs

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_CONFIG_PREF_NAMES_H_
