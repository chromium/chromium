// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PROXY_CONFIG_PROXY_PREFS_H_
#define COMPONENTS_PROXY_CONFIG_PROXY_PREFS_H_

#include <string>

#include "components/proxy_config/proxy_config_export.h"

namespace ProxyPrefs {

// Possible types of specifying proxy settings. Do not change the order of
// the constants, because numeric values are exposed to users.
// If you add an enum constant, you should also add a string to
// kProxyModeNames in the .cc file.
enum ProxyMode {
  // Direct connection to the network, other proxy preferences are ignored.
  MODE_DIRECT = 0,

  // Try to auto-detect the PAC script location.
  // On Windows and Chrome OS, DHCP is tried first (DHCP Option 252), and DNS
  // (resolving http://wpad/wpad.dat) is tried second.
  // On other platforms, only DNS is tried.
  // If no PAC script can be found by this method, fall back to direct
  // connection.
  MODE_AUTO_DETECT = 1,

  // Try to retrieve a PAC script from kProxyPacURL or fall back to direct
  // connection.
  MODE_PAC_SCRIPT = 2,

  // Use the settings specified in kProxyServer and kProxyBypassList.
  MODE_FIXED_SERVERS = 3,

  // The system's proxy settings are used, other proxy preferences are
  // ignored.
  MODE_SYSTEM = 4,

  kModeCount
};

// State of proxy configuration.
enum ConfigState {
  // Configuration is from policy.
  CONFIG_POLICY,
  // Configuration is from extension.
  CONFIG_EXTENSION,
  // Configuration is not from policy or extension but still precedes others.
  CONFIG_OTHER_PRECEDE,
  // Configuration is from system.
  CONFIG_SYSTEM,
  // Configuration is recommended i.e there's a fallback configuration.
  CONFIG_FALLBACK,
  // Configuration is known to be not set.
  CONFIG_UNSET,
};

// Constants for string values used to specify the proxy mode through externally
// visible APIs, e.g. through policy or the proxy extension API.
PROXY_CONFIG_EXPORT extern const char kDirectProxyModeName[];
PROXY_CONFIG_EXPORT extern const char kAutoDetectProxyModeName[];
PROXY_CONFIG_EXPORT extern const char kPacScriptProxyModeName[];
PROXY_CONFIG_EXPORT extern const char kFixedServersProxyModeName[];
PROXY_CONFIG_EXPORT extern const char kSystemProxyModeName[];

PROXY_CONFIG_EXPORT bool IntToProxyMode(int in_value, ProxyMode* out_value);
PROXY_CONFIG_EXPORT bool StringToProxyMode(const std::string& in_value,
                                           ProxyMode* out_value);
// Ownership of the return value is NOT passed to the caller.
PROXY_CONFIG_EXPORT const char* ProxyModeToString(ProxyMode mode);
PROXY_CONFIG_EXPORT std::string ConfigStateToDebugString(ConfigState state);

}  // namespace ProxyPrefs

#endif  // COMPONENTS_PROXY_CONFIG_PROXY_PREFS_H_
